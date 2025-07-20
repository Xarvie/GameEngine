#pragma once

#include "EntityComponents.h"
#include "glad/glad.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <unordered_map>
#include <vector>

// =========================================================================
// GPU骨骼纹理管理器 - 批量管理多角色蒙皮矩阵上传
// =========================================================================

/**
 * 高性能GPU骨骼纹理管理器
 * 职责：将所有可见角色的蒙皮矩阵批量打包并上传到单一4K纹理
 */
class BoneTextureManager {
public:
    static BoneTextureManager& getInstance() {
        static BoneTextureManager instance;
        return instance;
    }

    /**
     * 骨架分配信息
     */
    struct SkeletonAllocation {
        uint32_t boneOffset;      // 在纹理中的起始骨骼索引
        uint32_t boneCount;       // 分配的骨骼数量
        bool allocated = false;   // 是否已分配
    };

    /**
     * 初始化纹理管理器
     */
    bool initialize();

    /**
     * 为骨架分配纹理空间
     * @param handle 骨架句柄
     * @param boneCount 需要的骨骼数量
     * @return 是否分配成功
     */
    bool allocateSkeleton(SkeletonHandle handle, uint32_t boneCount);

    /**
     * 更新骨架的蒙皮矩阵（仅更新缓存）
     * @param handle 骨架句柄
     * @param matrices 蒙皮矩阵数组
     */
    void updateSkeletonMatrices(SkeletonHandle handle, const std::vector<glm::mat4>& matrices);

    /**
     * 批量提交所有更新到GPU
     */
    void commitToGPU();

    /**
     * 获取骨架的偏移量（用于着色器uniform）
     * @param handle 骨架句柄
     * @return 偏移量，如果未分配返回-1
     */
    int getSkeletonOffset(SkeletonHandle handle) const;

    /**
     * 获取纹理容量信息
     */
    uint32_t getMaxBones() const { return maxBones; }
    uint32_t getUsedBones() const { return usedBones; }

    /**
     * 获取GPU纹理ID（用于渲染绑定）
     * 这个方法被RenderDevice::getBoneTexture()调用
     */
    GLuint getBoneTextureID() const { return boneTexture; }

    /**
     * 清理资源
     */
    void cleanup();

private:
    BoneTextureManager() = default;
    ~BoneTextureManager() { cleanup(); }

    // 纹理配置 - 针对GLES 3.0优化
    static constexpr uint32_t TEXTURE_WIDTH = 4;      // 每个矩阵需要4列
    static constexpr uint32_t TEXTURE_HEIGHT = 1024;  // 支持1024个骨骼矩阵
    static constexpr uint32_t MAX_BONES = TEXTURE_HEIGHT;

    // GPU资源
    GLuint boneTexture = 0;
    bool isInitialized = false;

    // 分配管理
    std::unordered_map<uint32_t, SkeletonAllocation> allocations;  // handle.id -> allocation
    std::vector<glm::mat4> boneMatricesCache;  // CPU侧缓存
    std::vector<bool> dirtyRegions;            // 标记哪些区域需要更新

    uint32_t usedBones = 0;  // 已使用的骨骼数量
    uint32_t maxBones = MAX_BONES;

    bool needsGPUUpdate = false;

    // 内部方法
    void createBoneTexture();
    void uploadRegion(uint32_t startBone, uint32_t boneCount);
};




#include "ISystem.h"
#include "EntityComponents.h"
#include <unordered_map> // 新增：用于追踪任务状态

// 动画任务输入数据
struct AnimationTaskInput {
    SkeletonHandle skeleton;   // 保持使用完整Handle
    AnimationHandle animation; // 保持使用完整Handle
    float currentTime;
    float speed;
    float weight;
    bool looping;
    uint64_t taskId;

    AnimationTaskInput() = default;
    AnimationTaskInput(SkeletonHandle skel, AnimationHandle anim, float time,
                       float sp, float w, bool loop, uint64_t id)
            : skeleton(skel), animation(anim), currentTime(time), speed(sp),
              weight(w), looping(loop), taskId(id) {}
};

// 动画任务输出数据
struct AnimationTaskOutput {
    SkeletonHandle skeleton;   // 保持使用完整Handle
    std::vector<ozz::math::SoaTransform> localSoaTransforms;
    uint64_t taskId;
    bool success;

    AnimationTaskOutput() : success(false) {}
    AnimationTaskOutput(SkeletonHandle skel,  std::vector<ozz::math::SoaTransform>&& locals,
                        uint64_t id, bool succ = true)
            : skeleton(skel), localSoaTransforms(std::move(locals)), taskId(id), success(succ) {}
};

// 异步任务系统 - 使用PIMPL隐藏所有线程实现
class TaskSystem {
public:
    static TaskSystem& getInstance();

    // 初始化系统，传入资产引用
    bool initialize(const spartan::asset::ProcessedAsset& asset);

    // 提交动画计算任务
    uint64_t submitAnimationTask(const AnimationTaskInput& input);

    // 尝试获取计算结果（非阻塞）
    bool tryPopAnimationResult(AnimationTaskOutput& output);

    // 等待所有待处理任务完成（仅用于shutdown）
    void waitForAllPendingTasks();

    // 清理系统
    void shutdown();

    // 获取统计信息
    size_t getPendingTaskCount() const;
    size_t getCompletedTaskCount() const;

private:
    TaskSystem() = default;
    ~TaskSystem() = default;

    // PIMPL - 隐藏所有线程实现细节
    struct Impl;
    std::unique_ptr<Impl> pImpl;

    // 下一个任务ID
    uint64_t nextTaskId = 1;
};