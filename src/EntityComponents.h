#pragma once

#include "GltfTools/GltfTools.h"
#include "GltfTools/AssetSerializer.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/maths/soa_float4x4.h"
#include "ozz/base/memory/unique_ptr.h"
#include <entt/entt.hpp>
#include <vector>
#include <string>
#include <unordered_map>
#include <array>
#include <SDL2/SDL.h>

using namespace spartan::asset;

// =========================================================================
// 渲染指令系统枚举和结构体
// =========================================================================

enum class RenderCommandType {
    DRAW_MESH,
    DRAW_INSTANCED_MESH,
    SET_MATERIAL,
    SET_BONES,
    SET_UNIFORM
};

// 动画混合模式
enum class AnimationBlendMode {
    Replace,    // 替换模式 - 完全覆盖之前的动画
    Additive,   // 叠加模式 - 在现有动画基础上叠加
    Blend       // 混合模式 - 与之前的动画进行权重混合
};

// =========================================================================
// 渲染指令结构体
// =========================================================================

struct RenderCommand {
    RenderCommandType type;
    uint64_t sortKey = 0;

    struct DrawMeshData {
        const MeshData* mesh;
        const MeshData::SubMesh* submesh;
        glm::mat4 modelMatrix;
        MaterialHandle material;
        bool wireframe = false;
        float viewSpaceDepth = 0.0f;
    };

    struct DrawInstancedMeshData {
        const MeshData* mesh;
        const MeshData::SubMesh* submesh;
        const std::vector<glm::mat4>* instanceMatrices;
        MaterialHandle material;
    };

    struct SetBonesData {
        const std::vector<glm::mat4>* boneMatrices;
        int boneCount;
    };

    struct SetUniformData {
        std::string name;
        enum Type { MAT4, VEC3, VEC4, FLOAT, INT } type;

        glm::mat4 mat4Value{1.0f};
        glm::vec3 vec3Value{0.0f};
        glm::vec4 vec4Value{0.0f};
        float floatValue = 0.0f;
        int intValue = 0;
    };

    DrawMeshData drawMesh{};
    DrawInstancedMeshData drawInstancedMesh{};
    SetBonesData setBones{};
    SetUniformData setUniform{};

    RenderCommand(RenderCommandType t) : type(t) {}
    RenderCommand() : type(RenderCommandType::DRAW_MESH) {}
    ~RenderCommand() = default;
    RenderCommand(const RenderCommand&) = default;
    RenderCommand(RenderCommand&&) = default;
    RenderCommand& operator=(const RenderCommand&) = default;
    RenderCommand& operator=(RenderCommand&&) = default;

    // 静态工厂方法
    static RenderCommand DrawMesh(const MeshData* mesh, const MeshData::SubMesh* submesh,
                                  const glm::mat4& model, MaterialHandle mat, float depth, bool wireframe = false);
    static RenderCommand DrawInstancedMesh(const MeshData* mesh, const MeshData::SubMesh* submesh,
                                           const std::vector<glm::mat4>* instances, MaterialHandle mat);
    static RenderCommand SetBones(const std::vector<glm::mat4>* bones, int count);
    static RenderCommand SetUniformMat4(const std::string& name, const glm::mat4& value);
    static RenderCommand SetUniformVec3(const std::string& name, const glm::vec3& value);
    static RenderCommand SetUniformVec4(const std::string& name, const glm::vec4& value);
    static RenderCommand SetUniformFloat(const std::string& name, float value);
    static RenderCommand SetUniformInt(const std::string& name, int value);
};

// =========================================================================
// ECS组件定义
// =========================================================================

// 单个动画轨道
struct AnimationTrack {
    AnimationHandle animation;
    float currentTime = 0.0f;
    float speed = 1.0f;
    float weight = 1.0f;
    bool playing = false;
    bool looping = true;
    AnimationBlendMode blendMode = AnimationBlendMode::Replace;

    // 轨道淡入淡出
    float fadeTarget = 1.0f;
    float fadeSpeed = 2.0f;

    void update(float deltaTime);
    void play();
    void stop();
    void reset();
};

// 局部变换矩阵
struct LocalTransform {
    glm::mat4 matrix{1.0f};
    bool dirty = true;  // 标记是否需要更新
};

// 父实体引用
struct ParentEntity {
    entt::entity parent = entt::null;
    uint32_t depth = 0;  // 在层级中的深度，用于优化排序
};

struct NodeComponent {
    uint32_t index; // 存储原始的 GLTF node index
};

// 子实体列表（可选，用于优化遍历）
struct ChildrenComponent {
    std::vector<entt::entity> children;
};

struct ModelRootTag {};

// 多轨动画播放器组件
struct MultiTrackAnimationComponent {
    static constexpr int MAX_TRACKS = 8;  // 最大轨道数，遵循斯巴达原则

    AnimationTrack tracks[MAX_TRACKS];
    int activeTrackCount = 0;

    // 主控制
    bool masterPlaying = true;
    float masterSpeed = 1.0f;

    // 成员方法声明
    int addTrack(AnimationHandle anim, float weight = 1.0f, AnimationBlendMode mode = AnimationBlendMode::Replace);
    void removeTrack(int index);
    void cleanupTracks();
    void update(float deltaTime);
    void getNormalizedWeights(float* weights) const;
};

struct SkeletonComponent {
    SkeletonHandle handle;
    std::vector<ozz::math::SoaTransform> localTransforms;
    std::vector<ozz::math::Float4x4> modelMatrices;
    ozz::unique_ptr<ozz::animation::SamplingJob::Context> samplingContext;
    std::vector<glm::mat4> skinningMatrices;
    std::vector<glm::mat4> inverseBindPoses;

    // 【新增】一个从骨骼索引到对应实体的映射表
    std::unordered_map<int, entt::entity> joint_entity_map;

    // 多轨道混合缓冲区
    std::vector<ozz::math::SoaTransform> blendBuffer[MultiTrackAnimationComponent::MAX_TRACKS];
    std::vector<ozz::math::SoaTransform> finalTransforms;

    bool needsUpdate = true;
};

struct Transform {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    // 多轨道混合缓冲区
    struct TrackTransform {
        glm::vec3 position{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 scale{1.0f};
        bool valid = false;
    };
    TrackTransform trackTransforms[MultiTrackAnimationComponent::MAX_TRACKS];

    glm::mat4 getMatrix() const;
};

// 动画控制器组件
struct AnimationControllerComponent {
    std::vector<AnimationHandle> animations;
    std::unordered_map<std::string, AnimationHandle> animationMap;

    struct AnimationState {
        std::string name;
        AnimationHandle animation;
        bool looping = true;
        float speed = 1.0f;
        std::vector<std::string> transitions;
    };

    std::unordered_map<std::string, AnimationState> states;
    std::string currentState;
    std::string defaultState = "idle";

    void addAnimation(const std::string& name, AnimationHandle handle);
    void addState(const std::string& name, AnimationHandle animation, bool looping = true, float speed = 1.0f);
    void addTransition(const std::string& from, const std::string& to);
    bool canTransitionTo(const std::string& stateName) const;
};

struct MeshComponent {
    MeshHandle handle;
    std::vector<MaterialHandle> materials;
};

struct MaterialComponent {
    MaterialHandle handle;
};

struct TextureComponent {
    TextureHandle handle;
};

struct CameraComponent {
    glm::vec3 target{0.0f, 0.5f, 0.0f};
    float distance = 4.0f;
    float angle = 0.0f;
    float fov = 45.0f;
    float near = 0.1f;
    float far = 100.0f;
    bool autoRotate = false;
    float rotationSpeed = 0.5f;
    float manualControlTimer = 0.0f;
    const float manualControlDuration = 3.0f;
};

struct RenderStateComponent {
    bool wireframe = false;
    bool visible = true;
    int renderLayer = 0;
    float alpha = 1.0f;
};

struct InputStateComponent {
    struct MouseState {
        glm::vec2 position{0.0f};
        glm::vec2 deltaPosition{0.0f};
        bool leftButtonPressed = false;
        bool rightButtonPressed = false;
        bool middleButtonPressed = false;
        float wheelDelta = 0.0f;
        bool isRelativeMode = false;
    } mouse;

    struct KeyboardState {
        std::array<bool, SDL_NUM_SCANCODES> currentKeys{};
        std::array<bool, SDL_NUM_SCANCODES> previousKeys{};
        std::vector<SDL_Keycode> pressedThisFrame;
        std::vector<SDL_Keycode> releasedThisFrame;

        bool isKeyDown(SDL_Scancode scancode) const;
        bool isKeyPressed(SDL_Scancode scancode) const;
        bool isKeyReleased(SDL_Scancode scancode) const;
    } keyboard;

    struct InputConfig {
        float mouseSensitivity = 0.005f;
        float keyboardMoveSpeed = 2.0f;
        float keyboardScaleSpeed = 1.0f;
        bool invertMouseY = false;
    } config;
};

struct InstancedMeshComponent {
    MeshHandle handle;
    std::vector<MaterialHandle> materials;
    std::vector<glm::mat4> instanceMatrices;
    uint32_t instanceBuffer = 0;
    bool needsUpdate = true;

    void addInstance(const glm::mat4& matrix);
    void clearInstances();
    size_t getInstanceCount() const;
};

struct InstanceSourceComponent {};

// 新增一个Tag，用于标记当前玩家可以控制的角色
struct ControllableCharacterTag {};

// 新增一个结构体，用于定义每个模型实例的加载信息
struct ModelInstanceConfig {
    std::string filePath;
    glm::vec3 initialPosition = glm::vec3(0.0f);
    glm::vec3 initialScale = glm::vec3(1.0f);
};

struct InstanceController {
    int instanceCount = 3;
    float spacing = 2.0f;
    float circleRadius = 0.0f;
    bool enableAnimation = false;
    float animationSpeed = 1.0f;
    float time = 0.0f;

    enum class Pattern {
        LINE, CIRCLE, GRID, RANDOM
    } pattern = Pattern::LINE;

    int gridWidth = 3;
    int gridHeight = 3;
    int randomSeed = 12345;
    glm::vec3 areaMin = glm::vec3(-5.0f, 0.0f, -5.0f);
    glm::vec3 areaMax = glm::vec3(5.0f, 2.0f, 5.0f);
    bool needsUpdate = true;
};


#pragma once

#include "GltfTools/AssetSerializer.h"
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <vector>

using namespace spartan::asset;

// =========================================================================
// 实体工厂 - 支持资源复用的模型创建系统
// =========================================================================

class EntityFactory {
public:
    static entt::entity createCamera(entt::registry& registry);

    static std::vector<entt::entity> createAnimatedModelFromAsset(entt::registry& registry,
                                                                  const ProcessedAsset& asset,
                                                                  int modelIndex = 0);

    static entt::entity createInstancedMesh(entt::registry& registry, MeshHandle meshHandle);

    static entt::entity createInstanceSource(entt::registry& registry,
                                             MeshHandle meshHandle,
                                             const glm::vec3& position);

    static entt::entity createInstanceController(entt::registry& registry);

    static entt::entity createInputHandler(entt::registry& registry);
};