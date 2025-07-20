#include "EntityComponents.h"
#include <algorithm>
#include <iostream>
#include <cmath>

// =========================================================================
// RenderCommand 静态工厂方法实现
// =========================================================================

RenderCommand RenderCommand::DrawMesh(const MeshData* mesh, const MeshData::SubMesh* submesh,
                                      const glm::mat4& model, MaterialHandle mat, float depth, bool wireframe) {
    RenderCommand cmd(RenderCommandType::DRAW_MESH);
    cmd.drawMesh = {mesh, submesh, model, mat, wireframe, depth};
    return cmd;
}

RenderCommand RenderCommand::DrawInstancedMesh(const MeshData* mesh, const MeshData::SubMesh* submesh,
                                               const std::vector<glm::mat4>* instances, MaterialHandle mat) {
    RenderCommand cmd(RenderCommandType::DRAW_INSTANCED_MESH);
    cmd.drawInstancedMesh = {mesh, submesh, instances, mat};
    return cmd;
}

RenderCommand RenderCommand::SetBones(const std::vector<glm::mat4>* bones, int count) {
    RenderCommand cmd(RenderCommandType::SET_BONES);
    cmd.setBones = {bones, count};
    return cmd;
}

RenderCommand RenderCommand::SetUniformMat4(const std::string& name, const glm::mat4& value) {
    RenderCommand cmd(RenderCommandType::SET_UNIFORM);
    cmd.setUniform.name = name;
    cmd.setUniform.type = SetUniformData::MAT4;
    cmd.setUniform.mat4Value = value;
    return cmd;
}

RenderCommand RenderCommand::SetUniformVec3(const std::string& name, const glm::vec3& value) {
    RenderCommand cmd(RenderCommandType::SET_UNIFORM);
    cmd.setUniform.name = name;
    cmd.setUniform.type = SetUniformData::VEC3;
    cmd.setUniform.vec3Value = value;
    return cmd;
}

RenderCommand RenderCommand::SetUniformVec4(const std::string& name, const glm::vec4& value) {
    RenderCommand cmd(RenderCommandType::SET_UNIFORM);
    cmd.setUniform.name = name;
    cmd.setUniform.type = SetUniformData::VEC4;
    cmd.setUniform.vec4Value = value;
    return cmd;
}

RenderCommand RenderCommand::SetUniformFloat(const std::string& name, float value) {
    RenderCommand cmd(RenderCommandType::SET_UNIFORM);
    cmd.setUniform.name = name;
    cmd.setUniform.type = SetUniformData::FLOAT;
    cmd.setUniform.floatValue = value;
    return cmd;
}

RenderCommand RenderCommand::SetUniformInt(const std::string& name, int value) {
    RenderCommand cmd(RenderCommandType::SET_UNIFORM);
    cmd.setUniform.name = name;
    cmd.setUniform.type = SetUniformData::INT;
    cmd.setUniform.intValue = value;
    return cmd;
}

// =========================================================================
// AnimationTrack 实现
// =========================================================================

void AnimationTrack::update(float deltaTime) {
    if (!playing) return;

    currentTime += deltaTime * speed;

    // 权重渐变
    if (weight != fadeTarget) {
        float fadeAmount = fadeSpeed * deltaTime;
        if (weight < fadeTarget) {
            weight = std::min(weight + fadeAmount, fadeTarget);
        } else {
            weight = std::max(weight - fadeAmount, fadeTarget);
        }
    }
}

void AnimationTrack::play() {
    playing = true;
    fadeTarget = 1.0f;
}

void AnimationTrack::stop() {
    playing = false;
    fadeTarget = 0.0f;
}

void AnimationTrack::reset() {
    currentTime = 0.0f;
}

// =========================================================================
// Transform 实现
// =========================================================================

glm::mat4 Transform::getMatrix() const {
    glm::mat4 t = glm::translate(glm::mat4(1.0f), position);
    glm::mat4 r = glm::mat4_cast(rotation);
    glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);
    return t * r * s;
}

// =========================================================================
// MultiTrackAnimationComponent 实现
// =========================================================================

int MultiTrackAnimationComponent::addTrack(AnimationHandle anim, float weight, AnimationBlendMode mode) {
    if (activeTrackCount >= MAX_TRACKS) {
        // 找到权重最小的轨道并替换
        int minWeightIdx = 0;
        float minWeight = tracks[0].weight;
        for (int i = 1; i < activeTrackCount; ++i) {
            if (tracks[i].weight < minWeight) {
                minWeight = tracks[i].weight;
                minWeightIdx = i;
            }
        }

        tracks[minWeightIdx].animation = anim;
        tracks[minWeightIdx].weight = weight;
        tracks[minWeightIdx].fadeTarget = weight;
        tracks[minWeightIdx].blendMode = mode;
        tracks[minWeightIdx].currentTime = 0.0f;
        tracks[minWeightIdx].playing = true;
        return minWeightIdx;
    }

    int idx = activeTrackCount++;
    tracks[idx].animation = anim;
    tracks[idx].weight = 0.0f;  // 从0开始淡入
    tracks[idx].fadeTarget = weight;
    tracks[idx].blendMode = mode;
    tracks[idx].currentTime = 0.0f;
    tracks[idx].playing = true;
    return idx;
}

void MultiTrackAnimationComponent::removeTrack(int index) {
    if (index < 0 || index >= activeTrackCount) return;

    // 淡出动画
    tracks[index].fadeTarget = 0.0f;
}

void MultiTrackAnimationComponent::cleanupTracks() {
    int writeIdx = 0;
    for (int readIdx = 0; readIdx < activeTrackCount; ++readIdx) {
        if (tracks[readIdx].weight > 0.001f || tracks[readIdx].fadeTarget > 0.001f) {
            if (writeIdx != readIdx) {
                tracks[writeIdx] = tracks[readIdx];
            }
            writeIdx++;
        }
    }
    activeTrackCount = writeIdx;
}

void MultiTrackAnimationComponent::update(float deltaTime) {
    if (!masterPlaying) return;

    float adjustedDelta = deltaTime * masterSpeed;

    for (int i = 0; i < activeTrackCount; ++i) {
        tracks[i].update(adjustedDelta);
    }

    // 定期清理无效轨道
    static float cleanupTimer = 0.0f;
    cleanupTimer += deltaTime;
    if (cleanupTimer > 1.0f) {
        cleanupTracks();
        cleanupTimer = 0.0f;
    }
}

void MultiTrackAnimationComponent::getNormalizedWeights(float* weights) const {
    float totalWeight = 0.0f;
    for (int i = 0; i < activeTrackCount; ++i) {
        if (tracks[i].playing) {
            totalWeight += tracks[i].weight;
        }
    }

    if (totalWeight > 0.0f) {
        for (int i = 0; i < activeTrackCount; ++i) {
            weights[i] = tracks[i].playing ? tracks[i].weight / totalWeight : 0.0f;
        }
    } else {
        for (int i = 0; i < activeTrackCount; ++i) {
            weights[i] = 0.0f;
        }
    }
}

// =========================================================================
// AnimationControllerComponent 实现
// =========================================================================

void AnimationControllerComponent::addAnimation(const std::string& name, AnimationHandle handle) {
    animationMap[name] = handle;
    animations.push_back(handle);
}

void AnimationControllerComponent::addState(const std::string& name, AnimationHandle animation, bool looping, float speed) {
    states[name] = {name, animation, looping, speed};
}

void AnimationControllerComponent::addTransition(const std::string& from, const std::string& to) {
    if (states.find(from) != states.end()) {
        states[from].transitions.push_back(to);
    }
}

bool AnimationControllerComponent::canTransitionTo(const std::string& stateName) const {
    if (currentState.empty()) return true;

    auto it = states.find(currentState);
    if (it == states.end()) return true;

    const auto& transitions = it->second.transitions;
    return std::find(transitions.begin(), transitions.end(), stateName) != transitions.end();
}

// =========================================================================
// InputStateComponent 实现
// =========================================================================

bool InputStateComponent::KeyboardState::isKeyDown(SDL_Scancode scancode) const {
    return currentKeys[scancode];
}

bool InputStateComponent::KeyboardState::isKeyPressed(SDL_Scancode scancode) const {
    return currentKeys[scancode] && !previousKeys[scancode];
}

bool InputStateComponent::KeyboardState::isKeyReleased(SDL_Scancode scancode) const {
    return !currentKeys[scancode] && previousKeys[scancode];
}

// =========================================================================
// InstancedMeshComponent 实现
// =========================================================================

void InstancedMeshComponent::addInstance(const glm::mat4& matrix) {
    instanceMatrices.push_back(matrix);
    needsUpdate = true;
}

void InstancedMeshComponent::clearInstances() {
    instanceMatrices.clear();
    needsUpdate = true;
}

size_t InstancedMeshComponent::getInstanceCount() const {
    return instanceMatrices.size();
}


// =======================================================================
// EntityFactory
// =======================================================================

#include "EntityComponents.h"
#include <iostream>
#include <unordered_map>

entt::entity EntityFactory::createCamera(entt::registry& registry) {
    auto entity = registry.create();
    registry.emplace<Transform>(entity);
    registry.emplace<CameraComponent>(entity);
    return entity;
}

std::vector<entt::entity> EntityFactory::createAnimatedModelFromAsset(entt::registry& registry, const ProcessedAsset& asset,
                                                                      int modelIndex) {
    std::vector<entt::entity> created_entities;
    std::unordered_map<int, entt::entity> node_to_entity;
    std::unordered_map<std::string, entt::entity> name_to_entity;

    // --- 1. 创建一个"总根"实体, 它将是这个模型实例在场景中的唯一代表 ---
    auto masterRoot = registry.create();
    registry.emplace<Transform>(masterRoot);
    registry.emplace<LocalTransform>(masterRoot); // 世界矩阵
    registry.emplace<ModelRootTag>(masterRoot);   // 标记为总根
    created_entities.push_back(masterRoot);
    std::cout << "创建模型总根实体 " << modelIndex << ": " << static_cast<uint32_t>(masterRoot) << std::endl;

    // --- 2. 为资源中的每个节点创建对应的实体和基础组件 ---
    for (size_t nodeIdx = 0; nodeIdx < asset.nodes.size(); ++nodeIdx) {
        const auto& node = asset.nodes[nodeIdx];
        auto entity = registry.create();
        created_entities.push_back(entity);
        node_to_entity[static_cast<int>(nodeIdx)] = entity;

        // 保证节点名唯一，用于后续骨骼映射
        std::string node_name = node.name.empty()
                                ? ("joint_" + std::to_string(nodeIdx))
                                : node.name.c_str();
        name_to_entity[node_name] = entity;

        // 每个节点都有局部变换组件，其值由动画系统或父节点驱动
        auto& transform = registry.emplace<Transform>(entity);
        transform.position = ToGLM(node.local_transform.translation);
        transform.rotation = ToGLM(node.local_transform.rotation);
        transform.scale = ToGLM(node.local_transform.scale);

        // 每个节点也都有世界变换组件，其值由变换系统计算
        auto& localTransform = registry.emplace<LocalTransform>(entity);
        localTransform.dirty = true; // 标记需要更新

        // 如果节点有关联的网格，添加 MeshComponent
        if (node.mesh.has_value()) {
            registry.emplace<MeshComponent>(entity, node.mesh.value());
            registry.emplace<RenderStateComponent>(entity);
        }
    }

    // --- 3. 建立实体间的父子层级关系 ---
    for (const auto& [nodeIdx, entity] : node_to_entity) {
        const auto& node = asset.nodes[nodeIdx];
        if (node.parent_index >= 0) {
            // 如果在 glTF 中有父节点，建立父子关系
            registry.emplace<ParentEntity>(entity, node_to_entity.at(node.parent_index));
        } else {
            // 如果在 glTF 中是根节点，就让它成为我们新创建的 masterRoot 的子节点
            registry.emplace<ParentEntity>(entity, masterRoot);
        }
    }

    // --- 4. 将动画和骨架组件【统一】附加到总根实体上 ---
    if (!asset.skeletons.empty()) {
        SkeletonHandle main_skeleton_handle = asset.skeletons.begin()->first;

        // 在总根上创建骨架组件
        auto& skel_comp = registry.emplace<SkeletonComponent>(masterRoot);
        skel_comp.handle = main_skeleton_handle;

        const auto& skeleton_asset = asset.skeletons.at(main_skeleton_handle);
        const int num_joints = skeleton_asset->num_joints();
        const int num_soa_joints = skeleton_asset->num_soa_joints();

        skel_comp.localTransforms.resize(num_soa_joints);
        skel_comp.modelMatrices.resize(num_joints);
        skel_comp.finalTransforms.resize(num_soa_joints);
        skel_comp.skinningMatrices.resize(num_joints, glm::mat4(1.0f));
        skel_comp.samplingContext = ozz::make_unique<ozz::animation::SamplingJob::Context>(num_joints);

        // 核心：填充骨骼索引 -> 实体的映射表
        for (int i = 0; i < skeleton_asset->num_joints(); ++i) {
            std::string joint_name(skeleton_asset->joint_names()[i]);
            if (name_to_entity.count(joint_name)) {
                skel_comp.joint_entity_map[i] = name_to_entity.at(joint_name);
            }
        }

        // 为蒙皮网格找到逆绑定矩阵
        for (const auto& [m_handle, m_data] : asset.meshes) {
            if (m_data.skeleton.has_value()) {
                skel_comp.inverseBindPoses = m_data.inverse_bind_poses;
                break;
            }
        }

        // 将动画播放器和控制器也附加到总根上
        auto& multiTrack = registry.emplace<MultiTrackAnimationComponent>(masterRoot);
        auto& controller = registry.emplace<AnimationControllerComponent>(masterRoot);
        for (const auto& [handle, anim] : asset.animations) {
            // 默认播放第一个动画轨道，防止模型加载后静止
            if (multiTrack.activeTrackCount == 0) {
                multiTrack.addTrack(handle, 1.0f, AnimationBlendMode::Replace);
            }
            controller.addAnimation(anim.name.c_str(), handle);
        }
    }

    // --- 5. 标记需要重建变换系统缓存 ---
    // 注意：这里不再调用静态方法，而是让调用者负责通知变换系统
    // 或者在下一帧自动检测到层级变化时重建缓存

    return created_entities;
}

entt::entity EntityFactory::createInstancedMesh(entt::registry& registry, MeshHandle meshHandle) {
    auto entity = registry.create();
    auto& instancedMesh = registry.emplace<InstancedMeshComponent>(entity);
    instancedMesh.handle = meshHandle;
    std::cout << "创建实例化网格实体，句柄ID: " << meshHandle.id << std::endl;
    return entity;
}

entt::entity EntityFactory::createInstanceSource(entt::registry& registry, MeshHandle meshHandle, const glm::vec3& position) {
    auto entity = registry.create();
    auto& transform = registry.emplace<Transform>(entity);
    transform.position = position;
    registry.emplace<MeshComponent>(entity, meshHandle);
    registry.emplace<RenderStateComponent>(entity);
    registry.emplace<InstanceSourceComponent>(entity);
    return entity;
}

entt::entity EntityFactory::createInstanceController(entt::registry& registry) {
    auto entity = registry.create();
    registry.emplace<InstanceController>(entity);
    std::cout << "创建实例化控制器" << std::endl;
    return entity;
}

entt::entity EntityFactory::createInputHandler(entt::registry& registry) {
    auto entity = registry.create();
    registry.emplace<InputStateComponent>(entity);
    std::cout << "创建输入处理器" << std::endl;
    return entity;
}