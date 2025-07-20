#include "GltfTools.h"
#include <set>

// 禁用tiny_gltf中json.hpp的警告
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-literal-operator"
#endif

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define STB_IMAGE_IMPLEMENTATION
#include "tiny_gltf.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include "ozz/animation/offline/animation_builder.h"
#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/offline/skeleton_builder.h"
#include "ozz/base/log.h"
#include "ozz/base/maths/math_ex.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <cstring>
#include <chrono>
#include <fstream>
#include <cfloat>

// GLM额外头文件
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/epsilon.hpp>

namespace spartan {
    namespace asset {
// VertexKey实现
        size_t GltfProcessor::Impl::ProcessingContext::VertexKey::Hash() const {
            size_t hash = 0;

            // FNV-1a hash
            auto hash_combine = [&hash](const void* data, size_t size) {
                const uint8_t* bytes = static_cast<const uint8_t*>(data);
                for (size_t i = 0; i < size; ++i) {
                    hash ^= bytes[i];
                    hash *= 0x100000001b3;
                }
            };

            hash_combine(&position, sizeof(position));
            hash_combine(&normal, sizeof(normal));
            hash_combine(&uv0, sizeof(uv0));
            hash_combine(joints, sizeof(joints));
            hash_combine(&weights, sizeof(weights));

            return hash;
        }

        bool GltfProcessor::Impl::ProcessingContext::VertexKey::operator==(const VertexKey& other) const {
            const float eps = 1e-6f;
            return
                    std::abs(position.x - other.position.x) < eps &&
                    std::abs(position.y - other.position.y) < eps &&
                    std::abs(position.z - other.position.z) < eps &&
                    std::abs(normal.x - other.normal.x) < eps &&
                    std::abs(normal.y - other.normal.y) < eps &&
                    std::abs(normal.z - other.normal.z) < eps &&
                    std::abs(uv0.x - other.uv0.x) < eps &&
                    std::abs(uv0.y - other.uv0.y) < eps &&
                    std::memcmp(joints, other.joints, sizeof(joints)) == 0 &&
                    std::abs(weights.x - other.weights.x) < eps &&
                    std::abs(weights.y - other.weights.y) < eps &&
                    std::abs(weights.z - other.weights.z) < eps &&
                    std::abs(weights.w - other.weights.w) < eps;
        }

// GltfProcessor实现
        GltfProcessor::GltfProcessor(const ProcessConfig& config)
                : impl_(std::make_unique<Impl>()), config_(config) {}

        GltfProcessor::~GltfProcessor() = default;


// 加载GLTF文件
        bool GltfProcessor::Impl::LoadGltfFile(const char* path) {
            tinygltf::TinyGLTF loader;
            std::string err, warn;

            // 设置自定义图像加载器（避免加载图像数据到内存）
            loader.SetImageLoader([](tinygltf::Image*, int, std::string*, std::string*,
                                     int, int, const unsigned char*, int, void*) {
                return true;
            }, nullptr);

            bool success = false;
            std::string path_str(path);

            if (path_str.find(".glb") != std::string::npos) {
                success = loader.LoadBinaryFromFile(&model, &err, &warn, path);
            } else {
                success = loader.LoadASCIIFromFile(&model, &err, &warn, path);
            }

            if (!warn.empty()) {
                ReportWarning("GLTF Warning: " + warn);
            }

            if (!err.empty()) {
                ReportError("GLTF Error: " + err);
                return false;
            }

            return success;
        }

// 处理纹理
        bool GltfProcessor::Impl::ProcessTextures() {
            for (size_t i = 0; i < model.textures.size(); ++i) {
                const auto& gltf_texture = model.textures[i];

                TextureHandle handle = output->handle_generator.Generate<TextureTag>();
                texture_handle_map[static_cast<int>(i)] = handle;

                TextureData& texture = output->textures[handle];

                if (gltf_texture.source >= 0 && gltf_texture.source < static_cast<int>(model.images.size())) {
                    const auto& image = model.images[gltf_texture.source];
                    texture.uri = image.uri.c_str();
                    texture.width = image.width;
                    texture.height = image.height;
                    texture.channels = image.component;

                    // 确定格式
                    switch (image.component) {
                        case 1: texture.format = TextureData::R8; break;
                        case 2: texture.format = TextureData::RG8; break;
                        case 3: texture.format = TextureData::RGB8; break;
                        case 4: texture.format = TextureData::RGBA8; break;
                    }

                    // 设置生成mipmap
                    texture.generate_mipmaps = config->generate_mipmaps;
                }
            }

            return true;
        }

// 处理材质
        bool GltfProcessor::Impl::ProcessMaterials() {
            for (size_t i = 0; i < model.materials.size(); ++i) {
                const auto& gltf_material = model.materials[i];

                MaterialHandle handle = output->handle_generator.Generate<MaterialTag>();
                material_handle_map[static_cast<int>(i)] = handle;

                MaterialData& material = output->materials[handle];
                material.name = gltf_material.name.c_str();

                // PBR参数
                const auto& pbr = gltf_material.pbrMetallicRoughness;
                if (pbr.baseColorFactor.size() == 4) {
                    material.base_color_factor = ozz::math::Float4(
                            static_cast<float>(pbr.baseColorFactor[0]),
                            static_cast<float>(pbr.baseColorFactor[1]),
                            static_cast<float>(pbr.baseColorFactor[2]),
                            static_cast<float>(pbr.baseColorFactor[3])
                    );
                }

                material.metallic_factor = static_cast<float>(pbr.metallicFactor);
                material.roughness_factor = static_cast<float>(pbr.roughnessFactor);

                // 纹理
                if (pbr.baseColorTexture.index >= 0) {
                    material.base_color_texture = texture_handle_map[pbr.baseColorTexture.index];
                }

                if (pbr.metallicRoughnessTexture.index >= 0) {
                    material.metallic_roughness_texture = texture_handle_map[pbr.metallicRoughnessTexture.index];
                }

                if (gltf_material.normalTexture.index >= 0) {
                    material.normal_texture = texture_handle_map[gltf_material.normalTexture.index];
                }

                if (gltf_material.occlusionTexture.index >= 0) {
                    material.occlusion_texture = texture_handle_map[gltf_material.occlusionTexture.index];
                }

                if (gltf_material.emissiveTexture.index >= 0) {
                    material.emissive_texture = texture_handle_map[gltf_material.emissiveTexture.index];
                }

                // 发光因子
                if (gltf_material.emissiveFactor.size() == 3) {
                    material.emissive_factor = ozz::math::Float3(
                            static_cast<float>(gltf_material.emissiveFactor[0]),
                            static_cast<float>(gltf_material.emissiveFactor[1]),
                            static_cast<float>(gltf_material.emissiveFactor[2])
                    );
                }

                // Alpha模式
                if (gltf_material.alphaMode == "OPAQUE") {
                    material.alpha_mode = MaterialData::MODE_OPAQUE;
                } else if (gltf_material.alphaMode == "MASK") {
                    material.alpha_mode = MaterialData::MODE_MASK;
                    material.alpha_cutoff = static_cast<float>(gltf_material.alphaCutoff);
                } else if (gltf_material.alphaMode == "BLEND") {
                    material.alpha_mode = MaterialData::MODE_BLEND;
                }

                material.double_sided = gltf_material.doubleSided;
            }

            return true;
        }

// 处理网格
        bool GltfProcessor::Impl::ProcessMeshes() {
            for (size_t i = 0; i < model.meshes.size(); ++i) {
                if (!ProcessMesh(static_cast<int>(i))) {
                    return false;
                }
            }
            return true;
        }

// 处理单个网格
        bool GltfProcessor::Impl::ProcessMesh(int mesh_index) {
            const auto& gltf_mesh = model.meshes[mesh_index];

            // 查找使用此网格的节点
            int node_index = -1;
            for (size_t i = 0; i < model.nodes.size(); ++i) {
                if (model.nodes[i].mesh == mesh_index) {
                    node_index = static_cast<int>(i);
                    break;
                }
            }

            MeshHandle handle = output->handle_generator.Generate<MeshTag>();
            mesh_handle_map[mesh_index] = handle;

            MeshData& mesh_data = output->meshes[handle];
            mesh_data.vertex_count = 0;  // 初始化顶点计数

            // 首先确定整个网格的统一顶点格式
            mesh_data.format.attributes = 0;
            for (const auto& primitive : gltf_mesh.primitives) {
                VertexFormat prim_format = DetermineVertexFormat(primitive);
                mesh_data.format.attributes |= prim_format.attributes;
            }

            // 计算最终的步长和属性图
            uint32_t current_offset = 0;
            auto& final_format = mesh_data.format;
            final_format.attribute_map.clear(); // 确保从空白状态开始

            // 定义一个辅助 lambda，用于添加属性并更新偏移量
            auto add_attribute = [&](VertexFormat::Attribute attr, uint32_t size, uint32_t type, uint32_t components, bool normalized = false) {
                if (final_format.attributes & attr) {
                    final_format.attribute_map[attr] = {current_offset, components, type, normalized};
                    current_offset += size;
                }
            };

            add_attribute(VertexFormat::POSITION, sizeof(float) * 3, GL_FLOAT, 3);
            add_attribute(VertexFormat::NORMAL,   sizeof(float) * 3, GL_FLOAT, 3);
            add_attribute(VertexFormat::TANGENT,  sizeof(float) * 4, GL_FLOAT, 4);
            add_attribute(VertexFormat::UV0,      sizeof(float) * 2, GL_FLOAT, 2);
            add_attribute(VertexFormat::UV1,      sizeof(float) * 2, GL_FLOAT, 2);
            add_attribute(VertexFormat::COLOR0,   sizeof(float) * 4, GL_FLOAT, 4);
            add_attribute(VertexFormat::JOINTS0,  sizeof(uint16_t) * 4, GL_UNSIGNED_SHORT, 4);
            add_attribute(VertexFormat::WEIGHTS0, sizeof(float) * 4, GL_FLOAT, 4);

            final_format.stride = current_offset;

            // 处理每个primitive
            for (const auto& primitive : gltf_mesh.primitives) {
                if (!ProcessPrimitive(mesh_data, gltf_mesh, primitive, node_index)) {
                    return false;
                }
            }

            // 优化网格
            if (config->optimize_vertex_cache) {
                OptimizeVertexCache(mesh_data);
            }

            // 生成切线
            if (config->generate_tangents &&
                (mesh_data.format.attributes & VertexFormat::TANGENT) == 0) {
                GenerateTangents(mesh_data);
            }

            // 更新统计信息
            output->metadata.stats.total_vertices += mesh_data.vertex_count;
            output->metadata.stats.total_triangles += static_cast<uint32_t>(mesh_data.index_buffer.size() / 3);

            return true;
        }

// 处理骨骼

// 构建骨骼层级
        void GltfProcessor::Impl::BuildSkeletonHierarchy(int node_index,
                                                         ozz::animation::offline::RawSkeleton::Joint* joint,
                                                         const std::set<int>& allowed_joints) {
            const auto& node = model.nodes[node_index];

            // 确保节点名称不为空
            if (node.name.empty()) {
                joint->name = ("Joint_" + std::to_string(node_index)).c_str();
                std::cout << "节点 " << node_index << " 名称为空，使用默认名称: " << joint->name << std::endl;
            } else {
                joint->name = node.name.c_str();
                std::cout << "节点 " << node_index << " 名称: " << joint->name << std::endl;
            }

            joint->transform = GetNodeTransform(node);

            // 输出变换信息
            std::cout << "节点 " << node_index << " 变换:" << std::endl;
            std::cout << "  平移: (" << joint->transform.translation.x << ", "
                      << joint->transform.translation.y << ", " << joint->transform.translation.z << ")" << std::endl;
            std::cout << "  旋转: (" << joint->transform.rotation.x << ", "
                      << joint->transform.rotation.y << ", " << joint->transform.rotation.z << ", "
                      << joint->transform.rotation.w << ")" << std::endl;

            // 递归处理子节点
            for (int child_idx : node.children) {
                if (allowed_joints.count(child_idx)) {
                    std::cout << "添加子节点 " << child_idx << " 到节点 " << node_index << std::endl;
                    joint->children.resize(joint->children.size() + 1);
                    BuildSkeletonHierarchy(child_idx, &joint->children.back(), allowed_joints);
                } else {
                    std::cout << "跳过非骨骼子节点 " << child_idx << std::endl;
                }
            }
        }

// 处理动画
        // GltfTools.cpp




// 验证和完成
        bool GltfProcessor::Impl::ValidateAndFinalize() {
            // 验证网格数据
            for (auto& [handle, mesh] : output->meshes) {
                if (mesh.vertex_count == 0) {
                    ReportError("Mesh has no vertices");
                    return false;
                }

                if (mesh.index_buffer.empty()) {
                    ReportError("Mesh has no indices");
                    return false;
                }
            }

            // 验证动画
            for (const auto& [handle, anim] : output->animations) {
                if (anim.duration <= 0.0f) {
                    return false;
                }
            }

            // 计算总内存使用
            uint64_t total_memory = 0;
            for (const auto& [handle, mesh] : output->meshes) {
                total_memory += mesh.vertex_buffer.size();
                total_memory += mesh.index_buffer.size() * sizeof(uint32_t);
            }

            output->metadata.stats.total_memory_bytes = total_memory;

            // 输出警告
            for (const auto& warning : context.warnings) {
                ozz::log::Log() << "Warning: " << warning << std::endl;
            }

            return true;
        }

// 验证访问器
        bool GltfProcessor::Impl::ValidateAccessor(int accessor_index, size_t expected_component_size,
                                                   const uint8_t** out_data, size_t* out_stride, size_t* out_count) const {
            if (accessor_index < 0 || accessor_index >= static_cast<int>(model.accessors.size())) {
                return false;
            }

            const auto& accessor = model.accessors[accessor_index];

            // 验证buffer view
            if (accessor.bufferView < 0 || accessor.bufferView >= static_cast<int>(model.bufferViews.size())) {
                ozz::log::Log() << "Warning: Invalid buffer view index in accessor" << std::endl;
                return false;
            }

            const auto& buffer_view = model.bufferViews[accessor.bufferView];

            // 验证buffer
            if (buffer_view.buffer < 0 || buffer_view.buffer >= static_cast<int>(model.buffers.size())) {
                ozz::log::Log() << "Warning: Invalid buffer index in buffer view" << std::endl;
                return false;
            }

            const auto& buffer = model.buffers[buffer_view.buffer];

            // 计算数据范围
            size_t data_start = buffer_view.byteOffset + accessor.byteOffset;
            size_t stride = buffer_view.byteStride ? buffer_view.byteStride : expected_component_size;
            size_t data_end = data_start + accessor.count * stride;

            // 验证数据范围
            if (data_end > buffer.data.size()) {
                ozz::log::Log() << "Warning: Accessor data exceeds buffer size" << std::endl;
                return false;
            }

            if (out_data) *out_data = buffer.data.data() + data_start;
            if (out_stride) *out_stride = stride;
            if (out_count) *out_count = accessor.count;

            return true;
        }
// 获取访问器数据
        template<typename T>
        std::vector<T> GltfProcessor::Impl::GetAccessorData(int accessor_index) const {
            const uint8_t* data;
            size_t stride, count;

            if (!ValidateAccessor(accessor_index, sizeof(T), &data, &stride, &count)) {
                return {};
            }

            std::vector<T> result(count);
            for (size_t i = 0; i < count; ++i) {
                std::memcpy(&result[i], data + i * stride, sizeof(T));
            }

            return result;
        }
// 模板特化实现
        template<>
        std::vector<float> GltfProcessor::Impl::GetAccessorData<float>(int accessor_index) const {
            const uint8_t* data;
            size_t stride, count;

            if (!ValidateAccessor(accessor_index, sizeof(float), &data, &stride, &count)) {
                return {};
            }

            std::vector<float> result(count);
            for (size_t i = 0; i < count; ++i) {
                std::memcpy(&result[i], data + i * stride, sizeof(float));
            }

            return result;
        }

        template<>
        std::vector<glm::vec2> GltfProcessor::Impl::GetAccessorData<glm::vec2>(int accessor_index) const {
            const uint8_t* data;
            size_t stride, count;

            if (!ValidateAccessor(accessor_index, sizeof(glm::vec2), &data, &stride, &count)) {
                return {};
            }

            std::vector<glm::vec2> result(count);
            for (size_t i = 0; i < count; ++i) {
                std::memcpy(&result[i], data + i * stride, sizeof(glm::vec2));
            }

            return result;
        }

        template<>
        std::vector<glm::vec3> GltfProcessor::Impl::GetAccessorData<glm::vec3>(int accessor_index) const {
            const uint8_t* data;
            size_t stride, count;

            if (!ValidateAccessor(accessor_index, sizeof(glm::vec3), &data, &stride, &count)) {
                return {};
            }

            std::vector<glm::vec3> result(count);
            for (size_t i = 0; i < count; ++i) {
                std::memcpy(&result[i], data + i * stride, sizeof(glm::vec3));
            }

            return result;
        }

        template<>
        std::vector<glm::vec4> GltfProcessor::Impl::GetAccessorData<glm::vec4>(int accessor_index) const {
            const uint8_t* data;
            size_t stride, count;

            if (!ValidateAccessor(accessor_index, sizeof(glm::vec4), &data, &stride, &count)) {
                return {};
            }

            std::vector<glm::vec4> result(count);
            for (size_t i = 0; i < count; ++i) {
                std::memcpy(&result[i], data + i * stride, sizeof(glm::vec4));
            }

            return result;
        }

        template<>
        std::vector<glm::u16vec4> GltfProcessor::Impl::GetAccessorData<glm::u16vec4>(int accessor_index) const {
            if (accessor_index < 0 || accessor_index >= static_cast<int>(model.accessors.size())) {
                return {};
            }

            const auto& accessor = model.accessors[accessor_index];
            const uint8_t* data;
            size_t stride, count;

            // 对于joints数据，可能是uint8或uint16
            size_t component_size = (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                                    ? sizeof(glm::u16vec4) : sizeof(uint8_t) * 4;

            if (!ValidateAccessor(accessor_index, component_size, &data, &stride, &count)) {
                return {};
            }

            std::vector<glm::u16vec4> result(count);
            for (size_t i = 0; i < count; ++i) {
                if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    std::memcpy(&result[i], data + i * stride, sizeof(glm::u16vec4));
                } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                    // 转换 uint8 到 uint16
                    const uint8_t* joints8 = data + i * stride;
                    result[i] = glm::u16vec4(joints8[0], joints8[1], joints8[2], joints8[3]);
                }
            }

            return result;
        }

        template<>
        std::vector<glm::mat4> GltfProcessor::Impl::GetAccessorData<glm::mat4>(int accessor_index) const {
            const uint8_t* data;
            size_t stride, count;

            if (!ValidateAccessor(accessor_index, sizeof(float) * 16, &data, &stride, &count)) {
                return {};
            }

            std::vector<glm::mat4> result(count);
            for (size_t i = 0; i < count; ++i) {
                float matrix_data[16];
                std::memcpy(matrix_data, data + i * stride, sizeof(matrix_data));
                result[i] = asset::ToGLM(matrix_data);
            }

            return result;
        }


// 获取节点变换
        ozz::math::Transform GltfProcessor::Impl::GetNodeTransform(const tinygltf::Node& node) const {
            ozz::math::Transform transform;
            // 初始化为单位变换
            transform.translation = ozz::math::Float3(0.0f, 0.0f, 0.0f);
            transform.rotation = ozz::math::Quaternion(0.0f, 0.0f, 0.0f, 1.0f);  // 单位四元数
            transform.scale = ozz::math::Float3(1.0f, 1.0f, 1.0f);

            if (!node.matrix.empty()) {
                // 从矩阵提取变换
                float matrix[16];
                for (int i = 0; i < 16; ++i) {
                    matrix[i] = static_cast<float>(node.matrix[i]);
                }

                // 从4x4矩阵提取变换
                glm::mat4 glm_matrix = asset::ToGLM(matrix);

                // 分解矩阵到TRS
                glm::vec3 scale;
                glm::quat rotation;
                glm::vec3 translation;
                glm::vec3 skew;
                glm::vec4 perspective;

                if (glm::decompose(glm_matrix, scale, rotation, translation, skew, perspective)) {
                    transform.translation = asset::ToOZZ(translation);
                    transform.rotation = asset::ToOZZ(rotation);
                    transform.scale = asset::ToOZZ(scale);
                }
            } else {
                // 从TRS获取变换
                if (!node.translation.empty()) {
                    transform.translation = ozz::math::Float3(
                            static_cast<float>(node.translation[0]),
                            static_cast<float>(node.translation[1]),
                            static_cast<float>(node.translation[2])
                    );
                }

                if (!node.rotation.empty()) {
                    // 1. 先用glm::quat来读取和归一化
                    glm::quat q(
                            static_cast<float>(node.rotation[3]), // w
                            static_cast<float>(node.rotation[0]), // x
                            static_cast<float>(node.rotation[1]), // y
                            static_cast<float>(node.rotation[2])  // z
                    );
                    q = glm::normalize(q);

                    // 2. 再将归一化后的值赋给OZZ的四元数
                    transform.rotation = ozz::math::Quaternion(q.x, q.y, q.z, q.w);
                }

                if (!node.scale.empty()) {
                    transform.scale = ozz::math::Float3(
                            static_cast<float>(node.scale[0]),
                            static_cast<float>(node.scale[1]),
                            static_cast<float>(node.scale[2])
                    );
                }
            }

            return transform;
        }

// 错误报告
        void GltfProcessor::Impl::ReportError(const std::string& error) {
            if (last_error) {
                *last_error = error;
            }
            ozz::log::Err() << error << std::endl;
        }

        void GltfProcessor::Impl::ReportWarning(const std::string& warning) {
            context.warnings.push_back(warning);
        }

        void GltfProcessor::Impl::ReportProgress(float progress, const char* stage) {
            if (progress_callback && *progress_callback) {
                (*progress_callback)(progress, stage);
            }
        }

// ProcessedAsset验证
        bool ProcessedAsset::Validate() const {
            // 验证网格
            for (const auto& [handle, mesh] : meshes) {
                if (mesh.vertex_count == 0 || mesh.index_buffer.empty()) {
                    return false;
                }

                // 验证索引范围
                for (uint32_t idx : mesh.index_buffer) {
                    if (idx >= mesh.vertex_count) {
                        return false;
                    }
                }
            }

            // 验证动画
            for (const auto& [handle, anim] : animations) {
                if (anim.duration <= 0.0f) {
                    return false;
                }
            }

            return true;
        }

        bool GltfProcessor::Impl::ProcessPrimitive(MeshData& mesh_data, const tinygltf::Mesh& gltf_mesh,
                                                   const tinygltf::Primitive& primitive, int node_index) {
            // 创建子网格
            MeshData::SubMesh submesh;
            submesh.index_offset = static_cast<uint32_t>(mesh_data.index_buffer.size());
            submesh.base_vertex = mesh_data.vertex_count;  // 记录基础顶点偏移

            // 提取原始顶点数据
            std::vector<uint8_t> raw_vertex_buffer;
            // 在这里调用 DetermineVertexFormat 来获取当前图元的正确格式
            VertexFormat raw_format = DetermineVertexFormat(primitive);
            uint32_t raw_vertex_count;

            if (!ExtractVertexData(primitive, raw_vertex_buffer, raw_format, raw_vertex_count)) {
                return false;
            }

            // 确保格式包含网格统一格式的所有属性
            raw_format.attributes = mesh_data.format.attributes;

            // 清空当前primitive的顶点缓存
            context.vertex_cache.clear();

            // 构建去重后的顶点数据和索引映射
            std::vector<uint32_t> index_remap(raw_vertex_count);
            std::vector<uint8_t> deduplicated_vertices;
            uint32_t deduplicated_count = 0;

            for (uint32_t i = 0; i < raw_vertex_count; ++i) {
                const uint8_t* vertex_data = raw_vertex_buffer.data() + i * raw_format.stride;

                // 构建顶点键
                ProcessingContext::VertexKey key;

                // 提取位置
                if (raw_format.attributes & VertexFormat::POSITION) {
                    const auto& info = raw_format.attribute_map.at(VertexFormat::POSITION);
                    glm::vec3 pos;
                    std::memcpy(&pos, vertex_data + info.offset, sizeof(glm::vec3));
                    key.position = ToOZZ(pos);
                } else {
                    key.position = ozz::math::Float3(0.0f, 0.0f, 0.0f);
                }

                // 提取法线
                if (raw_format.attributes & VertexFormat::NORMAL) {
                    const auto& info = raw_format.attribute_map.at(VertexFormat::NORMAL);
                    glm::vec3 normal;
                    std::memcpy(&normal, vertex_data + info.offset, sizeof(glm::vec3));
                    key.normal = ToOZZ(normal);
                } else {
                    key.normal = ozz::math::Float3(0.0f, 1.0f, 0.0f);
                }

                // 提取UV
                if (raw_format.attributes & VertexFormat::UV0) {
                    const auto& info = raw_format.attribute_map.at(VertexFormat::UV0);
                    glm::vec2 uv;
                    std::memcpy(&uv, vertex_data + info.offset, sizeof(glm::vec2));
                    key.uv0 = uv;
                } else {
                    key.uv0 = glm::vec2(0.0f, 0.0f);
                }

                // 提取蒙皮数据
                if (raw_format.attributes & VertexFormat::JOINTS0) {
                    const auto& info = raw_format.attribute_map.at(VertexFormat::JOINTS0);
                    glm::u16vec4 joints;
                    std::memcpy(&joints, vertex_data + info.offset, sizeof(glm::u16vec4));
                    key.joints[0] = joints.x;
                    key.joints[1] = joints.y;
                    key.joints[2] = joints.z;
                    key.joints[3] = joints.w;
                } else {
                    key.joints[0] = key.joints[1] = key.joints[2] = key.joints[3] = 0;
                }

                if (raw_format.attributes & VertexFormat::WEIGHTS0) {
                    const auto& info = raw_format.attribute_map.at(VertexFormat::WEIGHTS0);
                    glm::vec4 weights;
                    std::memcpy(&weights, vertex_data + info.offset, sizeof(glm::vec4));
                    key.weights = ozz::math::Float4(weights.x, weights.y, weights.z, weights.w);
                } else {
                    key.weights = ozz::math::Float4(1.0f, 0.0f, 0.0f, 0.0f);
                }

                // 查找或插入顶点
                auto it = context.vertex_cache.find(key);
                if (it != context.vertex_cache.end()) {
                    // 找到重复顶点
                    index_remap[i] = it->second;
                } else {
                    // 新顶点
                    index_remap[i] = deduplicated_count;
                    context.vertex_cache[key] = deduplicated_count;

                    // 添加到去重后的缓冲区
                    size_t current_size = deduplicated_vertices.size();
                    deduplicated_vertices.resize(current_size + raw_format.stride);
                    std::memcpy(deduplicated_vertices.data() + current_size, vertex_data, raw_format.stride);

                    deduplicated_count++;
                }
            }

            // 更新网格顶点缓冲区
            size_t vertex_buffer_offset = mesh_data.vertex_buffer.size();
            mesh_data.vertex_buffer.insert(mesh_data.vertex_buffer.end(),
                                           deduplicated_vertices.begin(),
                                           deduplicated_vertices.end());

            // 更新顶点数
            uint32_t primitive_vertex_count = deduplicated_count;
            mesh_data.vertex_count += primitive_vertex_count;

            // 提取并重映射索引数据
            if (primitive.indices >= 0) {
                const auto& accessor = model.accessors[primitive.indices];
                const auto& buffer_view = model.bufferViews[accessor.bufferView];
                const auto& buffer = model.buffers[buffer_view.buffer];

                const uint8_t* data = buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset;
                uint32_t index_count = static_cast<uint32_t>(accessor.count);

                // 预留空间
                size_t old_size = mesh_data.index_buffer.size();
                mesh_data.index_buffer.resize(old_size + index_count);

                // 根据索引类型复制并重映射数据
                if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    const uint16_t* indices16 = reinterpret_cast<const uint16_t*>(data);
                    for (uint32_t i = 0; i < index_count; ++i) {
                        uint32_t original_index = indices16[i];
                        mesh_data.index_buffer[old_size + i] = index_remap[original_index] + submesh.base_vertex;
                    }
                } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                    const uint32_t* indices32 = reinterpret_cast<const uint32_t*>(data);
                    for (uint32_t i = 0; i < index_count; ++i) {
                        uint32_t original_index = indices32[i];
                        mesh_data.index_buffer[old_size + i] = index_remap[original_index] + submesh.base_vertex;
                    }
                } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                    const uint8_t* indices8 = data;
                    for (uint32_t i = 0; i < index_count; ++i) {
                        uint32_t original_index = indices8[i];
                        mesh_data.index_buffer[old_size + i] = index_remap[original_index] + submesh.base_vertex;
                    }
                }

                submesh.index_count = index_count;
            } else {
                // 如果没有索引，生成索引
                size_t old_size = mesh_data.index_buffer.size();
                mesh_data.index_buffer.resize(old_size + raw_vertex_count);

                for (uint32_t i = 0; i < raw_vertex_count; ++i) {
                    mesh_data.index_buffer[old_size + i] = index_remap[i] + submesh.base_vertex;
                }

                submesh.index_count = raw_vertex_count;
            }

            // 设置材质
            if (primitive.material >= 0) {
                auto mat_it = material_handle_map.find(primitive.material);
                if (mat_it != material_handle_map.end()) {
                    submesh.material = mat_it->second;
                }
            }

            // 计算包围盒（使用去重后的顶点）
            if (raw_format.attributes & VertexFormat::POSITION) {
                const auto& pos_info = raw_format.attribute_map.at(VertexFormat::POSITION);
                bool first = true;

                for (uint32_t i = 0; i < primitive_vertex_count; ++i) {
                    const uint8_t* vertex = deduplicated_vertices.data() + i * raw_format.stride;
                    glm::vec3 pos;
                    std::memcpy(&pos, vertex + pos_info.offset, sizeof(glm::vec3));
                    ozz::math::Float3 p = ToOZZ(pos);

                    if (first) {
                        submesh.aabb_min = submesh.aabb_max = p;
                        first = false;
                    } else {
                        submesh.aabb_min.x = std::min(submesh.aabb_min.x, p.x);
                        submesh.aabb_min.y = std::min(submesh.aabb_min.y, p.y);
                        submesh.aabb_min.z = std::min(submesh.aabb_min.z, p.z);
                        submesh.aabb_max.x = std::max(submesh.aabb_max.x, p.x);
                        submesh.aabb_max.y = std::max(submesh.aabb_max.y, p.y);
                        submesh.aabb_max.z = std::max(submesh.aabb_max.z, p.z);
                    }
                }
            }

            mesh_data.submeshes.push_back(submesh);

            // 处理蒙皮信息
            if (node_index >= 0 && model.nodes[node_index].skin >= 0) {
                const auto& skin = model.skins[model.nodes[node_index].skin];

                auto skel_it = skeleton_handle_map.find(model.nodes[node_index].skin);
                if (skel_it != skeleton_handle_map.end()) {
                    mesh_data.skeleton = skel_it->second;

                    // 提取逆绑定矩阵
                    if (skin.inverseBindMatrices >= 0) {
                        mesh_data.inverse_bind_poses = GetAccessorData<glm::mat4>(skin.inverseBindMatrices);
                    }
                }
            }

            // 输出去重统计
            float dedup_ratio = raw_vertex_count > 0 ?
                                (1.0f - static_cast<float>(primitive_vertex_count) / raw_vertex_count) * 100.0f : 0.0f;

            ozz::log::Out() << "Primitive deduplication: " << raw_vertex_count << " -> "
                            << primitive_vertex_count << " vertices ("
                            << dedup_ratio << "% reduction)" << std::endl;

            return true;
        }

        bool GltfProcessor::Impl::ExtractVertexData(const tinygltf::Primitive& primitive,
                                                    std::vector<uint8_t>& vertex_buffer,
                                                    VertexFormat& format,
                                                    uint32_t& vertex_count) {
            // 获取位置数据以确定顶点数
            auto pos_it = primitive.attributes.find("POSITION");
            if (pos_it == primitive.attributes.end()) {
                ReportError("Primitive missing POSITION attribute");
                return false;
            }

            const auto& pos_accessor = model.accessors[pos_it->second];
            vertex_count = static_cast<uint32_t>(pos_accessor.count);

            // 计算顶点大小和偏移
            uint32_t vertex_size = 0;
            format.attribute_map.clear();

            // 位置 (vec3)
            if (format.attributes & VertexFormat::POSITION) {
                format.attribute_map[VertexFormat::POSITION] = {vertex_size, 3, GL_FLOAT, false};
                vertex_size += sizeof(float) * 3;
            }

            // 法线 (vec3)
            if (format.attributes & VertexFormat::NORMAL) {
                format.attribute_map[VertexFormat::NORMAL] = {vertex_size, 3, GL_FLOAT, false};
                vertex_size += sizeof(float) * 3;
            }

            // 切线 (vec4)
            if (format.attributes & VertexFormat::TANGENT) {
                format.attribute_map[VertexFormat::TANGENT] = {vertex_size, 4, GL_FLOAT, false};
                vertex_size += sizeof(float) * 4;
            }

            // UV0 (vec2)
            if (format.attributes & VertexFormat::UV0) {
                format.attribute_map[VertexFormat::UV0] = {vertex_size, 2, GL_FLOAT, false};
                vertex_size += sizeof(float) * 2;
            }

            // UV1 (vec2)
            if (format.attributes & VertexFormat::UV1) {
                format.attribute_map[VertexFormat::UV1] = {vertex_size, 2, GL_FLOAT, false};
                vertex_size += sizeof(float) * 2;
            }

            // 颜色 (vec4)
            if (format.attributes & VertexFormat::COLOR0) {
                format.attribute_map[VertexFormat::COLOR0] = {vertex_size, 4, GL_FLOAT, false};
                vertex_size += sizeof(float) * 4;
            }

            // 关节 (uvec4)
            if (format.attributes & VertexFormat::JOINTS0) {
                format.attribute_map[VertexFormat::JOINTS0] = {vertex_size, 4, GL_UNSIGNED_SHORT, false};
                vertex_size += sizeof(uint16_t) * 4;
            }

            // 权重 (vec4)
            if (format.attributes & VertexFormat::WEIGHTS0) {
                format.attribute_map[VertexFormat::WEIGHTS0] = {vertex_size, 4, GL_FLOAT, false};
                vertex_size += sizeof(float) * 4;
            }

            format.stride = vertex_size;

            // 分配顶点缓冲区
            vertex_buffer.clear();
            vertex_buffer.resize(vertex_count * vertex_size);
            uint8_t* vertex_data = vertex_buffer.data();

            // 提取各个属性数据
            auto positions = GetAccessorData<glm::vec3>(pos_it->second);

            std::vector<glm::vec3> normals;
            if (auto it = primitive.attributes.find("NORMAL"); it != primitive.attributes.end()) {
                normals = GetAccessorData<glm::vec3>(it->second);
            }

            std::vector<glm::vec4> tangents;
            if (auto it = primitive.attributes.find("TANGENT"); it != primitive.attributes.end()) {
                tangents = GetAccessorData<glm::vec4>(it->second);
            }

            std::vector<glm::vec2> uvs0;
            if (auto it = primitive.attributes.find("TEXCOORD_0"); it != primitive.attributes.end()) {
                uvs0 = GetAccessorData<glm::vec2>(it->second);
            }

            std::vector<glm::vec2> uvs1;
            if (auto it = primitive.attributes.find("TEXCOORD_1"); it != primitive.attributes.end()) {
                uvs1 = GetAccessorData<glm::vec2>(it->second);
            }

            std::vector<glm::vec4> colors;
            if (auto it = primitive.attributes.find("COLOR_0"); it != primitive.attributes.end()) {
                colors = GetAccessorData<glm::vec4>(it->second);
            }

            std::vector<glm::u16vec4> joints;
            if (auto it = primitive.attributes.find("JOINTS_0"); it != primitive.attributes.end()) {
                joints = GetAccessorData<glm::u16vec4>(it->second);
            }

            std::vector<glm::vec4> weights;
            if (auto it = primitive.attributes.find("WEIGHTS_0"); it != primitive.attributes.end()) {
                weights = GetAccessorData<glm::vec4>(it->second);
            }

            // 交错存储顶点数据
            for (uint32_t i = 0; i < vertex_count; ++i) {
                uint8_t* vertex = vertex_data + i * vertex_size;
                uint32_t offset = 0;

                if (format.attributes & VertexFormat::POSITION) {
                    std::memcpy(vertex + offset, &positions[i], sizeof(glm::vec3));
                    offset += sizeof(glm::vec3);
                }

                if (format.attributes & VertexFormat::NORMAL) {
                    if (i < normals.size()) {
                        std::memcpy(vertex + offset, &normals[i], sizeof(glm::vec3));
                    } else {
                        glm::vec3 default_normal(0, 1, 0);
                        std::memcpy(vertex + offset, &default_normal, sizeof(glm::vec3));
                    }
                    offset += sizeof(glm::vec3);
                }

                if (format.attributes & VertexFormat::TANGENT) {
                    if (i < tangents.size()) {
                        std::memcpy(vertex + offset, &tangents[i], sizeof(glm::vec4));
                    } else {
                        glm::vec4 default_tangent(1, 0, 0, 1);
                        std::memcpy(vertex + offset, &default_tangent, sizeof(glm::vec4));
                    }
                    offset += sizeof(glm::vec4);
                }

                if (format.attributes & VertexFormat::UV0) {
                    if (i < uvs0.size()) {
                        std::memcpy(vertex + offset, &uvs0[i], sizeof(glm::vec2));
                    } else {
                        glm::vec2 default_uv(0, 0);
                        std::memcpy(vertex + offset, &default_uv, sizeof(glm::vec2));
                    }
                    offset += sizeof(glm::vec2);
                }

                if (format.attributes & VertexFormat::UV1) {
                    if (i < uvs1.size()) {
                        std::memcpy(vertex + offset, &uvs1[i], sizeof(glm::vec2));
                    } else {
                        glm::vec2 default_uv(0, 0);
                        std::memcpy(vertex + offset, &default_uv, sizeof(glm::vec2));
                    }
                    offset += sizeof(glm::vec2);
                }

                if (format.attributes & VertexFormat::COLOR0) {
                    if (i < colors.size()) {
                        std::memcpy(vertex + offset, &colors[i], sizeof(glm::vec4));
                    } else {
                        glm::vec4 default_color(1, 1, 1, 1);
                        std::memcpy(vertex + offset, &default_color, sizeof(glm::vec4));
                    }
                    offset += sizeof(glm::vec4);
                }

                if (format.attributes & VertexFormat::JOINTS0) {
                    if (i < joints.size()) {
                        std::memcpy(vertex + offset, &joints[i], sizeof(glm::u16vec4));
                    } else {
                        glm::u16vec4 default_joints(0, 0, 0, 0);
                        std::memcpy(vertex + offset, &default_joints, sizeof(glm::u16vec4));
                    }
                    offset += sizeof(glm::u16vec4);
                }

                if (format.attributes & VertexFormat::WEIGHTS0) {
                    if (i < weights.size()) {
                        // 归一化权重
                        glm::vec4 w = weights[i];
                        float sum = w.x + w.y + w.z + w.w;
                        if (sum > 0.0f) {
                            w /= sum;
                        }
                        std::memcpy(vertex + offset, &w, sizeof(glm::vec4));
                    } else {
                        glm::vec4 default_weights(1, 0, 0, 0);
                        std::memcpy(vertex + offset, &default_weights, sizeof(glm::vec4));
                    }
                    offset += sizeof(glm::vec4);
                }
            }

            return true;
        }

        VertexFormat GltfProcessor::Impl::DetermineVertexFormat(const tinygltf::Primitive& primitive) const {
            VertexFormat format;
            format.attributes = 0;

            // 检查各个属性是否存在
            if (primitive.attributes.find("POSITION") != primitive.attributes.end()) {
                format.attributes |= VertexFormat::POSITION;
            }

            if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
                format.attributes |= VertexFormat::NORMAL;
            }

            if (primitive.attributes.find("TANGENT") != primitive.attributes.end()) {
                format.attributes |= VertexFormat::TANGENT;
            }

            if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
                format.attributes |= VertexFormat::UV0;
            }

            if (primitive.attributes.find("TEXCOORD_1") != primitive.attributes.end()) {
                format.attributes |= VertexFormat::UV1;
            }

            if (primitive.attributes.find("COLOR_0") != primitive.attributes.end()) {
                format.attributes |= VertexFormat::COLOR0;
            }

            if (primitive.attributes.find("JOINTS_0") != primitive.attributes.end()) {
                format.attributes |= VertexFormat::JOINTS0;
            }

            if (primitive.attributes.find("WEIGHTS_0") != primitive.attributes.end()) {
                format.attributes |= VertexFormat::WEIGHTS0;
            }

            return format;
        }

        void GltfProcessor::Impl::OptimizeVertexCache(MeshData& mesh_data) {
            if (mesh_data.index_buffer.empty()) {
                return;
            }

            // Forsyth算法优化顶点缓存
            struct VertexScore {
                int cache_position = -1;
                float score = 0.0f;
                uint32_t not_added_triangles = 0;
            };

            std::vector<VertexScore> vertex_scores(mesh_data.vertex_count);
            std::vector<uint32_t> optimized_indices;
            optimized_indices.reserve(mesh_data.index_buffer.size());

            // 统计每个顶点被多少三角形使用
            for (size_t i = 0; i < mesh_data.index_buffer.size(); i += 3) {
                for (int j = 0; j < 3; ++j) {
                    vertex_scores[mesh_data.index_buffer[i + j]].not_added_triangles++;
                }
            }

            // LRU缓存模拟
            const int CACHE_SIZE = 32;
            std::vector<int> cache_timestamps(mesh_data.vertex_count, -1);
            int timestamp = 0;

            // 计算顶点分数
            auto calculate_vertex_score = [&](uint32_t vertex) -> float {
                const float CACHE_DECAY_POWER = 1.5f;
                const float LAST_TRI_SCORE = 0.75f;
                const float VALENCE_BOOST_SCALE = 2.0f;
                const float VALENCE_BOOST_POWER = 0.5f;

                auto& vs = vertex_scores[vertex];

                if (vs.not_added_triangles == 0) {
                    return -1.0f;
                }

                float score = 0.0f;
                int cache_position = vs.cache_position;

                if (cache_position >= 0) {
                    if (cache_position < 3) {
                        score = LAST_TRI_SCORE;
                    } else {
                        const float scale = 1.0f / (CACHE_SIZE - 3);
                        score = 1.0f - (cache_position - 3) * scale;
                        score = std::pow(score, CACHE_DECAY_POWER);
                    }
                }

                float valence_boost = std::pow(vs.not_added_triangles, -VALENCE_BOOST_POWER);
                score += VALENCE_BOOST_SCALE * valence_boost;

                return score;
            };

            // 优化主循环
            std::vector<bool> triangle_added(mesh_data.index_buffer.size() / 3, false);

            // 找到起始三角形（选择顶点分数最低的）
            uint32_t best_triangle = 0;
            float best_score = FLT_MAX;

            for (size_t i = 0; i < mesh_data.index_buffer.size(); i += 3) {
                float score = 0.0f;
                for (int j = 0; j < 3; ++j) {
                    score += calculate_vertex_score(mesh_data.index_buffer[i + j]);
                }

                if (score < best_score) {
                    best_score = score;
                    best_triangle = static_cast<uint32_t>(i / 3);
                }
            }

            // 处理所有三角形
            std::vector<int> cache;

            while (optimized_indices.size() < mesh_data.index_buffer.size()) {
                // 添加当前最佳三角形
                uint32_t tri_idx = best_triangle * 3;
                for (int i = 0; i < 3; ++i) {
                    uint32_t idx = mesh_data.index_buffer[tri_idx + i];
                    optimized_indices.push_back(idx);

                    // 更新缓存
                    auto it = std::find(cache.begin(), cache.end(), idx);
                    if (it != cache.end()) {
                        cache.erase(it);
                    }
                    cache.insert(cache.begin(), idx);
                    if (cache.size() > CACHE_SIZE) {
                        cache.resize(CACHE_SIZE);
                    }

                    // 更新缓存位置
                    for (size_t j = 0; j < cache.size(); ++j) {
                        vertex_scores[cache[j]].cache_position = static_cast<int>(j);
                    }

                    vertex_scores[idx].not_added_triangles--;
                }

                triangle_added[best_triangle] = true;

                // 找下一个最佳三角形
                best_triangle = UINT32_MAX;
                best_score = -1.0f;

                // 检查缓存中顶点相邻的三角形
                for (int idx : cache) {
                    // 查找使用此顶点的三角形
                    for (size_t i = 0; i < mesh_data.index_buffer.size(); i += 3) {
                        if (triangle_added[i / 3]) continue;

                        bool uses_vertex = false;
                        for (int j = 0; j < 3; ++j) {
                            if (mesh_data.index_buffer[i + j] == static_cast<uint32_t>(idx)) {
                                uses_vertex = true;
                                break;
                            }
                        }

                        if (uses_vertex) {
                            float score = 0.0f;
                            for (int j = 0; j < 3; ++j) {
                                score += calculate_vertex_score(mesh_data.index_buffer[i + j]);
                            }

                            if (score > best_score) {
                                best_score = score;
                                best_triangle = static_cast<uint32_t>(i / 3);
                            }
                        }
                    }
                }

                // 如果没找到，选择任意未处理的三角形
                if (best_triangle == UINT32_MAX) {
                    for (size_t i = 0; i < triangle_added.size(); ++i) {
                        if (!triangle_added[i]) {
                            best_triangle = static_cast<uint32_t>(i);
                            break;
                        }
                    }
                }
            }

            // 替换原索引缓冲区
            mesh_data.index_buffer = std::move(optimized_indices);
        }

        void GltfProcessor::Impl::GenerateTangents(MeshData& mesh_data) {
            // 步骤1: 检查需要生成哪些数据 (法线? 切线?)
            bool needs_normals = (mesh_data.format.attributes & VertexFormat::NORMAL) == 0;
            bool needs_tangents = (mesh_data.format.attributes & VertexFormat::TANGENT) == 0;

            // 如果数据已经齐全，则无需任何操作
            if (!needs_normals && !needs_tangents) {
                return;
            }

            // 检查生成数据的先决条件：必须要有顶点位置
            if ((mesh_data.format.attributes & VertexFormat::POSITION) == 0) {
                ReportWarning("Cannot generate normals/tangents without POSITION attribute.");
                return;
            }

            // 检查是否有UV，这将决定切线的生成方式
            bool has_uvs = (mesh_data.format.attributes & VertexFormat::UV0) != 0;
            if (needs_tangents && !has_uvs) {
                ReportWarning("Generating procedural tangents because UVs are missing.");
            }

            // 步骤2: 重新计算包含所有新属性的顶点格式 (Format) 和步长 (Stride)
            VertexFormat new_format = mesh_data.format;
            if (needs_normals)  new_format.attributes |= VertexFormat::NORMAL;
            if (needs_tangents) new_format.attributes |= VertexFormat::TANGENT;

            uint32_t old_stride = mesh_data.format.stride;

            // 重新计算stride和所有属性的offset
            new_format.attribute_map.clear();
            uint32_t current_offset = 0;
            auto add_attribute = [&](VertexFormat::Attribute attr, uint32_t size, uint32_t type, uint32_t components, bool normalized = false) {
                if (new_format.attributes & attr) {
                    new_format.attribute_map[attr] = {current_offset, components, type, normalized};
                    current_offset += size;
                }
            };
            add_attribute(VertexFormat::POSITION, sizeof(float) * 3, GL_FLOAT, 3);
            add_attribute(VertexFormat::NORMAL,   sizeof(float) * 3, GL_FLOAT, 3);
            add_attribute(VertexFormat::TANGENT,  sizeof(float) * 4, GL_FLOAT, 4);
            add_attribute(VertexFormat::UV0,      sizeof(float) * 2, GL_FLOAT, 2);
            add_attribute(VertexFormat::UV1,      sizeof(float) * 2, GL_FLOAT, 2);
            add_attribute(VertexFormat::COLOR0,   sizeof(float) * 4, GL_FLOAT, 4);
            add_attribute(VertexFormat::JOINTS0,  sizeof(uint16_t) * 4, GL_UNSIGNED_SHORT, 4);
            add_attribute(VertexFormat::WEIGHTS0, sizeof(float) * 4, GL_FLOAT, 4);
            new_format.stride = current_offset;

            // 创建一个新的、足够大的顶点缓冲区
            std::vector<uint8_t> new_vertex_buffer(mesh_data.vertex_count * new_format.stride);

            // 步骤3: 累积计算法线和切线
            std::vector<glm::vec3> temp_normals(mesh_data.vertex_count, glm::vec3(0.0f));
            std::vector<glm::vec3> temp_tangents(mesh_data.vertex_count, glm::vec3(0.0f));
            std::vector<glm::vec3> temp_bitangents(mesh_data.vertex_count, glm::vec3(0.0f));

            const auto& old_pos_info = mesh_data.format.attribute_map.at(VertexFormat::POSITION);

            for (size_t i = 0; i < mesh_data.index_buffer.size(); i += 3) {
                uint32_t i0 = mesh_data.index_buffer[i];
                uint32_t i1 = mesh_data.index_buffer[i + 1];
                uint32_t i2 = mesh_data.index_buffer[i + 2];

                glm::vec3 p0, p1, p2;
                std::memcpy(&p0, mesh_data.vertex_buffer.data() + i0 * old_stride + old_pos_info.offset, sizeof(glm::vec3));
                std::memcpy(&p1, mesh_data.vertex_buffer.data() + i1 * old_stride + old_pos_info.offset, sizeof(glm::vec3));
                std::memcpy(&p2, mesh_data.vertex_buffer.data() + i2 * old_stride + old_pos_info.offset, sizeof(glm::vec3));

                glm::vec3 edge1 = p1 - p0;
                glm::vec3 edge2 = p2 - p0;

                // -> 如果需要法线，计算面法线并累加到顶点
                if (needs_normals) {
                    glm::vec3 face_normal = glm::cross(edge1, edge2);
                    temp_normals[i0] += face_normal;
                    temp_normals[i1] += face_normal;
                    temp_normals[i2] += face_normal;
                }

                // -> 如果需要切线并且模型有UV，用标准方法计算
                if (needs_tangents && has_uvs) {
                    const auto& old_uv_info = mesh_data.format.attribute_map.at(VertexFormat::UV0);
                    glm::vec2 uv0, uv1, uv2;
                    std::memcpy(&uv0, mesh_data.vertex_buffer.data() + i0 * old_stride + old_uv_info.offset, sizeof(glm::vec2));
                    std::memcpy(&uv1, mesh_data.vertex_buffer.data() + i1 * old_stride + old_uv_info.offset, sizeof(glm::vec2));
                    std::memcpy(&uv2, mesh_data.vertex_buffer.data() + i2 * old_stride + old_uv_info.offset, sizeof(glm::vec2));

                    glm::vec2 delta_uv1 = uv1 - uv0;
                    glm::vec2 delta_uv2 = uv2 - uv0;
                    float det = delta_uv1.x * delta_uv2.y - delta_uv2.x * delta_uv1.y;

                    if (std::abs(det) > 1e-6f) {
                        float f = 1.0f / det;
                        glm::vec3 tangent   = f * (delta_uv2.y * edge1 - delta_uv1.y * edge2);
                        glm::vec3 bitangent = f * (-delta_uv2.x * edge1 + delta_uv1.x * edge2);
                        temp_tangents[i0] += tangent;
                        temp_tangents[i1] += tangent;
                        temp_tangents[i2] += tangent;
                        temp_bitangents[i0] += bitangent;
                        temp_bitangents[i1] += bitangent;
                        temp_bitangents[i2] += bitangent;
                    }
                }
            }

            // 步骤4: 填充最终的顶点缓冲区
            for (uint32_t i = 0; i < mesh_data.vertex_count; ++i) {
                uint8_t* new_vertex_ptr = new_vertex_buffer.data() + i * new_format.stride;
                const uint8_t* old_vertex_ptr = mesh_data.vertex_buffer.data() + i * old_stride;

                // -> 复制所有旧数据到新缓冲区
                for (const auto& [attr, old_info] : mesh_data.format.attribute_map) {
                    const auto& new_info = new_format.attribute_map.at(attr);
                    size_t size_to_copy = 0;
                    if (attr == VertexFormat::JOINTS0) size_to_copy = sizeof(uint16_t) * 4;
                    else if (attr == VertexFormat::TANGENT || attr == VertexFormat::WEIGHTS0 || attr == VertexFormat::COLOR0) size_to_copy = sizeof(float) * 4;
                    else if (attr == VertexFormat::POSITION || attr == VertexFormat::NORMAL) size_to_copy = sizeof(float) * 3;
                    else if (attr == VertexFormat::UV0 || attr == VertexFormat::UV1) size_to_copy = sizeof(float) * 2;

                    if (size_to_copy > 0) {
                        std::memcpy(new_vertex_ptr + new_info.offset, old_vertex_ptr + old_info.offset, size_to_copy);
                    }
                }

                // -> 写入新的或旧的法线
                glm::vec3 final_normal;
                if (needs_normals) {
                    final_normal = glm::normalize(temp_normals[i]);
                } else {
                    const auto& old_normal_info = mesh_data.format.attribute_map.at(VertexFormat::NORMAL);
                    std::memcpy(&final_normal, old_vertex_ptr + old_normal_info.offset, sizeof(glm::vec3));
                }
                std::memcpy(new_vertex_ptr + new_format.attribute_map.at(VertexFormat::NORMAL).offset, &final_normal, sizeof(glm::vec3));

                // -> 生成最终的切线 (根据有无UV选择不同策略)
                if (needs_tangents) {
                    glm::vec4 final_tangent;
                    if (has_uvs) {
                        // Gram-Schmidt 正交化
                        glm::vec3 t = glm::normalize(temp_tangents[i] - final_normal * glm::dot(final_normal, temp_tangents[i]));
                        float handedness = (glm::dot(glm::cross(final_normal, t), temp_bitangents[i]) < 0.0f) ? -1.0f : 1.0f;
                        final_tangent = glm::vec4(t, handedness);
                    } else {
                        // 程序化生成一个与法线正交的切线
                        glm::vec3 up = (std::abs(final_normal.y) < 0.999f) ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
                        glm::vec3 t = glm::normalize(glm::cross(up, final_normal));
                        final_tangent = glm::vec4(t, 1.0f);
                    }
                    std::memcpy(new_vertex_ptr + new_format.attribute_map.at(VertexFormat::TANGENT).offset, &final_tangent, sizeof(glm::vec4));
                }
            }

            // 步骤5: 替换掉旧的顶点数据和格式
            mesh_data.vertex_buffer = std::move(new_vertex_buffer);
            mesh_data.format = new_format;
        }

    } // namespace asset
} // namespace spartan