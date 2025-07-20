#include "AnimationTask.h"
#include "RenderDevice.h"
#include <iostream>
#include <cstring>

// =========================================================================
// BoneTextureManager 完整实现
// =========================================================================

bool BoneTextureManager::initialize() {
    if (isInitialized) {
        return true;
    }

    std::cout << "初始化GPU骨骼纹理管理器..." << std::endl;

    // 初始化缓存
    boneMatricesCache.resize(maxBones, glm::mat4(1.0f));
    dirtyRegions.resize(maxBones, false);

    // 创建GPU纹理
    createBoneTexture();

    isInitialized = true;
    std::cout << "骨骼纹理管理器初始化完成，支持最多 " << maxBones << " 个骨骼矩阵" << std::endl;
    return true;
}

bool BoneTextureManager::allocateSkeleton(SkeletonHandle handle, uint32_t boneCount) {
    if (!isInitialized) {
        std::cerr << "错误：BoneTextureManager未初始化" << std::endl;
        return false;
    }

    // 检查是否已经分配过
    auto it = allocations.find(handle.id);
    if (it != allocations.end() && it->second.allocated) {
        if (it->second.boneCount >= boneCount) {
            // 已有分配足够大，直接使用
            return true;
        } else {
            // 需要重新分配更大空间，先释放旧的
            usedBones -= it->second.boneCount;
            it->second.allocated = false;
        }
    }

    // 检查剩余空间
    if (usedBones + boneCount > maxBones) {
        std::cerr << "错误：骨骼纹理空间不足，需要 " << boneCount
                  << " 个骨骼，剩余 " << (maxBones - usedBones) << " 个" << std::endl;
        return false;
    }

    // 分配新空间
    SkeletonAllocation allocation;
    allocation.boneOffset = usedBones;
    allocation.boneCount = boneCount;
    allocation.allocated = true;

    allocations[handle.id] = allocation;
    usedBones += boneCount;

    std::cout << "为骨架 " << handle.id << " 分配了 " << boneCount
              << " 个骨骼，偏移: " << allocation.boneOffset << std::endl;

    return true;
}

void BoneTextureManager::updateSkeletonMatrices(SkeletonHandle handle, const std::vector<glm::mat4>& matrices) {
    auto it = allocations.find(handle.id);
    if (it == allocations.end() || !it->second.allocated) {
        // 自动分配空间
        if (!allocateSkeleton(handle, static_cast<uint32_t>(matrices.size()))) {
            return;
        }
        it = allocations.find(handle.id);
    }

    const auto& allocation = it->second;
    uint32_t copyCount = std::min(static_cast<uint32_t>(matrices.size()), allocation.boneCount);

    // 更新CPU缓存
    for (uint32_t i = 0; i < copyCount; ++i) {
        uint32_t cacheIndex = allocation.boneOffset + i;
        boneMatricesCache[cacheIndex] = matrices[i];
        dirtyRegions[cacheIndex] = true;
    }

    needsGPUUpdate = true;
}

void BoneTextureManager::commitToGPU() {
    if (!isInitialized || !needsGPUUpdate) {
        return;
    }

    // 找到所有脏区域并批量上传
    uint32_t startRegion = 0;
    bool inDirtyRegion = false;

    for (uint32_t i = 0; i <= usedBones; ++i) {
        bool isDirty = (i < usedBones) ? dirtyRegions[i] : false;

        if (!inDirtyRegion && isDirty) {
            // 开始新的脏区域
            startRegion = i;
            inDirtyRegion = true;
        } else if (inDirtyRegion && (!isDirty || i == usedBones)) {
            // 结束当前脏区域，上传数据
            uint32_t regionSize = i - startRegion;
            uploadRegion(startRegion, regionSize);
            inDirtyRegion = false;
        }
    }

    // 清除脏标记
    std::fill(dirtyRegions.begin(), dirtyRegions.begin() + usedBones, false);
    needsGPUUpdate = false;
}

int BoneTextureManager::getSkeletonOffset(SkeletonHandle handle) const {
    auto it = allocations.find(handle.id);
    if (it != allocations.end() && it->second.allocated) {
        return static_cast<int>(it->second.boneOffset);
    }
    return -1;
}

void BoneTextureManager::cleanup() {
    if (!isInitialized) {
        return;
    }

    std::cout << "清理骨骼纹理管理器..." << std::endl;

    if (boneTexture != 0) {
        glDeleteTextures(1, &boneTexture);
        boneTexture = 0;
    }

    allocations.clear();
    boneMatricesCache.clear();
    dirtyRegions.clear();
    usedBones = 0;
    needsGPUUpdate = false;
    isInitialized = false;

    std::cout << "骨骼纹理管理器清理完成" << std::endl;
}

void BoneTextureManager::createBoneTexture() {
    glGenTextures(1, &boneTexture);
    glBindTexture(GL_TEXTURE_2D, boneTexture);

    // 创建RGBA32F纹理，4列用于存储4x4矩阵
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, TEXTURE_WIDTH, TEXTURE_HEIGHT,
                 0, GL_RGBA, GL_FLOAT, nullptr);

    // 设置纹理参数 - VTF需要精确采样
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    std::cout << "创建骨骼纹理: " << TEXTURE_WIDTH << "x" << TEXTURE_HEIGHT
              << " RGBA32F，纹理ID: " << boneTexture << std::endl;
}

void BoneTextureManager::uploadRegion(uint32_t startBone, uint32_t boneCount) {
    if (boneCount == 0) return;

    // 添加边界检查
    if (startBone + boneCount > TEXTURE_HEIGHT) {
        std::cerr << "错误：骨骼纹理上传越界" << std::endl;
        return;
    }

    // 准备上传数据：将glm::mat4转换为连续的float数组
    std::vector<float> uploadData(boneCount * 16);  // 每个矩阵16个float

    for (uint32_t i = 0; i < boneCount; ++i) {
        const glm::mat4& mat = boneMatricesCache[startBone + i];
        std::memcpy(&uploadData[i * 16], glm::value_ptr(mat), 16 * sizeof(float));
    }

    // 上传到GPU纹理的对应区域
    glBindTexture(GL_TEXTURE_2D, boneTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0,
                    0, static_cast<GLint>(startBone),              // x, y offset
                    TEXTURE_WIDTH, static_cast<GLint>(boneCount),  // width, height
                    GL_RGBA, GL_FLOAT, uploadData.data());

    RenderDevice::getInstance().checkGLError("BoneTextureManager::uploadRegion");
}



//=====================================================================
// TaskSystem
// =========================================================================


#include "GltfTools/AssetSerializer.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <iostream>
#include <chrono>

// TaskSystem的内部实现 - 所有线程相关代码都隐藏在这里
struct TaskSystem::Impl {
    // 资产引用（只读，线程安全）
    const spartan::asset::ProcessedAsset* asset = nullptr;

    // 线程池
    std::vector<std::thread> workers;

    // 任务队列
    std::queue<AnimationTaskInput> taskQueue;
    std::mutex taskQueueMutex;
    std::condition_variable taskCondition;

    // 结果队列
    std::queue<AnimationTaskOutput> resultQueue;
    std::mutex resultQueueMutex;

    // 控制变量
    std::atomic<bool> shutdown{false};
    std::atomic<size_t> pendingTasks{0};
    std::atomic<size_t> completedTasks{0};

    // 构造函数
    Impl() {
        // 创建工作线程（2-4个，基于CPU核心数）
        size_t numThreads = std::max(1u, std::min(4u, std::thread::hardware_concurrency()));
        std::cout << "TaskSystem: 创建 " << numThreads << " 个动画工作线程" << std::endl;

        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back([this, i]() { this->workerLoop(i); });
        }
    }

    // 析构函数
    ~Impl() {
        shutdownWorkers();
    }

    // 工作线程主循环
    void workerLoop(size_t workerId) {
        std::cout << "AnimationWorker " << workerId << " 启动" << std::endl;

        while (!shutdown.load()) {
            AnimationTaskInput task;

            // 获取任务
            {
                std::unique_lock<std::mutex> lock(taskQueueMutex);
                taskCondition.wait(lock, [this] {
                    return !taskQueue.empty() || shutdown.load();
                });

                if (shutdown.load() && taskQueue.empty()) {
                    break;
                }

                task = taskQueue.front();
                taskQueue.pop();
            }

            // 处理任务
            auto result = processAnimationTask(task);

            // 提交结果
            {
                std::lock_guard<std::mutex> lock(resultQueueMutex);
                resultQueue.push(std::move(result));
            }

            pendingTasks.fetch_sub(1);
            completedTasks.fetch_add(1);
        }

        std::cout << "AnimationWorker " << workerId << " 退出" << std::endl;
    }

    // 处理单个动画任务
    AnimationTaskOutput processAnimationTask(const AnimationTaskInput& input) {
        if (!asset) {
            // 返回修改后的新数据结构
            return AnimationTaskOutput(input.skeleton, {}, input.taskId, false);
        }

        // 查找动画和骨架的逻辑
        auto animIt = asset->animations.find(input.animation);
        auto skelIt = asset->skeletons.find(input.skeleton);
        if (animIt == asset->animations.end() || skelIt == asset->skeletons.end() || !animIt->second.skeletal_animation) {
            return AnimationTaskOutput(input.skeleton, {}, input.taskId, false);
        }
        const auto& animData = animIt->second;
        const auto& skelPtr = skelIt->second;

        // 计算动画播放进度的逻辑
        float ratio = 0.0f;
        if (animData.duration > 0.0f) {
            ratio = input.currentTime / animData.duration;
            if (input.looping) {
                ratio = fmod(ratio, 1.0f);
            } else {
                ratio = std::clamp(ratio, 0.0f, 1.0f);
            }
        }

        // 创建临时的采样上下文和变换缓冲区
        const int numJoints = skelPtr->num_joints();
        const int numSoaJoints = skelPtr->num_soa_joints();

        ozz::unique_ptr<ozz::animation::SamplingJob::Context> samplingContext =
                ozz::make_unique<ozz::animation::SamplingJob::Context>(numJoints);

        // 【修改】这个局部变换缓冲区现在是此函数的最终目标
        std::vector<ozz::math::SoaTransform> localTransforms(numSoaJoints);

        // 【核心计算】进行动画采样
        ozz::animation::SamplingJob samplingJob;
        samplingJob.animation = animData.skeletal_animation.get();
        samplingJob.context = samplingContext.get();
        samplingJob.ratio = ratio;
        samplingJob.output = ozz::make_span(localTransforms);

        if (!samplingJob.Run()) {
            return AnimationTaskOutput(input.skeleton, {}, input.taskId, false);
        }

        // ====================================================================
        // 【重要改动】删除所有后续计算步骤
        // ====================================================================
        // 【已删除】不再需要计算模型空间矩阵 (LocalToModelJob)
        // 【已删除】不再需要将 ozz::math::Float4x4 转换为 glm::mat4
        // ====================================================================

        // 【修改】直接打包并返回采样任务的直接结果：局部空间变换
        return AnimationTaskOutput(input.skeleton, std::move(localTransforms), input.taskId, true);
    }

    // 关闭工作线程
    void shutdownWorkers() {
        shutdown.store(true);
        taskCondition.notify_all();

        for (auto& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }

        workers.clear();
        std::cout << "所有动画工作线程已关闭" << std::endl;
    }
};

// =========================================================================
// TaskSystem 公共接口实现
// =========================================================================

TaskSystem& TaskSystem::getInstance() {
    static TaskSystem instance;
    return instance;
}

bool TaskSystem::initialize(const spartan::asset::ProcessedAsset& asset) {
    if (!pImpl) {
        pImpl = std::make_unique<Impl>();
    }

    pImpl->asset = &asset;
    std::cout << "TaskSystem 初始化完成，异步动画计算就绪" << std::endl;
    return true;
}

uint64_t TaskSystem::submitAnimationTask(const AnimationTaskInput& input) {
    if (!pImpl || !pImpl->asset) {
        return 0;
    }

    uint64_t taskId = nextTaskId++;
    AnimationTaskInput taskWithId = input;
    taskWithId.taskId = taskId;

    {
        std::lock_guard<std::mutex> lock(pImpl->taskQueueMutex);
        pImpl->taskQueue.push(taskWithId);
    }

    pImpl->pendingTasks.fetch_add(1);
    pImpl->taskCondition.notify_one();

    return taskId;
}

bool TaskSystem::tryPopAnimationResult(AnimationTaskOutput& output) {
    if (!pImpl) {
        return false;
    }

    std::lock_guard<std::mutex> lock(pImpl->resultQueueMutex);
    if (pImpl->resultQueue.empty()) {
        return false;
    }

    output = std::move(pImpl->resultQueue.front());
    pImpl->resultQueue.pop();
    return true;
}

void TaskSystem::waitForAllPendingTasks() {
    if (!pImpl) {
        return;
    }

    while (pImpl->pendingTasks.load() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void TaskSystem::shutdown() {
    if (pImpl) {
        std::cout << "TaskSystem 关闭中..." << std::endl;
        pImpl->shutdownWorkers();
        pImpl.reset();
        std::cout << "TaskSystem 已关闭" << std::endl;
    }
}

size_t TaskSystem::getPendingTaskCount() const {
    return pImpl ? pImpl->pendingTasks.load() : 0;
}

size_t TaskSystem::getCompletedTaskCount() const {
    return pImpl ? pImpl->completedTasks.load() : 0;
}
