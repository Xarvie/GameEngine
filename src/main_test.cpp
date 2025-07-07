/*
 * 文件: main_test.cpp
 * 描述: [最终修正版] 运行时主程序。
 * - 添加了 STB_IMAGE_IMPLEMENTATION 宏定义。
 * - 修正了蒙皮矩阵的计算逻辑，直接使用场景图的世界变换，解决了动画和缩放问题。
 * - 修正了 ozz/base/io/stream.h 的头文件引用。
 * - [修正] 实现了动态加载动画文件，使其不再依赖硬编码的文件名。
 * - [修正] 调整了相机和光照参数以获得更好的观察效果。
 */
#define STB_IMAGE_IMPLEMENTATION
#include "example_util.h" // 包含所有辅助代码
#include "mesh.h"
#include "render_material.h"
#include "gltf_tools.h"

#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/animation/runtime/skeleton_utils.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/io/stream.h"
#include "ozz/base/log.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/memory/unique_ptr.h"
#include "ozz/base/maths/simd_math.h"
#include "ozz/base/span.h"

// SoA 到 AoS 转换的辅助函数
inline void SoaToAos(ozz::span<const ozz::math::SoaTransform> _soa, ozz::span<ozz::math::Transform> _aos) {
    assert(_soa.size() * 4 >= _aos.size());
    const int num_soa_joints = static_cast<int>(_soa.size());
    const int num_joints = static_cast<int>(_aos.size());
    for (int i = 0; i < num_soa_joints; ++i) {
        const ozz::math::SoaTransform& soa_transform = _soa[i];
        ozz::math::SimdFloat4 translation[4], rotation[4], scale[4];
        ozz::math::Transpose3x4(&soa_transform.translation.x, translation);
        ozz::math::Transpose4x4(&soa_transform.rotation.x, rotation);
        ozz::math::Transpose3x4(&soa_transform.scale.x, scale);
        for (int j = 0; j < 4 && i * 4 + j < num_joints; ++j) {
            ozz::math::Transform& out = _aos[i * 4 + j];
            ozz::math::Store3PtrU(translation[j], &out.translation.x);
            ozz::math::StorePtrU(rotation[j], &out.rotation.x);
            ozz::math::Store3PtrU(scale[j], &out.scale.x);
        }
    }
}

// 顶点着色器 (完整版)
const char* pbrVertexShaderSource_fixed = R"(
#version 300 es
precision highp float;

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec4 aTangent;
layout (location = 4) in uvec4 aJointIndices;
layout (location = 5) in vec4 aJointWeights;

out vec3 FragPos;
out vec2 TexCoords;
out mat3 TBN;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model; // 全局模型变换

uniform bool u_isSkinned;
uniform mat4 u_skinning_matrices[128];
uniform mat4 u_static_transform; // 用于静态网格的世界变换

void main()
{
    mat4 final_node_transform;
    if (u_isSkinned) {
        mat4 skin_matrix =
            aJointWeights.x * u_skinning_matrices[aJointIndices.x] +
            aJointWeights.y * u_skinning_matrices[aJointIndices.y] +
            aJointWeights.z * u_skinning_matrices[aJointIndices.z] +
            aJointWeights.w * u_skinning_matrices[aJointIndices.w];
        final_node_transform = skin_matrix;
    } else {
        final_node_transform = u_static_transform;
    }

    vec4 worldPos = model * final_node_transform * vec4(aPos, 1.0);
    FragPos = vec3(worldPos);
    TexCoords = aTexCoords;

    mat3 normalMatrix = transpose(inverse(mat3(model * final_node_transform)));
    vec3 N = normalize(normalMatrix * aNormal);
    vec3 T = normalize(normalMatrix * aTangent.xyz);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T) * aTangent.w;
    TBN = mat3(T, B, N);

    gl_Position = projection * view * worldPos;
}
)";

// 片段着色器 (完整版)
const char* pbrFragmentShaderSource_final_es3 = R"(
#version 300 es
precision highp float;

in vec3 FragPos;
in vec2 TexCoords;
in mat3 TBN;

out vec4 FragColor;

uniform sampler2D u_baseColorTexture;
uniform sampler2D u_metallicRoughnessTexture;
uniform sampler2D u_normalTexture;
uniform sampler2D u_occlusionTexture;
uniform sampler2D u_emissiveTexture;

uniform vec4 u_baseColorFactor;
uniform float u_metallicFactor;
uniform float u_roughnessFactor;
uniform vec3 u_emissiveFactor;

uniform bool u_hasBaseColorTexture;
uniform bool u_hasMetallicRoughnessTexture;
uniform bool u_hasNormalTexture;
uniform bool u_hasOcclusionTexture;
uniform bool u_hasEmissiveTexture;

uniform vec3 u_lightPositions[4];
uniform vec3 u_lightColors[4];
uniform int u_lightCount;
uniform vec3 u_cameraPos;

const float PI = 3.14159265359;

vec3 srgbToLinear(vec3 srgb) {
    return pow(srgb, vec3(2.2));
}

vec3 linearToSrgb(vec3 linear) {
    return pow(linear, vec3(1.0/2.2));
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return num / max(denom, 0.0001);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return num / max(denom, 0.0001);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    vec4 baseColor = u_baseColorFactor;
    if (u_hasBaseColorTexture) {
        vec4 texColor = texture(u_baseColorTexture, TexCoords);
        texColor.rgb = srgbToLinear(texColor.rgb);
        baseColor *= texColor;
    }

    if (baseColor.a < 0.01) {
        discard;
    }

    float metallic = u_metallicFactor;
    float roughness = u_roughnessFactor;
    if (u_hasMetallicRoughnessTexture) {
        vec3 mr = texture(u_metallicRoughnessTexture, TexCoords).rgb;
        roughness *= mr.g;
        metallic *= mr.b;
    }
    roughness = max(roughness, 0.04);

    vec3 N = normalize(TBN[2]);
    if (u_hasNormalTexture) {
        vec3 tangentNormal = texture(u_normalTexture, TexCoords).xyz * 2.0 - 1.0;
        N = normalize(TBN * tangentNormal);
    }

    float ao = 1.0;
    if (u_hasOcclusionTexture) {
        ao = texture(u_occlusionTexture, TexCoords).r;
    }

    vec3 emissive = u_emissiveFactor;
    if (u_hasEmissiveTexture) {
        vec3 emissiveTexColor = texture(u_emissiveTexture, TexCoords).rgb;
        emissive *= srgbToLinear(emissiveTexColor);
    }

    vec3 albedo = baseColor.rgb;
    vec3 V = normalize(u_cameraPos - FragPos);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);
    for(int i = 0; i < u_lightCount && i < 4; ++i) {
        vec3 L = normalize(u_lightPositions[i] - FragPos);
        vec3 H = normalize(V + L);
        float distance = length(u_lightPositions[i] - FragPos);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance = u_lightColors[i] * attenuation;

        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;

        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 color = ambient + Lo + emissive;

    color = color / (color + vec3(1.0));
    color = linearToSrgb(color);

    FragColor = vec4(color, baseColor.a);
}
)";

#define fileName "AnimatedMorphCube" // 可以切换为 "robot" 或 "ABeautifulGame"
int main(int argc_, char* argv_[]) {
    // 1. 运行资产转换工具
    char* argv[] = {".", "--gltf=art/" fileName "/glTF/" fileName ".gltf", "--output=assets/" fileName};
    gltf2Ozz(3, argv);

    // 2. 初始化SDL和OpenGL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Failed to init SDL" << std::endl;
        return -1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_Window* window = SDL_CreateWindow("High-Performance PBR Renderer", 100, 100, 1280, 720, SDL_WINDOW_OPENGL);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!gladLoadGLES2Loader((GLADloadproc)SDL_GL_GetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glViewport(0, 0, 1280, 720);
    SDL_SetRelativeMouseMode(SDL_TRUE);

    // 3. 创建着色器和纹理管理器
    GLuint pbr_program = CreateShaderProgram(pbrVertexShaderSource_fixed, pbrFragmentShaderSource_final_es3);
    SimpleTextureManager texture_manager;

    // 4. 加载所有Ozz资产
    ozz::sample::MeshAsset mesh_asset;
    ozz::io::File mesh_file("assets/" fileName ".mesh.ozz", "rb");
    if (!mesh_file.opened()) { std::cerr << "Cannot open mesh file." << std::endl; return -1; }
    ozz::io::IArchive mesh_archive(&mesh_file);
    mesh_archive >> mesh_asset;

    ozz::sample::MaterialSet material_set;
    ozz::io::File material_file("assets/" fileName ".materials.ozz", "rb");
    if (!material_file.opened()) { std::cerr << "Cannot open material file." << std::endl; return -1; }
    ozz::io::IArchive material_archive(&material_file);
    material_archive >> material_set;

    ozz::unique_ptr<ozz::animation::Skeleton> skeleton = ozz::make_unique<ozz::animation::Skeleton>();
    ozz::io::File skel_file("assets/" fileName ".skeleton.ozz", "rb");
    if (skel_file.opened()) {
        ozz::io::IArchive skel_archive(&skel_file);
        skel_archive >> *skeleton;
    } else {
        skeleton.reset();
        std::cout << "No skeleton file found, assuming a static-only model." << std::endl;
    }

    ozz::unique_ptr<ozz::animation::Animation> animation = ozz::make_unique<ozz::animation::Animation>();
    // [修正] 动态查找并加载第一个动画文件
    std::string anim_path_str;
    for(int i = 0; i < 10; ++i) { // 尝试查找 animation_0 到 animation_9
        std::string temp_path = std::string("assets/") + fileName + ".animation_" + std::to_string(i) + ".anim.ozz";
        ozz::io::File test_file(temp_path.c_str(), "rb");
        if(test_file.opened()){
            anim_path_str = temp_path;
            break;
        }
    }
    // 兼容旧的命名方式
    if (anim_path_str.empty()) {
        anim_path_str = "assets/AnimatedCube.animation_AnimatedCube.anim.ozz";
    }

    if (anim_path_str.empty()) {
        anim_path_str = "assets/robot.Take_001.anim.ozz";
    }

    ozz::io::File anim_file(anim_path_str.c_str(), "rb");
    if (anim_file.opened()) {
        ozz::io::IArchive anim_archive(&anim_file);
        anim_archive >> *animation;
        std::cout << "Successfully loaded animation: " << anim_path_str << std::endl;
    } else {
        animation.reset();
        std::cout << "Could not load any animation file." << std::endl;
    }

    // 5. 准备运行时材质数据
    std::vector<MaterialData> materials;
    for (const auto& src_mat : material_set.materials) {
        MaterialData mat_data;
        mat_data.base_color_factor = glm::vec4(src_mat.base_color_factor.x, src_mat.base_color_factor.y, src_mat.base_color_factor.z, src_mat.base_color_factor.w);
        mat_data.metallic_factor = src_mat.metallic_factor;
        mat_data.roughness_factor = src_mat.roughness_factor;
        mat_data.emissive_factor = glm::vec3(0.0f);
        std::string base_path = "art/" fileName "/glTF/";
        if (!src_mat.base_color_texture_path.empty()) mat_data.base_color_texture = texture_manager.LoadTexture(base_path + src_mat.base_color_texture_path.c_str());
        if (!src_mat.metallic_roughness_texture_path.empty()) mat_data.metallic_roughness_texture = texture_manager.LoadTexture(base_path + src_mat.metallic_roughness_texture_path.c_str());
        if (!src_mat.normal_texture_path.empty()) mat_data.normal_texture = texture_manager.LoadTexture(base_path + src_mat.normal_texture_path.c_str());
        if (!src_mat.occlusion_texture_path.empty()) mat_data.occlusion_texture = texture_manager.LoadTexture(base_path + src_mat.occlusion_texture_path.c_str());
        if (!src_mat.emissive_texture_path.empty()) mat_data.emissive_texture = texture_manager.LoadTexture(base_path + src_mat.emissive_texture_path.c_str());
        materials.push_back(mat_data);
    }

    // 6. 准备运行时数据
    ozz::animation::SamplingJob::Context scene_sampling_context;
    ozz::vector<ozz::math::SoaTransform> scene_local_soa_transforms;
    ozz::vector<ozz::math::Transform> scene_local_transforms;
    std::vector<glm::mat4> scene_world_matrices;
    if (!mesh_asset.scene_nodes.empty()) {
        scene_sampling_context.Resize(mesh_asset.scene_nodes.size());
        size_t num_soa_nodes = (mesh_asset.scene_nodes.size() + 3) / 4;
        scene_local_soa_transforms.resize(num_soa_nodes);
        scene_local_transforms.resize(mesh_asset.scene_nodes.size());
        scene_world_matrices.resize(mesh_asset.scene_nodes.size());
    }

    std::vector<glm::mat4> skinning_gl_matrices;
    std::vector<int> skeleton_to_scene_node_map;
    if (skeleton) {
        skinning_gl_matrices.resize(skeleton->num_joints());
        skeleton_to_scene_node_map.resize(skeleton->num_joints());

        std::map<std::string, int> scene_node_name_to_index_map;
        for(size_t i = 0; i < mesh_asset.scene_nodes.size(); ++i) {
            scene_node_name_to_index_map[mesh_asset.scene_nodes[i].name.c_str()] = i;
        }

        for(int i = 0; i < skeleton->num_joints(); ++i) {
            const char* joint_name = skeleton->joint_names()[i];
            auto it = scene_node_name_to_index_map.find(joint_name);
            if(it != scene_node_name_to_index_map.end()) {
                skeleton_to_scene_node_map[i] = it->second;
            } else {
                skeleton_to_scene_node_map[i] = -1;
            }
        }
    }

    // 7. 设置VAO/VBO/EBO
    GLuint vao, vbo, ebo;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh_asset.vertex_buffer.size(), mesh_asset.vertex_buffer.data(), GL_STATIC_DRAW);
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh_asset.index_buffer.size() * sizeof(uint32_t), mesh_asset.index_buffer.data(), GL_STATIC_DRAW);
    GLsizei stride = static_cast<GLsizei>(mesh_asset.vertex_stride);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(VertexData, position));
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(VertexData, normal));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(VertexData, uv0));
    glEnableVertexAttribArray(3); glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(VertexData, tangent));
    glEnableVertexAttribArray(4); glVertexAttribIPointer(4, 4, GL_UNSIGNED_SHORT, stride, (void*)offsetof(VertexData, joint_indices));
    glEnableVertexAttribArray(5); glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(VertexData, joint_weights));
    glBindVertexArray(0);

    // 8. 主循环
    bool running = true, pause_animation = false;
    float time_ratio = 0.f;
    Uint32 last_tick = SDL_GetTicks();
    glm::vec3 cameraPos = glm::vec3(0.0f, 2.0f, 5.0f), cameraFront = glm::vec3(0.0f, 0.0f, -1.0f), cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
    float yaw = -90.0f, pitch = -15.0f, lastX = 1280.0f / 2.0f, lastY = 720.0f / 2.0f, cameraSpeed = 0.005f;
    bool firstMouse = true;

    while (running) {
        Uint32 current_tick = SDL_GetTicks();
        float delta_time_ms = (current_tick - last_tick);
        float delta_time_s = delta_time_ms / 1000.0f;
        last_tick = current_tick;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) running = false;
                if (event.key.keysym.sym == SDLK_p) pause_animation = !pause_animation;
            }
            if (event.type == SDL_MOUSEMOTION) {
                if (firstMouse) { lastX = event.motion.x; lastY = event.motion.y; firstMouse = false; }
                float xoffset = event.motion.xrel, yoffset = -event.motion.yrel;
                float sensitivity = 0.1f;
                xoffset *= sensitivity; yoffset *= sensitivity;
                yaw += xoffset; pitch += yoffset;
                if (pitch > 89.0f) pitch = 89.0f; if (pitch < -89.0f) pitch = -89.0f;
                glm::vec3 front;
                front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
                front.y = sin(glm::radians(pitch));
                front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
                cameraFront = glm::normalize(front);
            }
        }
        const Uint8* keystate = SDL_GetKeyboardState(NULL);
        if (keystate[SDL_SCANCODE_W]) cameraPos += cameraSpeed * delta_time_ms * cameraFront;
        if (keystate[SDL_SCANCODE_S]) cameraPos -= cameraSpeed * delta_time_ms * cameraFront;
        if (keystate[SDL_SCANCODE_A]) cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed * delta_time_ms;
        if (keystate[SDL_SCANCODE_D]) cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed * delta_time_ms;

        // 8.1 更新场景图动画
        if (animation && !mesh_asset.scene_nodes.empty()) {
            if (!pause_animation) {
                time_ratio += delta_time_s / animation->duration();
                if (time_ratio > 1.f) time_ratio = fmodf(time_ratio, 1.f);
            }
            ozz::animation::SamplingJob sampling_job;
            sampling_job.animation = animation.get();
            sampling_job.context = &scene_sampling_context;
            sampling_job.ratio = time_ratio;
            sampling_job.output = ozz::make_span(scene_local_soa_transforms);
            sampling_job.Run();
            SoaToAos(ozz::make_span(scene_local_soa_transforms), ozz::make_span(scene_local_transforms));
        } else if (!mesh_asset.scene_nodes.empty()) {
            for(size_t i=0; i<mesh_asset.scene_nodes.size(); ++i) {
                scene_local_transforms[i] = mesh_asset.scene_nodes[i].transform;
            }
        }

        // 8.2 计算场景图世界矩阵
        for(size_t i = 0; i < mesh_asset.scene_nodes.size(); ++i) {
            const auto& node = mesh_asset.scene_nodes[i];
            glm::mat4 local_transform = OzzToGlm(ozz::math::Float4x4::FromAffine(scene_local_transforms[i]));
            if (node.parent_index != -1) {
                scene_world_matrices[i] = scene_world_matrices[node.parent_index] * local_transform;
            } else {
                scene_world_matrices[i] = local_transform;
            }
        }

        // 8.3 计算蒙皮矩阵
        if (skeleton) {
            for (int i = 0; i < skeleton->num_joints(); ++i) {
                int scene_node_idx = skeleton_to_scene_node_map[i];
                if (scene_node_idx != -1) {
                    glm::mat4 joint_world_mat = scene_world_matrices[scene_node_idx];
                    glm::mat4 inv_bind_mat = OzzToGlm(mesh_asset.inverse_bind_poses[i]);
                    skinning_gl_matrices[i] = joint_world_mat * inv_bind_mat;
                } else {
                    skinning_gl_matrices[i] = glm::mat4(1.0f);
                }
            }
        }

        // 8.4 渲染
        glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(pbr_program);

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 model_mat = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));

        glUniformMatrix4fv(glGetUniformLocation(pbr_program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(pbr_program, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(pbr_program, "model"), 1, GL_FALSE, glm::value_ptr(model_mat));
        glUniform3fv(glGetUniformLocation(pbr_program, "u_cameraPos"), 1, glm::value_ptr(cameraPos));

        glm::vec3 lightPositions[] = {glm::vec3(5.0f, 5.0f, 5.0f), glm::vec3(-5.0f, 5.0f, -5.0f)};
        glm::vec3 lightColors[] = {glm::vec3(300.0f), glm::vec3(200.0f)};
        glUniform3fv(glGetUniformLocation(pbr_program, "u_lightPositions"), 2, glm::value_ptr(lightPositions[0]));
        glUniform3fv(glGetUniformLocation(pbr_program, "u_lightColors"), 2, glm::value_ptr(lightColors[0]));
        glUniform1i(glGetUniformLocation(pbr_program, "u_lightCount"), 2);

        if (!skinning_gl_matrices.empty()) {
            glUniformMatrix4fv(glGetUniformLocation(pbr_program, "u_skinning_matrices"), static_cast<GLsizei>(skinning_gl_matrices.size()), GL_FALSE, glm::value_ptr(skinning_gl_matrices[0]));
        }

        glBindVertexArray(vao);
        GLint is_skinned_loc = glGetUniformLocation(pbr_program, "u_isSkinned");
        GLint static_transform_loc = glGetUniformLocation(pbr_program, "u_static_transform");

        for (const auto& part : mesh_asset.parts) {
            if (part.material_index >= materials.size()) continue;
            BindMaterial(pbr_program, materials[part.material_index]);

            if (part.is_static_body) {
                glUniform1i(is_skinned_loc, 0);
                if (part.scene_node_index != -1 && (size_t)part.scene_node_index < scene_world_matrices.size()) {
                    glUniformMatrix4fv(static_transform_loc, 1, GL_FALSE, glm::value_ptr(scene_world_matrices[part.scene_node_index]));
                }
            } else {
                glUniform1i(is_skinned_loc, 1);
            }
            glDrawElements(GL_TRIANGLES, part.index_count, GL_UNSIGNED_INT, (void*)(part.index_offset * sizeof(uint32_t)));
        }
        glBindVertexArray(0);
        SDL_GL_SwapWindow(window);
    }

    // 9. 清理
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
    glDeleteProgram(pbr_program);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
