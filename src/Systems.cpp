#include "Systems.h"
#include "EntityComponents.h"
#include "Renderer.h"
#include "RenderDevice.h"
#include "glad/glad.h"
#include "AnimationTask.h"
#include <algorithm>
#include <iostream>
#include <cmath>
#include <cstring>

#define M_PI 3.1415926

// =========================================================================
// TransformSystem 实现
// =========================================================================

void TransformSystem::initialize(entt::registry& registry) {
    std::cout << "TransformSystem 初始化" << std::endl;
    // 初始化时强制重建缓存
    invalidateCache();
}

void TransformSystem::update(entt::registry& registry, float deltaTime) {
    // 如果缓存无效，重新构建排序列表
    if (!cache.valid) {
        buildSortedList(registry);
    }

    // 按深度顺序更新所有实体
    updateTransforms(registry);
}

void TransformSystem::invalidateCache() {
    cache.valid = false;
}

void TransformSystem::markDirty(entt::registry& registry, entt::entity entity) {
    if (auto* localTransform = registry.try_get<LocalTransform>(entity)) {
        localTransform->dirty = true;
    }
}

void TransformSystem::printStats() const {
    if (cache.valid) {
        std::cout << "变换系统缓存：" << cache.sortedEntities.size()
                  << " 个实体，最大深度 "
                  << (cache.depths.empty() ? 0 : *std::max_element(cache.depths.begin(), cache.depths.end()))
                  << std::endl;
    }
}

void TransformSystem::buildSortedList(entt::registry& registry) {
    cache.sortedEntities.clear();
    cache.depths.clear();

    // 收集所有需要更新的实体及其深度
    std::vector<std::pair<uint32_t, entt::entity>> depthEntityPairs;

    auto view = registry.view<Transform, LocalTransform>();
    for (auto entity : view) {
        uint32_t depth = calculateDepth(registry, entity);
        depthEntityPairs.emplace_back(depth, entity);

        // 更新ParentEntity组件中的深度信息
        if (auto* parent = registry.try_get<ParentEntity>(entity)) {
            parent->depth = depth;
        }
    }

    // 按深度排序（稳定排序保证同层级顺序一致）
    std::stable_sort(depthEntityPairs.begin(), depthEntityPairs.end(),
                     [](const auto& a, const auto& b) { return a.first < b.first; });

    // 提取排序后的实体列表
    cache.sortedEntities.reserve(depthEntityPairs.size());
    cache.depths.reserve(depthEntityPairs.size());

    for (const auto& [depth, entity] : depthEntityPairs) {
        cache.sortedEntities.push_back(entity);
        cache.depths.push_back(depth);
    }

    cache.valid = true;

    // 构建子实体列表（优化遍历）
    buildChildrenLists(registry);
}

uint32_t TransformSystem::calculateDepth(entt::registry& registry, entt::entity entity) {
    uint32_t depth = 0;
    entt::entity current = entity;

    // 向上遍历到根节点
    while (auto* parent = registry.try_get<ParentEntity>(current)) {
        if (!registry.valid(parent->parent)) break;
        current = parent->parent;
        depth++;

        // 防止循环引用
        if (depth > 1000) {
            std::cerr << "警告：检测到可能的循环父子关系！" << std::endl;
            break;
        }
    }

    return depth;
}

void TransformSystem::buildChildrenLists(entt::registry& registry) {
    // 清空所有现有的子实体列表
    auto childrenView = registry.view<ChildrenComponent>();
    for (auto entity : childrenView) {
        childrenView.get<ChildrenComponent>(entity).children.clear();
    }

    // 重新构建
    auto parentView = registry.view<ParentEntity>();
    for (auto entity : parentView) {
        auto& parent = parentView.get<ParentEntity>(entity);
        if (registry.valid(parent.parent)) {
            auto& children = registry.get_or_emplace<ChildrenComponent>(parent.parent);
            children.children.push_back(entity);
        }
    }
}

void TransformSystem::updateTransforms(entt::registry& registry) {
    // 按排序顺序更新，确保父节点先于子节点
    for (auto entity : cache.sortedEntities) {
        if (!registry.valid(entity)) continue;

        auto* transform = registry.try_get<Transform>(entity);
        auto* localTransform = registry.try_get<LocalTransform>(entity);

        if (!transform || !localTransform) continue;

        // 检查是否需要更新
        if (!needsUpdate(registry, entity, *localTransform)) continue;

        // 更新变换
        updateEntityTransform(registry, entity, *transform, *localTransform);

        // 标记子节点需要更新
        markChildrenDirty(registry, entity);
    }
}

bool TransformSystem::needsUpdate(entt::registry& registry, entt::entity entity,
                                  const LocalTransform& localTransform) {
    if (localTransform.dirty) return true;

    // 检查父节点是否更新过
    if (auto* parent = registry.try_get<ParentEntity>(entity)) {
        if (registry.valid(parent->parent)) {
            if (auto* parentLocal = registry.try_get<LocalTransform>(parent->parent)) {
                return parentLocal->dirty;
            }
        }
    }

    return false;
}

void TransformSystem::updateEntityTransform(entt::registry& registry, entt::entity entity,
                                            Transform& transform, LocalTransform& localTransform) {
    // 从 Transform 组件获取局部变换矩阵
    glm::mat4 localMatrix = transform.getMatrix();

    // 检查是否有父节点
    if (auto* parent = registry.try_get<ParentEntity>(entity)) {
        if (registry.valid(parent->parent)) {
            if (auto* parentLocal = registry.try_get<LocalTransform>(parent->parent)) {
                // 如果有父节点，则世界变换 = 父节点的世界变换 * 当前节点的局部变换
                localTransform.matrix = parentLocal->matrix * localMatrix;
            } else {
                // 父节点没有变换组件（理论上不应发生），直接使用局部变换
                localTransform.matrix = localMatrix;
            }
        } else {
            // 父节点句柄无效，视为根节点
            localTransform.matrix = localMatrix;
        }
    } else {
        // 没有父节点，是根节点，其世界变换就是它的局部变换
        localTransform.matrix = localMatrix;
    }

    // 标记为已更新
    localTransform.dirty = false;
}

void TransformSystem::markChildrenDirty(entt::registry& registry, entt::entity entity) {
    if (auto* children = registry.try_get<ChildrenComponent>(entity)) {
        for (auto child : children->children) {
            if (registry.valid(child)) {
                if (auto* childLocal = registry.try_get<LocalTransform>(child)) {
                    childLocal->dirty = true;
                }
            }
        }
    }
}

// =========================================================================
// AnimationSystem 实现
// =========================================================================
// =========================================================================
// AnimationSystem 异步实现 - 替换Systems.cpp中现有的AnimationSystem部分
// =========================================================================

void AnimationSystem::initialize(entt::registry& registry) {
    std::cout << "AsyncAnimationSystem 初始化" << std::endl;

    // 初始化异步任务系统
    auto& renderer = Renderer::getInstance();
    if (!taskSystem.initialize(renderer.getAsset())) {
        std::cerr << "TaskSystem 初始化失败" << std::endl;
    }
}

void AnimationSystem::cleanup(entt::registry& registry) {
    std::cout << "AsyncAnimationSystem 清理" << std::endl;

    // 等待所有待处理任务完成
    taskSystem.waitForAllPendingTasks();

    // 清理任务状态
    skeletonTasks.clear();

    // 关闭任务系统
    taskSystem.shutdown();
}

void AnimationSystem::update(entt::registry& registry, float deltaTime) {
    if (!globalPlayState) {
        return; // 全局暂停动画
    }

    // 第一阶段：快速收集数据并提交异步任务
    dispatchAnimationTasks(registry, deltaTime);

    // 第二阶段：尝试回收结果并应用
    applyAnimationResults(registry);

    // 更新统计信息
    activeAnimationCount = 0;
    totalSkeletons = 0;
    auto playerView = registry.view<MultiTrackAnimationComponent>();
    for (auto entity : playerView) {
        auto& multiTrack = playerView.get<MultiTrackAnimationComponent>(entity);
        totalSkeletons++;
        if (multiTrack.activeTrackCount > 0) {
            activeAnimationCount++;
        }
    }
}

void AnimationSystem::dispatchAnimationTasks(entt::registry& registry, float deltaTime) {
    auto& renderer = Renderer::getInstance();

    // 遍历所有带动画播放器的实体（通常是模型的总根）
    auto playerView = registry.view<MultiTrackAnimationComponent>();
    for (auto playerEntity : playerView) {
        auto& multiTrack = playerView.get<MultiTrackAnimationComponent>(playerEntity);

        // 更新动画轨道的时间、权重等
        multiTrack.update(deltaTime);

        if (multiTrack.activeTrackCount == 0) {
            continue;
        }

        // 查找这个播放器对应的骨架组件
        if (auto* skeleton = registry.try_get<SkeletonComponent>(playerEntity)) {
            submitSkeletonTask(registry, playerEntity, *skeleton, multiTrack);
        }
    }
}

void AnimationSystem::submitSkeletonTask(entt::registry& registry, entt::entity entity,
                                         SkeletonComponent& skeleton,
                                         const MultiTrackAnimationComponent& multiTrack) {
    auto& taskState = skeletonTasks[skeleton.handle.id];  // 使用handle.id作为key

    // 如果已经有待处理的任务，跳过（避免重复提交）
    if (taskState.pendingTaskId != 0) {
        return;
    }

    // 找到权重最大的主要轨道
    int primaryTrackIndex = -1;
    float maxWeight = 0.0f;
    for (int i = 0; i < multiTrack.activeTrackCount; ++i) {
        const auto& track = multiTrack.tracks[i];
        if (track.playing && track.weight > maxWeight) {
            maxWeight = track.weight;
            primaryTrackIndex = i;
        }
    }

    if (primaryTrackIndex < 0) {
        return;
    }

    const auto& primaryTrack = multiTrack.tracks[primaryTrackIndex];

    // 创建任务输入
    AnimationTaskInput taskInput(
            skeleton.handle,
            primaryTrack.animation,
            primaryTrack.currentTime,
            primaryTrack.speed,
            primaryTrack.weight,
            primaryTrack.looping,
            0  // taskId will be set by TaskSystem
    );

    // 提交任务
    uint64_t taskId = taskSystem.submitAnimationTask(taskInput);
    if (taskId != 0) {
        taskState.pendingTaskId = taskId;
        taskState.lastSubmittedTime = primaryTrack.currentTime;
    }
}

// =========================================================================
// AnimationSystem 关键更新部分 - 集成BoneTextureManager
// =========================================================================

void AnimationSystem::applyAnimationResults(entt::registry& registry) {
    auto& renderer = Renderer::getInstance();
    AnimationTaskOutput result;

    // 步骤1: 从任务队列中取出所有已完成的结果，并更新对应骨架的任务状态缓存
    while (taskSystem.tryPopAnimationResult(result)) {
        if (!result.success) {
            continue;
        }

        auto taskStateIt = skeletonTasks.find(result.skeleton.id);
        if (taskStateIt == skeletonTasks.end()) {
            continue;
        }

        auto& taskState = taskStateIt->second;
        if (taskState.pendingTaskId != result.taskId) {
            continue;
        }

        taskState.cachedLocalTransforms = std::move(result.localSoaTransforms);
        taskState.hasNewResult = true;
        taskState.pendingTaskId = 0;
    }

    // 步骤2: 遍历所有骨架，应用最新的动画结果
    auto skeletonView = registry.view<SkeletonComponent>();
    for (auto entity : skeletonView) {
        auto& skeleton = skeletonView.get<SkeletonComponent>(entity);
        auto& taskState = skeletonTasks[skeleton.handle.id];

        if (taskState.hasNewResult) {
            auto skelIt = renderer.getAsset().skeletons.find(skeleton.handle);
            if (skelIt == renderer.getAsset().skeletons.end()) {
                continue;
            }
            const auto& skelPtr = skelIt->second;

            skeleton.finalTransforms = taskState.cachedLocalTransforms;

            // 【核心1: 驱动节点变换】
            std::vector<ozz::math::Transform> aos_local_transforms(skelPtr->num_joints());
            SoaToAos(ozz::make_span(skeleton.finalTransforms), ozz::make_span(aos_local_transforms));

            for (int i = 0; i < skelPtr->num_joints(); ++i) {
                auto jointEntityIt = skeleton.joint_entity_map.find(i);
                if (jointEntityIt != skeleton.joint_entity_map.end()) {
                    entt::entity joint_entity = jointEntityIt->second;
                    if (registry.valid(joint_entity)) {
                        if (auto* transform = registry.try_get<Transform>(joint_entity)) {
                            const auto& ozz_transform = aos_local_transforms[i];
                            transform->position = ToGLM(ozz_transform.translation);
                            transform->rotation = ToGLM(ozz_transform.rotation);
                            transform->scale = ToGLM(ozz_transform.scale);

                            if (auto* localTransform = registry.try_get<LocalTransform>(joint_entity)) {
                                localTransform->dirty = true;
                            }
                        }
                    }
                }
            }

            // 【核心2: 计算蒙皮矩阵并提交GPU】
            ozz::animation::LocalToModelJob ltmJob;
            ltmJob.skeleton = skelPtr.get();
            ltmJob.input = ozz::make_span(skeleton.finalTransforms);
            ltmJob.output = ozz::make_span(skeleton.modelMatrices);

            if (ltmJob.Run()) {
                // 计算最终蒙皮矩阵
                updateSkinningMatrices(skeleton);

                // 【NEW】将蒙皮矩阵提交给GPU纹理管理器
                auto& boneManager = BoneTextureManager::getInstance();
                if (!boneManager.allocateSkeleton(skeleton.handle, static_cast<uint32_t>(skeleton.skinningMatrices.size()))) {
                    std::cerr << "警告：为骨架分配GPU纹理空间失败" << std::endl;
                } else {
                    boneManager.updateSkeletonMatrices(skeleton.handle, skeleton.skinningMatrices);
                }

                skeleton.needsUpdate = true;
            }
        }

        taskState.hasNewResult = false;
    }

    // 【NEW】批量提交所有骨骼矩阵更新到GPU
    BoneTextureManager::getInstance().commitToGPU();
}

void AnimationSystem::forceRefresh(entt::registry& registry) {
    std::cout << "强制刷新异步动画系统" << std::endl;

    // 清除所有任务状态
    skeletonTasks.clear();

    // 重置所有动画轨道
    auto view = registry.view<MultiTrackAnimationComponent>();
    for (auto entity : view) {
        auto& multiTrack = view.get<MultiTrackAnimationComponent>(entity);
        for (int i = 0; i < multiTrack.activeTrackCount; ++i) {
            multiTrack.tracks[i].reset();
        }
    }
}

void AnimationSystem::setGlobalPlayState(bool playing) {
    globalPlayState = playing;
    std::cout << "全局动画播放状态: " << (globalPlayState ? "播放" : "暂停") << std::endl;
}

void AnimationSystem::printAnimationStats() const {
    std::cout << "异步动画系统统计: " << activeAnimationCount << "/" << totalSkeletons
              << " 个骨架有活跃动画, 待处理任务: " << taskSystem.getPendingTaskCount()
              << ", 已完成任务: " << taskSystem.getCompletedTaskCount() << std::endl;
}

// =========================================================================
// 原有的辅助方法保持不变（用于fallback和多轨道混合）
// =========================================================================

void AnimationSystem::SoaToAos(ozz::span<const ozz::math::SoaTransform> _soa, ozz::span<ozz::math::Transform> _aos) {
    const int num_soa_nodes = static_cast<int>(_soa.size());
    const int num_nodes = static_cast<int>(_aos.size());

    for (int i = 0; i < num_soa_nodes; ++i) {
        const ozz::math::SoaTransform& soa_transform = _soa[i];

        ozz::math::SimdFloat4 translation[4], rotation[4], scale[4];
        ozz::math::Transpose3x4(&soa_transform.translation.x, translation);
        ozz::math::Transpose4x4(&soa_transform.rotation.x, rotation);
        ozz::math::Transpose3x4(&soa_transform.scale.x, scale);

        const int i4 = i * 4;
        for (int j = 0; j < 4 && (i4 + j) < num_nodes; ++j) {
            ozz::math::Transform& out = _aos[i4 + j];
            ozz::math::Store3PtrU(translation[j], &out.translation.x);
            ozz::math::StorePtrU(rotation[j], &out.rotation.x);
            ozz::math::Store3PtrU(scale[j], &out.scale.x);
        }
    }
}

void AnimationSystem::applyTrackToSkeleton(const AnimationTrack& track, float normalizedWeight,
                                           const std::vector<ozz::math::SoaTransform>& trackTransforms,
                                           std::vector<ozz::math::SoaTransform>& finalTransforms) {
    const float weight = track.weight * normalizedWeight;

    switch (track.blendMode) {
        case AnimationBlendMode::Replace:
            if (weight >= 0.999f) {
                std::copy(trackTransforms.begin(), trackTransforms.end(), finalTransforms.begin());
            } else {
                blendSoaTransforms(finalTransforms, trackTransforms, weight, finalTransforms);
            }
            break;

        case AnimationBlendMode::Additive:
            additiveSoaTransforms(finalTransforms, trackTransforms, weight, finalTransforms);
            break;

        case AnimationBlendMode::Blend:
            blendSoaTransforms(finalTransforms, trackTransforms, weight, finalTransforms);
            break;
    }
}

void AnimationSystem::blendSoaTransforms(const std::vector<ozz::math::SoaTransform>& from,
                                         const std::vector<ozz::math::SoaTransform>& to,
                                         float ratio,
                                         std::vector<ozz::math::SoaTransform>& result) {
    if (from.size() != to.size() || from.size() != result.size()) {
        return;
    }

    const ozz::math::SimdFloat4 blend_weight = ozz::math::simd_float4::Load1(ratio);

    for (size_t i = 0; i < result.size(); ++i) {
        result[i].translation = ozz::math::Lerp(from[i].translation, to[i].translation, blend_weight);
        result[i].scale = ozz::math::Lerp(from[i].scale, to[i].scale, blend_weight);

        // 四元数插值前修正符号
        ozz::math::SoaQuaternion corrected_to = to[i].rotation;
        ozz::math::SimdFloat4 dot = from[i].rotation.x * to[i].rotation.x +
                                    from[i].rotation.y * to[i].rotation.y +
                                    from[i].rotation.z * to[i].rotation.z +
                                    from[i].rotation.w * to[i].rotation.w;

        ozz::math::SimdFloat4 negative_mask = ozz::math::CmpLt(dot, ozz::math::simd_float4::zero());
        corrected_to.x = ozz::math::Select(negative_mask, -corrected_to.x, corrected_to.x);
        corrected_to.y = ozz::math::Select(negative_mask, -corrected_to.y, corrected_to.y);
        corrected_to.z = ozz::math::Select(negative_mask, -corrected_to.z, corrected_to.z);
        corrected_to.w = ozz::math::Select(negative_mask, -corrected_to.w, corrected_to.w);

        result[i].rotation = ozz::math::NLerp(from[i].rotation, corrected_to, blend_weight);
    }
}

void AnimationSystem::additiveSoaTransforms(const std::vector<ozz::math::SoaTransform>& base,
                                            const std::vector<ozz::math::SoaTransform>& additive,
                                            float weight,
                                            std::vector<ozz::math::SoaTransform>& result) {
    if (base.size() != additive.size() || base.size() != result.size()) {
        return;
    }
    const ozz::math::SimdFloat4 blend_weight = ozz::math::simd_float4::Load1(weight);

    for (size_t i = 0; i < result.size(); ++i) {
        result[i].translation = base[i].translation + additive[i].translation * blend_weight;

        ozz::math::SoaQuaternion identity;
        identity.x = ozz::math::simd_float4::zero();
        identity.y = ozz::math::simd_float4::zero();
        identity.z = ozz::math::simd_float4::zero();
        identity.w = ozz::math::simd_float4::one();

        ozz::math::SoaQuaternion additiveRot = ozz::math::NLerp(identity, additive[i].rotation, blend_weight);
        result[i].rotation = base[i].rotation * additiveRot;

        ozz::math::SoaFloat3 one;
        one.x = ozz::math::simd_float4::one();
        one.y = ozz::math::simd_float4::one();
        one.z = ozz::math::simd_float4::one();

        ozz::math::SoaFloat3 additiveScale = ozz::math::Lerp(one, additive[i].scale, blend_weight);
        result[i].scale = base[i].scale * additiveScale;
    }
}

void AnimationSystem::updateSkinningMatrices(SkeletonComponent& skeleton) {
    const int numBones = static_cast<int>(skeleton.modelMatrices.size());
    skeleton.skinningMatrices.resize(numBones);

    for (int i = 0; i < numBones; ++i) {
        if (i < static_cast<int>(skeleton.inverseBindPoses.size())) {
            const ozz::math::Float4x4& ozzMatrix = skeleton.modelMatrices[i];
            glm::mat4 modelMatrix;

            for (int col = 0; col < 4; ++col) {
                float column[4];
                ozz::math::StorePtrU(ozzMatrix.cols[col], column);
                modelMatrix[col] = glm::vec4(column[0], column[1], column[2], column[3]);
            }
            skeleton.skinningMatrices[i] = modelMatrix * skeleton.inverseBindPoses[i];
        }
    }
}

// =========================================================================
// 其他系统的快速实现（保持功能完整性）
// =========================================================================

void InputSystem::update(entt::registry& registry, float deltaTime) {
    auto view = registry.view<InputStateComponent>();
    for (auto entity : view) {
        auto& input = view.get<InputStateComponent>(entity);

        updateMouseState(input.mouse);
        updateKeyboardState(input.keyboard);
        handleMouseInput(registry, input);
        handleKeyboardInput(registry, input);
        handleDiscreteInput(registry, input);

        input.keyboard.pressedThisFrame.clear();
        input.keyboard.releasedThisFrame.clear();
        input.mouse.wheelDelta = 0.0f;
    }
}

// InputSystem的其他方法实现保持不变...
void InputSystem::processEvent(SDL_Event& event, entt::registry& registry, bool& running) {
    if (event.type == SDL_QUIT) {
        running = false;
        return;
    }

    auto view = registry.view<InputStateComponent>();
    for (auto entity : view) {
        auto& input = view.get<InputStateComponent>(entity);

        switch (event.type) {
            case SDL_KEYDOWN:
                if (!event.key.repeat) {
                    input.keyboard.pressedThisFrame.push_back(event.key.keysym.sym);
                }
                break;

            case SDL_KEYUP:
                input.keyboard.releasedThisFrame.push_back(event.key.keysym.sym);
                break;

            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                    input.mouse.isRelativeMode = true;
                }
                break;

            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    SDL_SetRelativeMouseMode(SDL_FALSE);
                    input.mouse.isRelativeMode = false;
                }
                break;

            case SDL_MOUSEMOTION:
                input.mouse.deltaPosition.x = event.motion.xrel * input.config.mouseSensitivity;
                input.mouse.deltaPosition.y = event.motion.yrel * input.config.mouseSensitivity;
                if (input.config.invertMouseY) {
                    input.mouse.deltaPosition.y = -input.mouse.deltaPosition.y;
                }
                break;

            case SDL_MOUSEWHEEL:
                input.mouse.wheelDelta = event.wheel.y;
                break;
        }
    }
}

void InputSystem::updateMouseState(InputStateComponent::MouseState& mouse) {
    int x, y;
    uint32_t buttons = SDL_GetMouseState(&x, &y);

    mouse.position.x = static_cast<float>(x);
    mouse.position.y = static_cast<float>(y);
    mouse.leftButtonPressed = (buttons & SDL_BUTTON_LMASK) != 0;
    mouse.rightButtonPressed = (buttons & SDL_BUTTON_RMASK) != 0;
    mouse.middleButtonPressed = (buttons & SDL_BUTTON_MMASK) != 0;
}

void InputSystem::updateKeyboardState(InputStateComponent::KeyboardState& keyboard) {
    keyboard.previousKeys = keyboard.currentKeys;

    const Uint8* state = SDL_GetKeyboardState(nullptr);
    for (int i = 0; i < SDL_NUM_SCANCODES; ++i) {
        keyboard.currentKeys[i] = state[i] != 0;
    }
}

void InputSystem::handleMouseInput(entt::registry& registry, const InputStateComponent& input) {
    if (!input.mouse.isRelativeMode) return;

    auto cameraView = registry.view<CameraComponent>();
    for (auto entity : cameraView) {
        auto& camera = cameraView.get<CameraComponent>(entity);

        camera.angle += input.mouse.deltaPosition.x;
        camera.manualControlTimer = camera.manualControlDuration;
    }

    if (std::abs(input.mouse.wheelDelta) > 0.1f) {
        for (auto entity : cameraView) {
            auto& camera = cameraView.get<CameraComponent>(entity);

            if (input.mouse.wheelDelta > 0) {
                camera.distance *= 0.9f;
            } else {
                camera.distance *= 1.1f;
            }
            camera.distance = std::clamp(camera.distance, 0.1f, 100.0f);
            camera.manualControlTimer = camera.manualControlDuration;
        }
    }
}

void InputSystem::handleKeyboardInput(entt::registry& registry, const InputStateComponent& input) {
    bool anyKeyPressed = false;
    float deltaTime = 1.0f / 60.0f;

    auto cameraView = registry.view<Transform, CameraComponent>();
    for (auto entity : cameraView) {
        auto& transform = cameraView.get<Transform>(entity);
        auto& camera = cameraView.get<CameraComponent>(entity);

        glm::vec3 forward = glm::normalize(camera.target - transform.position);
        glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
        float moveSpeed = input.config.keyboardMoveSpeed * deltaTime;

        if (input.keyboard.isKeyDown(SDL_SCANCODE_W)) {
            camera.target += forward * moveSpeed;
            anyKeyPressed = true;
        }
        if (input.keyboard.isKeyDown(SDL_SCANCODE_S)) {
            camera.target -= forward * moveSpeed;
            anyKeyPressed = true;
        }
        if (input.keyboard.isKeyDown(SDL_SCANCODE_A)) {
            camera.target -= right * moveSpeed;
            anyKeyPressed = true;
        }
        if (input.keyboard.isKeyDown(SDL_SCANCODE_D)) {
            camera.target += right * moveSpeed;
            anyKeyPressed = true;
        }
    }

    // 模型缩放控制
    auto rootView = registry.view<Transform, ControllableCharacterTag>();
    for (auto entity : rootView) {
        auto& transform = rootView.get<Transform>(entity);

        float scaleSpeed = input.config.keyboardScaleSpeed * deltaTime;
        if (input.keyboard.isKeyDown(SDL_SCANCODE_Q)) {
            transform.scale *= (1.0f + scaleSpeed);
            transform.scale = glm::clamp(transform.scale, glm::vec3(0.01f), glm::vec3(10.0f));
            anyKeyPressed = true;
            // 标记变换系统需要更新
            if (auto* localTransform = registry.try_get<LocalTransform>(entity)) {
                localTransform->dirty = true;
            }
        }
        if (input.keyboard.isKeyDown(SDL_SCANCODE_E)) {
            transform.scale *= (1.0f - scaleSpeed);
            transform.scale = glm::clamp(transform.scale, glm::vec3(0.01f), glm::vec3(10.0f));
            anyKeyPressed = true;
            if (auto* localTransform = registry.try_get<LocalTransform>(entity)) {
                localTransform->dirty = true;
            }
        }
    }

    if (anyKeyPressed) {
        for (auto entity : cameraView) {
            auto& camera = cameraView.get<CameraComponent>(entity);
            camera.manualControlTimer = camera.manualControlDuration;
        }
    }
}

void InputSystem::handleDiscreteInput(entt::registry& registry, const InputStateComponent& input) {
    for (SDL_Keycode key : input.keyboard.pressedThisFrame) {
        switch (key) {
            case SDLK_ESCAPE:
            {
                SDL_Event quitEvent;
                quitEvent.type = SDL_QUIT;
                SDL_PushEvent(&quitEvent);
                break;
            }
            case SDLK_SPACE:
                registry.view<MultiTrackAnimationComponent>().each([](auto& multiTrack) {
                    multiTrack.masterPlaying = !multiTrack.masterPlaying;
                    std::cout << "动画 " << (multiTrack.masterPlaying ? "继续" : "暂停") << std::endl;
                });
                break;

            case SDLK_r:
                registry.view<MultiTrackAnimationComponent>().each([](auto& multiTrack) {
                    for (int i = 0; i < multiTrack.activeTrackCount; ++i) {
                        multiTrack.tracks[i].reset();
                    }
                    std::cout << "重置所有动画轨道" << std::endl;
                });
                break;

            case SDLK_f:
                registry.view<RenderStateComponent>().each([](auto& renderState) {
                    renderState.wireframe = !renderState.wireframe;
                    std::cout << "线框模式: " << (renderState.wireframe ? "开启" : "关闭") << std::endl;
                });
                break;

            case SDLK_c:
                registry.view<CameraComponent>().each([](auto& camera) {
                    camera.autoRotate = !camera.autoRotate;
                    std::cout << "自动旋转: " << (camera.autoRotate ? "开启" : "关闭") << std::endl;
                });
                break;
        }
    }
}

const char* InputSystem::getPatternName(InstanceController::Pattern pattern) {
    switch (pattern) {
        case InstanceController::Pattern::LINE: return "直线";
        case InstanceController::Pattern::CIRCLE: return "圆形";
        case InstanceController::Pattern::GRID: return "网格";
        case InstanceController::Pattern::RANDOM: return "随机";
        default: return "未知";
    }
}

// CameraSystem 实现
void CameraSystem::update(entt::registry& registry, float deltaTime) {
    auto view = registry.view<Transform, CameraComponent>();

    for (auto entity : view) {
        auto& transform = view.get<Transform>(entity);
        auto& camera = view.get<CameraComponent>(entity);

        if (camera.manualControlTimer > 0.0f) {
            camera.manualControlTimer -= deltaTime;
        }

        if (camera.autoRotate && camera.manualControlTimer <= 0.0f) {
            camera.angle += deltaTime * camera.rotationSpeed;
        }

        float height = camera.distance * 0.3f;
        transform.position.x = camera.target.x + sin(camera.angle) * camera.distance;
        transform.position.z = camera.target.z + cos(camera.angle) * camera.distance;
        transform.position.y = camera.target.y + height;

        // 标记缓存需要更新
        viewMatrixDirty = true;
        projectionMatrixDirty = true;
    }
}

glm::mat4 CameraSystem::getViewMatrix(entt::registry& registry) const {
    if (viewMatrixDirty) {
        auto view = registry.view<Transform, CameraComponent>();
        for (auto entity : view) {
            auto& transform = view.get<Transform>(entity);
            auto& camera = view.get<CameraComponent>(entity);
            cachedViewMatrix = glm::lookAt(transform.position, camera.target, glm::vec3(0, 1, 0));
            viewMatrixDirty = false;
            break; // 使用第一个相机
        }
    }
    return cachedViewMatrix;
}

glm::mat4 CameraSystem::getProjectionMatrix(entt::registry& registry) const {
    if (projectionMatrixDirty) {
        auto& renderer = Renderer::getInstance();
        float aspect = float(renderer.getWindowWidth()) / renderer.getWindowHeight();

        auto view = registry.view<CameraComponent>();
        for (auto entity : view) {
            auto& camera = view.get<CameraComponent>(entity);
            cachedProjectionMatrix = glm::perspective(
                    glm::radians(camera.fov), aspect, camera.near, camera.far);
            projectionMatrixDirty = false;
            break; // 使用第一个相机
        }
    }
    return cachedProjectionMatrix;
}

// InstancedRenderSystem 实现
void InstancedRenderSystem::update(entt::registry& registry, float deltaTime) {
    forceUpdateInstances(registry);
}

void InstancedRenderSystem::forceUpdateInstances(entt::registry& registry) {
    auto& renderer = Renderer::getInstance();

    auto instancedView = registry.view<InstancedMeshComponent>();
    for (auto entity : instancedView) {
        auto& instancedMesh = instancedView.get<InstancedMeshComponent>(entity);
        instancedMesh.clearInstances();
    }

    auto sourceView = registry.view<Transform, MeshComponent, InstanceSourceComponent>();
    for (auto entity : sourceView) {
        auto& transform = sourceView.get<Transform>(entity);
        auto& meshComp = sourceView.get<MeshComponent>(entity);

        for (auto instancedEntity : instancedView) {
            auto& instancedMesh = instancedView.get<InstancedMeshComponent>(instancedEntity);

            if (instancedMesh.handle == meshComp.handle) {
                instancedMesh.addInstance(transform.getMatrix());
                break;
            }
        }
    }

    for (auto entity : instancedView) {
        auto& instancedMesh = instancedView.get<InstancedMeshComponent>(entity);
        if (instancedMesh.needsUpdate && instancedMesh.getInstanceCount() > 0) {
            updateInstanceBuffer(instancedMesh);

            auto& asset = renderer.getAsset();
            auto meshIt = asset.meshes.find(instancedMesh.handle);
            if (meshIt != asset.meshes.end()) {
                renderer.setupInstancedVAO(meshIt->second, instancedMesh.instanceBuffer);
            }

            instancedMesh.needsUpdate = false;
        }
    }
}

void InstancedRenderSystem::updateInstanceBuffer(InstancedMeshComponent& instancedMesh) {
    if (instancedMesh.instanceMatrices.empty()) {
        return;
    }

    if (instancedMesh.instanceBuffer == 0) {
        glGenBuffers(1, &instancedMesh.instanceBuffer);
    }

    glBindBuffer(GL_ARRAY_BUFFER, instancedMesh.instanceBuffer);
    glBufferData(GL_ARRAY_BUFFER,
                 instancedMesh.instanceMatrices.size() * sizeof(glm::mat4),
                 instancedMesh.instanceMatrices.data(),
                 GL_DYNAMIC_DRAW);
}

// InstanceControlSystem 实现
void InstanceControlSystem::update(entt::registry& registry, float deltaTime) {
    auto controllerView = registry.view<InstanceController>();

    for (auto entity : controllerView) {
        auto& controller = controllerView.get<InstanceController>(entity);

        if (controller.enableAnimation) {
            controller.time += deltaTime * controller.animationSpeed;
            controller.needsUpdate = true;
        }

        if (controller.needsUpdate) {
            updateInstancePositions(registry, controller);
            controller.needsUpdate = false;
        }
    }
}

void InstanceControlSystem::updateInstancePositions(entt::registry& registry, InstanceController& controller) {
    auto sourceView = registry.view<InstanceSourceComponent>();
    std::vector<entt::entity> toDestroy;
    for (auto entity : sourceView) {
        toDestroy.push_back(entity);
    }
    for (auto entity : toDestroy) {
        registry.destroy(entity);
    }

    auto& renderer = Renderer::getInstance();
    auto& asset = renderer.getAsset();
    if (asset.meshes.empty()) return;
    auto meshHandle = asset.meshes.begin()->first;

    std::vector<glm::vec3> positions = generatePositions(controller);

    for (const auto& pos : positions) {
        createInstanceSource(registry, meshHandle, pos);
    }
}

std::vector<glm::vec3> InstanceControlSystem::generatePositions(const InstanceController& controller) {
    std::vector<glm::vec3> positions;
    positions.reserve(controller.instanceCount);

    switch (controller.pattern) {
        case InstanceController::Pattern::LINE:
            return generateLinePositions(controller);
        case InstanceController::Pattern::CIRCLE:
            return generateCirclePositions(controller);
        case InstanceController::Pattern::GRID:
            return generateGridPositions(controller);
        case InstanceController::Pattern::RANDOM:
            return generateRandomPositions(controller);
    }

    return positions;
}

std::vector<glm::vec3> InstanceControlSystem::generateLinePositions(const InstanceController& controller) {
    std::vector<glm::vec3> positions;

    for (int i = 0; i < controller.instanceCount; ++i) {
        float offset = (i - (controller.instanceCount - 1) * 0.5f) * controller.spacing;
        glm::vec3 pos(offset, 0.0f, 0.0f);

        if (controller.enableAnimation) {
            pos.y += 0.5f * sin(controller.time + i * 0.5f);
            pos.x += 0.2f * cos(controller.time * 0.7f + i * 0.3f);
        }

        positions.push_back(pos);
    }

    return positions;
}

std::vector<glm::vec3> InstanceControlSystem::generateCirclePositions(const InstanceController& controller) {
    std::vector<glm::vec3> positions;
    float radius = controller.circleRadius > 0 ? controller.circleRadius : controller.spacing;

    for (int i = 0; i < controller.instanceCount; ++i) {
        float angle = 2.0f * M_PI * i / controller.instanceCount;

        if (controller.enableAnimation) {
            angle += controller.time * 0.5f;
        }

        glm::vec3 pos(
                radius * cos(angle),
                controller.enableAnimation ? 0.3f * sin(controller.time * 2.0f + i) : 0.0f,
                radius * sin(angle)
        );

        positions.push_back(pos);
    }

    return positions;
}

std::vector<glm::vec3> InstanceControlSystem::generateGridPositions(const InstanceController& controller) {
    std::vector<glm::vec3> positions;

    int count = 0;
    for (int x = 0; x < controller.gridWidth && count < controller.instanceCount; ++x) {
        for (int z = 0; z < controller.gridHeight && count < controller.instanceCount; ++z) {
            glm::vec3 pos(
                    (x - controller.gridWidth * 0.5f) * controller.spacing,
                    0.0f,
                    (z - controller.gridHeight * 0.5f) * controller.spacing
            );

            if (controller.enableAnimation) {
                pos.y += 0.4f * sin(controller.time + x * 0.5f + z * 0.3f);
            }

            positions.push_back(pos);
            count++;
        }
    }

    return positions;
}

std::vector<glm::vec3> InstanceControlSystem::generateRandomPositions(const InstanceController& controller) {
    std::vector<glm::vec3> positions;

    std::srand(controller.randomSeed);

    for (int i = 0; i < controller.instanceCount; ++i) {
        glm::vec3 pos(
                controller.areaMin.x + (rand() / float(RAND_MAX)) * (controller.areaMax.x - controller.areaMin.x),
                controller.areaMin.y + (rand() / float(RAND_MAX)) * (controller.areaMax.y - controller.areaMin.y),
                controller.areaMin.z + (rand() / float(RAND_MAX)) * (controller.areaMax.z - controller.areaMin.z)
        );

        positions.push_back(pos);
    }

    return positions;
}

entt::entity InstanceControlSystem::createInstanceSource(entt::registry& registry, MeshHandle meshHandle, const glm::vec3& position) {
    auto entity = registry.create();

    auto& transform = registry.emplace<Transform>(entity);
    transform.position = position;

    registry.emplace<MeshComponent>(entity, meshHandle);
    registry.emplace<RenderStateComponent>(entity);
    registry.emplace<InstanceSourceComponent>(entity);

    return entity;
}

// LightManager 实现
void LightManager::update(entt::registry& registry, float deltaTime) {
    // 目前是空实现，预留给后续扩展
    // 可以在这里更新动态光照、阴影等
}

void LightManager::addDirectionalLight(const glm::vec3& direction, const glm::vec3& color, float intensity) {
    DirectionalLight light;
    light.direction = glm::normalize(direction);
    light.color = color;
    light.intensity = intensity;
    directionalLights.push_back(light);

    std::cout << "添加方向光: 方向(" << direction.x << ", " << direction.y << ", " << direction.z
              << "), 强度: " << intensity << std::endl;
}

glm::vec3 LightManager::getMainLightDirection() const {
    if (!directionalLights.empty()) {
        return directionalLights[0].direction;
    }
    return glm::vec3(-0.5f, -1.0f, -0.5f); // 默认方向
}