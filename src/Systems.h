#pragma once

#include "ISystem.h"
#include "EntityComponents.h"
#include "AnimationTask.h"
#include <unordered_map>

// =========================================================================
// 系统实现类 - 继承自接口的具体实现
// =========================================================================

/**
 * 变换继承系统实现
 */
class TransformSystem : public ITransformSystem {
private:
    struct Cache {
        std::vector<entt::entity> sortedEntities;
        std::vector<uint32_t> depths;
        bool valid = false;
    };
    Cache cache;

public:
    // ISystem 接口实现
    void update(entt::registry& registry, float deltaTime) override;
    void initialize(entt::registry& registry) override;
    const char* getName() const override { return "TransformSystem"; }
    int getPriority() const override { return 10; } // 变换系统优先级最高

    // ITransformSystem 接口实现
    void markDirty(entt::registry& registry, entt::entity entity) override;
    void invalidateCache() override;
    void printStats() const override;

private:
    void buildSortedList(entt::registry& registry);
    uint32_t calculateDepth(entt::registry& registry, entt::entity entity);
    void buildChildrenLists(entt::registry& registry);
    void updateTransforms(entt::registry& registry);
    bool needsUpdate(entt::registry& registry, entt::entity entity, const LocalTransform& localTransform);
    void updateEntityTransform(entt::registry& registry, entt::entity entity, Transform& transform, LocalTransform& localTransform);
    void markChildrenDirty(entt::registry& registry, entt::entity entity);
};

/**
 * 多轨骨骼动画系统实现
 */
/**
 * 多轨骨骼动画系统实现 - 异步计算版本
 */
class AnimationSystem : public IAnimationSystem {
public:
    // ISystem 接口实现
    void initialize(entt::registry& registry) override;
    void update(entt::registry& registry, float deltaTime) override;
    void cleanup(entt::registry& registry) override;
    const char* getName() const override { return "AnimationSystem"; }
    int getPriority() const override { return 20; } // 在变换系统之后

    // IAnimationSystem 接口实现
    void forceRefresh(entt::registry& registry) override;
    void setGlobalPlayState(bool playing) override;
    void printAnimationStats() const override;

private:
    bool globalPlayState = true;
    mutable int activeAnimationCount = 0;
    mutable int totalSkeletons = 0;

    // 异步任务系统引用
    TaskSystem& taskSystem = TaskSystem::getInstance();

    // 追踪每个skeleton的异步任务状态
    /**
 * @brief 用于追踪每个骨架异步动画任务的状态
 */
    struct SkeletonTaskState {
        /**
         * @brief 当前正在为这个骨架计算的异步任务ID。
         * 如果为0，表示当前没有正在进行的任务。
         */
        uint64_t pendingTaskId = 0;

        /**
         * @brief 标记在当前帧是否收到了一个“新”的计算结果。
         * 在应用完结果后，我们会将它置为false，防止在下一帧没有新结果时被错误地重复应用。
         */
        bool hasNewResult = false;

        /**
         * @brief 缓存的最新动画结果。
         * 这里存储的是由TaskSystem计算返回的、最原始的“局部空间变换”数据。
         */
        std::vector<ozz::math::SoaTransform> cachedLocalTransforms;

        float lastSubmittedTime = 0.0f;
    };
    std::unordered_map<uint32_t, SkeletonTaskState> skeletonTasks;  // 使用handle.id作为key

    // 异步动画处理方法
    void dispatchAnimationTasks(entt::registry& registry, float deltaTime);
    void applyAnimationResults(entt::registry& registry);
    void submitSkeletonTask(entt::registry& registry, entt::entity entity,
                            SkeletonComponent& skeleton,
                            const MultiTrackAnimationComponent& multiTrack);

    // 原有的辅助方法（用于fallback和多轨道混合）
    void SoaToAos(ozz::span<const ozz::math::SoaTransform> _soa, ozz::span<ozz::math::Transform> _aos);
    void applyTrackToSkeleton(const AnimationTrack& track, float normalizedWeight,
                              const std::vector<ozz::math::SoaTransform>& trackTransforms,
                              std::vector<ozz::math::SoaTransform>& finalTransforms);
    void blendSoaTransforms(const std::vector<ozz::math::SoaTransform>& from,
                            const std::vector<ozz::math::SoaTransform>& to,
                            float ratio,
                            std::vector<ozz::math::SoaTransform>& result);
    void additiveSoaTransforms(const std::vector<ozz::math::SoaTransform>& base,
                               const std::vector<ozz::math::SoaTransform>& additive,
                               float weight,
                               std::vector<ozz::math::SoaTransform>& result);
    void updateSkinningMatrices(SkeletonComponent& skeleton);
};

/**
 * 输入系统实现
 */
class InputSystem : public IInputSystem {
public:
    // ISystem 接口实现
    void update(entt::registry& registry, float deltaTime) override;
    const char* getName() const override { return "InputSystem"; }
    int getPriority() const override { return 30; }

    // IInputSystem 接口实现
    void processEvent(SDL_Event& event, entt::registry& registry, bool& running) override;

private:
    void updateMouseState(InputStateComponent::MouseState& mouse);
    void updateKeyboardState(InputStateComponent::KeyboardState& keyboard);
    void handleMouseInput(entt::registry& registry, const InputStateComponent& input);
    void handleKeyboardInput(entt::registry& registry, const InputStateComponent& input);
    void handleDiscreteInput(entt::registry& registry, const InputStateComponent& input);
    const char* getPatternName(InstanceController::Pattern pattern);
};

/**
 * 相机系统实现
 */
class CameraSystem : public ICameraSystem {
public:
    // ISystem 接口实现
    void update(entt::registry& registry, float deltaTime) override;
    const char* getName() const override { return "CameraSystem"; }
    int getPriority() const override { return 40; }

    // ICameraSystem 接口实现
    glm::mat4 getViewMatrix(entt::registry& registry) const override;
    glm::mat4 getProjectionMatrix(entt::registry& registry) const override;

private:
    mutable glm::mat4 cachedViewMatrix = glm::mat4(1.0f);
    mutable glm::mat4 cachedProjectionMatrix = glm::mat4(1.0f);
    mutable bool viewMatrixDirty = true;
    mutable bool projectionMatrixDirty = true;
};

/**
 * 实例化渲染系统实现
 */
class InstancedRenderSystem : public IInstancedRenderSystem {
public:
    // ISystem 接口实现
    void update(entt::registry& registry, float deltaTime) override;
    const char* getName() const override { return "InstancedRenderSystem"; }
    int getPriority() const override { return 50; }

    // IInstancedRenderSystem 接口实现
    void forceUpdateInstances(entt::registry& registry) override;

private:
    void updateInstanceBuffer(InstancedMeshComponent& instancedMesh);
};

/**
 * 实例控制系统实现
 */
class InstanceControlSystem : public ISystem {
public:
    // ISystem 接口实现
    void update(entt::registry& registry, float deltaTime) override;
    const char* getName() const override { return "InstanceControlSystem"; }
    int getPriority() const override { return 60; }

private:
    void updateInstancePositions(entt::registry& registry, InstanceController& controller);
    std::vector<glm::vec3> generatePositions(const InstanceController& controller);
    std::vector<glm::vec3> generateLinePositions(const InstanceController& controller);
    std::vector<glm::vec3> generateCirclePositions(const InstanceController& controller);
    std::vector<glm::vec3> generateGridPositions(const InstanceController& controller);
    std::vector<glm::vec3> generateRandomPositions(const InstanceController& controller);
    entt::entity createInstanceSource(entt::registry& registry, MeshHandle meshHandle, const glm::vec3& position);
};

/**
 * 简单光照管理器实现 - 预留扩展
 */
class LightManager : public ILightManager {
public:
    // ISystem 接口实现
    void update(entt::registry& registry, float deltaTime) override;
    const char* getName() const override { return "LightManager"; }
    int getPriority() const override { return 70; }

    // ILightManager 接口实现
    void addDirectionalLight(const glm::vec3& direction, const glm::vec3& color, float intensity) override;
    glm::vec3 getMainLightDirection() const override;

private:
    struct DirectionalLight {
        glm::vec3 direction = glm::vec3(-0.5f, -1.0f, -0.5f);
        glm::vec3 color = glm::vec3(1.0f);
        float intensity = 1.0f;
    };

    std::vector<DirectionalLight> directionalLights;
};