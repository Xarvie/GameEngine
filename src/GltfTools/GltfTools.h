#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <optional>
#include <vector>
#include <string>
#include <functional>
#include "AssetTypes.h"
#include "ozz/base/containers/string.h"
#include "ozz/base/memory/unique_ptr.h"
#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/animation/runtime/animation.h"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-literal-operator"
#endif
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif
#include <set>

namespace spartan {
    namespace asset {

// 前向声明
        struct ProcessedAsset;

// GPU友好的顶点格式定义
        struct VertexFormat {
            enum Attribute : uint32_t {
                POSITION    = 1 << 0,
                NORMAL      = 1 << 1,
                TANGENT     = 1 << 2,
                UV0         = 1 << 3,
                UV1         = 1 << 4,
                COLOR0      = 1 << 5,
                JOINTS0     = 1 << 6,
                WEIGHTS0    = 1 << 7,

                // 静态网格标准格式
                STATIC_MESH = POSITION | NORMAL | TANGENT | UV0,
                // 蒙皮网格标准格式
                SKINNED_MESH = STATIC_MESH | JOINTS0 | WEIGHTS0,
            };

            uint32_t attributes = 0;
            uint32_t stride = 0;

            struct AttributeInfo {
                uint32_t offset;
                uint32_t components;
                uint32_t type; // GL_FLOAT, GL_UNSIGNED_BYTE等
                bool normalized;
            };
            std::unordered_map<Attribute, AttributeInfo> attribute_map;
        };

// Morph Target定义
        struct MorphTarget {
            struct Displacement {
                uint32_t vertex_index;
                ozz::math::Float3 position_delta;
                ozz::math::Float3 normal_delta;
                ozz::math::Float3 tangent_delta;
            };

            ozz::string name;
            std::vector<Displacement> sparse_displacements; // 稀疏存储，只存储变化的顶点

            // GPU数据
            uint32_t gpu_texture_id = 0; // 位移纹理
            uint32_t affected_vertex_count = 0;
        };

// 网格数据
        struct MeshData {
            // 顶点数据
            std::vector<uint8_t> vertex_buffer;
            VertexFormat format;
            uint32_t vertex_count = 0;

            // 索引数据
            std::vector<uint32_t> index_buffer;

            // 子网格（按材质分组）
            struct SubMesh {
                uint32_t index_offset;
                uint32_t index_count;
                uint32_t base_vertex = 0;
                MaterialHandle material;

                // 边界信息
                ozz::math::Float3 aabb_min;
                ozz::math::Float3 aabb_max;
            };
            std::vector<SubMesh> submeshes;


            // 骨骼信息（如果是蒙皮网格）
            std::optional<SkeletonHandle> skeleton;
            std::vector<glm::mat4> inverse_bind_poses; // 使用GLM矩阵

            // GPU资源
            uint32_t vbo = 0;
            uint32_t ibo = 0;
            uint32_t vao = 0;

            // 实例化数据
            uint32_t instance_buffer = 0;
            uint32_t max_instance_count = 0;

            // 数据状态标志
            enum DataState : uint8_t {
                CPU_ONLY = 0,      // 只有CPU数据
                GPU_ONLY = 1,      // 只有GPU数据
                SYNCED = 2,        // CPU和GPU数据同步
                DIRTY = 3          // CPU数据已修改，需要重新上传
            } data_state = CPU_ONLY;

            // 释放CPU数据（在上传到GPU后节省内存）
            void ReleaseCPUData() {
                if (data_state == SYNCED || data_state == GPU_ONLY) {
                    vertex_buffer.clear();
                    vertex_buffer.shrink_to_fit();
                    index_buffer.clear();
                    index_buffer.shrink_to_fit();
                    data_state = GPU_ONLY;
                }
            }

            // 标记为需要更新
            void MarkDirty() {
                if (data_state == SYNCED || data_state == GPU_ONLY) {
                    data_state = DIRTY;
                }
            }

            // 检查是否需要上传到GPU
            bool NeedsGPUUpload() const {
                return data_state == CPU_ONLY || data_state == DIRTY;
            }

            // 检查是否有CPU数据
            bool HasCPUData() const {
                return data_state != GPU_ONLY;
            }

            // 检查是否有GPU数据
            bool HasGPUData() const {
                return vbo != 0 && ibo != 0 && (data_state != CPU_ONLY);
            }
        };

// 材质数据
        struct MaterialData {
            ozz::string name;

            // PBR参数
            ozz::math::Float4 base_color_factor = {1.0f, 1.0f, 1.0f, 1.0f};
            float metallic_factor = 1.0f;
            float roughness_factor = 1.0f;
            ozz::math::Float3 emissive_factor = {0.0f, 0.0f, 0.0f};

            // 纹理
            std::optional<TextureHandle> base_color_texture;
            std::optional<TextureHandle> metallic_roughness_texture;
            std::optional<TextureHandle> normal_texture;
            std::optional<TextureHandle> occlusion_texture;
            std::optional<TextureHandle> emissive_texture;

            // 渲染状态
            enum AlphaMode {
                MODE_OPAQUE,  // 避免与Windows宏冲突
                MODE_MASK,
                MODE_BLEND
            } alpha_mode = MODE_OPAQUE;
            float alpha_cutoff = 0.5f;
            bool double_sided = false;

            // Shader变体
            uint32_t shader_variant_flags = 0;
        };
// 节点变换动画的独立数据结构
        struct NodeTransformData {
            std::vector<float> position_times;
            std::vector<ozz::math::Float3> position_values;

            std::vector<float> rotation_times;
            std::vector<ozz::math::Quaternion> rotation_values;

            std::vector<float> scale_times;
            std::vector<ozz::math::Float3> scale_values;

            enum InterpolationType : uint8_t {
                LINEAR,
                STEP,
                CUBIC_SPLINE
            } interpolation = LINEAR;
        };

        // 动画数据
        struct AnimationData {
            ozz::string name;
            float duration = 0.0f;

            // 目标资源 (作为主要目标的提示)
            std::optional<SkeletonHandle> target_skeleton;
            std::optional<uint32_t> target_node;
            std::optional<MeshHandle> target_mesh;
            std::optional<uint32_t> root_motion_joint_index;

            // 动画数据
            std::unique_ptr<ozz::animation::Animation> skeletal_animation;

            std::unordered_map<uint32_t, NodeTransformData> node_animations;

        };

// 纹理数据
        struct TextureData {
            ozz::string uri;
            uint32_t width = 0;
            uint32_t height = 0;
            uint32_t channels = 0;

            enum Format {
                R8,
                RG8,
                RGB8,
                RGBA8,
                // 压缩格式
                BC1_RGB,
                BC3_RGBA,
                BC4_R,
                BC5_RG,
                BC7_RGBA,
            } format = RGBA8;

            bool is_srgb = false;

            // GPU资源
            uint32_t gpu_texture_id = 0;

            // Mipmap信息
            uint32_t mip_levels = 1;
            bool generate_mipmaps = true;
        };

// 场景节点
        struct SceneNode {
            ozz::string name;
            int parent_index = -1;
            std::vector<int> children;

            // 变换
            ozz::math::Transform local_transform;

            // 关联的资源
            std::optional<MeshHandle> mesh;
            std::optional<SkeletonHandle> skeleton;
            std::optional<uint32_t> camera_index;
            std::optional<uint32_t> light_index;

            // 动画目标标记
            bool is_animation_target = false;
        };

// 处理配置
        struct ProcessConfig {
            // 网格处理
            bool merge_meshes_by_material = true;
            bool generate_tangents = true;
            bool optimize_vertex_cache = true;
            float vertex_position_epsilon = 1e-6f;
            float vertex_normal_epsilon = 1e-3f;

            // 纹理处理
            bool compress_textures = true;
            bool generate_mipmaps = true;
            uint32_t max_texture_size = 4096;

            // 动画处理
            bool compress_animations = true;
            float animation_position_tolerance = 0.001f;
            float animation_rotation_tolerance = 0.001f;
            float animation_scale_tolerance = 0.001f;

            // 性能选项
            uint32_t max_bones_per_vertex = 4;
            uint32_t max_morph_targets = 8;
            bool enable_gpu_skinning = true;
            bool enable_instancing = true;
        };

// GLTF处理器
        class GltfProcessor {
        public:
            explicit GltfProcessor(const ProcessConfig& config = ProcessConfig{});
            ~GltfProcessor();

            // 禁用拷贝
            GltfProcessor(const GltfProcessor&) = delete;
            GltfProcessor& operator=(const GltfProcessor&) = delete;

            // 处理GLTF文件
            bool ProcessFile(const char* input_path, ProcessedAsset& output);

            // 获取错误信息
            const std::string& GetLastError() const { return last_error_; }

            // 进度回调
            using ProgressCallback = std::function<void(float progress, const char* stage)>;
            void SetProgressCallback(ProgressCallback callback) { progress_callback_ = callback; }

        private:
            struct Impl;
            std::unique_ptr<Impl> impl_;

            ProcessConfig config_;
            std::string last_error_;
            ProgressCallback progress_callback_;
        };

// 处理后的资产包
        struct ProcessedAsset {

            // 资源存储
            std::unordered_map<MeshHandle, MeshData, HandleHash<MeshTag>> meshes;
            std::unordered_map<MaterialHandle, MaterialData, HandleHash<MaterialTag>> materials;
            std::unordered_map<TextureHandle, TextureData, HandleHash<TextureTag>> textures;
            std::unordered_map<SkeletonHandle, ozz::unique_ptr<ozz::animation::Skeleton>, HandleHash<SkeletonTag>> skeletons;
            std::unordered_map<AnimationHandle, AnimationData, HandleHash<AnimationTag>> animations;

            // 场景数据
            std::vector<SceneNode> nodes;
            std::vector<int> root_nodes;

            // 资源句柄生成器
            struct HandleGenerator {
                uint32_t next_id = 1;
                uint32_t generation = 1;

                template<typename T>
                Handle<T> Generate() {
                    return Handle<T>{next_id++, generation};
                }
            } handle_generator;

            // 元数据
            struct Metadata {
                ozz::string source_file;
                ozz::string generator;
                ozz::string copyright;
                uint32_t version = 1;

                // 统计信息
                struct Stats {
                    uint32_t total_vertices = 0;
                    uint32_t total_triangles = 0;
                    uint32_t total_bones = 0;
                    uint32_t total_animations = 0;
                    uint64_t total_memory_bytes = 0;
                } stats;
            } metadata;

            // 验证资产完整性
            bool Validate() const;

            // 序列化
            bool Serialize(const char* output_path) const;
            bool Deserialize(const char* input_path);
        };

    } // namespace asset
} // namespace spartan


namespace spartan {
    namespace asset {
        struct UnifiedSkeletonData {
            std::set<int> unified_joints;                           // 步骤1：统一的关节集合
            std::vector<glm::mat4> global_transforms;              // 步骤2：所有节点的全局变换
            SkeletonHandle skeleton_handle;                         // 步骤3：统一骨架句柄
            std::unordered_map<int, int> gltf_node_to_ozz_joint;  // 步骤4：GLTF节点到OZZ关节的映射
            std::vector<glm::mat4> inverse_bind_matrices;         // 步骤5：逆绑定矩阵

            // 新增字段用于烘焙
            std::set<int> attachment_nodes;                        // 需要升格为骨骼的附属物节点
            std::unordered_map<int, int> attachment_to_parent;    // 附属物节点到其父骨骼的映射
            std::unordered_map<int, std::string> node_name_overrides; // 节点名称覆盖（用于规范化）

            void Clear() {
                unified_joints.clear();
                global_transforms.clear();
                skeleton_handle.Invalidate();
                gltf_node_to_ozz_joint.clear();
                inverse_bind_matrices.clear();
                attachment_nodes.clear();
                attachment_to_parent.clear();
                node_name_overrides.clear();
            }
        };

// 实现细节
        struct GltfProcessor::Impl {
            tinygltf::Model model;
            ProcessedAsset *output = nullptr;
            ProcessConfig *config = nullptr;
            ProgressCallback *progress_callback = nullptr;
            std::string *last_error = nullptr;
            std::set<int> unified_skeleton_nodes;  // 记录哪些节点在统一骨架中

            // 资源映射表
            std::unordered_map<int, MeshHandle> mesh_handle_map;
            std::unordered_map<int, MaterialHandle> material_handle_map;
            std::unordered_map<int, TextureHandle> texture_handle_map;
            std::unordered_map<int, SkeletonHandle> skeleton_handle_map;
            std::unordered_map<int, AnimationHandle> animation_handle_map;

            // 处理状态
            struct ProcessingContext {
                // 顶点去重
                struct VertexKey {
                    ozz::math::Float3 position;
                    ozz::math::Float3 normal;
                    glm::vec2 uv0;  // 使用glm::vec2代替ozz::math::Float2
                    uint32_t joints[4];
                    ozz::math::Float4 weights;

                    size_t Hash() const;

                    bool operator==(const VertexKey &other) const;
                };

                struct VertexKeyHasher {
                    size_t operator()(const VertexKey &key) const { return key.Hash(); }
                };

                std::unordered_map<VertexKey, uint32_t, VertexKeyHasher> vertex_cache;

                // 骨骼映射
                std::unordered_map<std::string, int> bone_name_to_index;
                std::vector<std::string> bone_names;

                // 错误收集
                std::vector<std::string> warnings;

                void Clear() {
                    vertex_cache.clear();
                    bone_name_to_index.clear();
                    bone_names.clear();
                    warnings.clear();
                }
            } context;

            // 主要处理函数
            bool LoadGltfFile(const char *path);

            bool ProcessTextures();

            bool ProcessMaterials();

            bool ProcessMeshes();

            bool ProcessSkeletons();

            bool ProcessAnimations();

            bool ProcessSceneNodes();

            bool ValidateAndFinalize();

            // 网格处理
            bool ProcessMesh(int mesh_index);

            bool ProcessPrimitive(MeshData &mesh_data, const tinygltf::Mesh &gltf_mesh,
                                  const tinygltf::Primitive &primitive, int node_index);

            bool ExtractVertexData(const tinygltf::Primitive &primitive,
                                   std::vector<uint8_t> &vertex_buffer,
                                   VertexFormat &format,
                                   uint32_t &vertex_count);

            // 动画处理


            // 工具函数
            bool ValidateAccessor(int accessor_index, size_t expected_component_size,
                                  const uint8_t **out_data, size_t *out_stride, size_t *out_count) const;

            template<typename T>
            std::vector<T> GetAccessorData(int accessor_index) const;

            ozz::math::Transform GetNodeTransform(const tinygltf::Node &node) const;

            VertexFormat DetermineVertexFormat(const tinygltf::Primitive &primitive) const;

            // 优化函数
            void OptimizeVertexCache(MeshData &mesh_data);

            void GenerateTangents(MeshData &mesh_data);

            // 骨骼处理
            void BuildSkeletonHierarchy(int node_index,
                                        ozz::animation::offline::RawSkeleton::Joint *joint,
                                        const std::set<int> &allowed_joints);  // 不需要模板参数

            // 错误报告
            void ReportError(const std::string &error);

            void ReportWarning(const std::string &warning);

            void ReportProgress(float progress, const char *stage);

            void CollectUnifiedJoints(UnifiedSkeletonData &data);

            void CalculateGlobalTransforms(UnifiedSkeletonData &data);

            void BuildUnifiedSkeleton(UnifiedSkeletonData &data);

            void GenerateGltfToOzzMapping(UnifiedSkeletonData &data);

            void CalculateInverseBindMatrices(UnifiedSkeletonData &data);

            void ProcessMeshesForUnifiedSkeleton(UnifiedSkeletonData &data);

            void ProcessAnimationsForUnifiedSkeleton(UnifiedSkeletonData &data);

            void ProcessPureNodeAnimations();

            glm::mat4 GetNodeLocalMatrix(const tinygltf::Node &node) const;

            void IdentifyAttachmentNodes(UnifiedSkeletonData& data);
            std::string NormalizeJointName(const std::string& name, int node_idx, const UnifiedSkeletonData& data);  // 添加data参数
            void BuildUnifiedSkeletonWithAttachments(UnifiedSkeletonData& data);
            bool BakeAndBuildAnimation(const tinygltf::Animation& gltf_anim,
                                       UnifiedSkeletonData& unified_data,
                                       AnimationHandle& out_handle);
            glm::mat4 SampleNodeTransform(const tinygltf::Animation& anim,
                                          int node_idx,
                                          float time) const;
            void ComputeSkeletonWorldMatrices(const UnifiedSkeletonData& unified_data,
                                              const std::unordered_map<int, ozz::math::Transform>& local_transforms,
                                              std::vector<glm::mat4>& world_matrices) const;

        };
    };
}