#include "AssetSerializer.h"
#include "OzzSerializationHelper.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/io/stream.h"
#include <sstream>
#include <algorithm>
#include <cstring>

// 为了实现ProcessedAsset的序列化方法
#include "GltfTools.h"

namespace spartan {
    namespace asset {

// StringTable 实现
        uint32_t StringTable::AddString(const char* str) {
            if (!str || str[0] == '\0') {
                return 0; // 0 表示空字符串
            }

            std::string s(str);
            auto it = string_map_.find(s);
            if (it != string_map_.end()) {
                return it->second;
            }

            uint32_t index = static_cast<uint32_t>(strings_.size() + 1); // 从1开始
            strings_.push_back(s);
            string_map_[s] = index;
            return index;
        }

        uint32_t StringTable::AddString(const ozz::string& str) {
            return AddString(str.c_str());
        }

        const char* StringTable::GetString(uint32_t index) const {
            if (index == 0) return "";
            if (index > strings_.size()) return nullptr;
            return strings_[index - 1].c_str();
        }

        void StringTable::Serialize(std::vector<uint8_t>& buffer) const {
            // 写入字符串数量
            uint32_t count = static_cast<uint32_t>(strings_.size());
            buffer.reserve(buffer.size() + sizeof(count) + strings_.size() * 32); // 预估大小

            size_t offset = buffer.size();
            buffer.resize(offset + sizeof(count));
            std::memcpy(buffer.data() + offset, &count, sizeof(count));

            // 写入每个字符串
            for (const auto& str : strings_) {
                uint32_t len = static_cast<uint32_t>(str.length());

                offset = buffer.size();
                buffer.resize(offset + sizeof(len) + len);
                std::memcpy(buffer.data() + offset, &len, sizeof(len));
                std::memcpy(buffer.data() + offset + sizeof(len), str.data(), len);
            }
        }

        bool StringTable::Deserialize(const uint8_t* data, size_t size) {
            Clear();

            if (size < sizeof(uint32_t)) return false;

            uint32_t count;
            std::memcpy(&count, data, sizeof(count));
            data += sizeof(count);
            size -= sizeof(count);

            strings_.reserve(count);

            for (uint32_t i = 0; i < count; ++i) {
                if (size < sizeof(uint32_t)) return false;

                uint32_t len;
                std::memcpy(&len, data, sizeof(len));
                data += sizeof(len);
                size -= sizeof(len);

                if (size < len) return false;

                std::string str(reinterpret_cast<const char*>(data), len);
                strings_.push_back(str);
                string_map_[str] = i + 1;

                data += len;
                size -= len;
            }

            return true;
        }

// AssetSerializer 实现
        bool AssetSerializer::SerializeToFile(const ProcessedAsset& asset, const char* filepath) {
            std::ofstream file(filepath, std::ios::binary);
            if (!file.is_open()) {
                SetError(std::string("Failed to open file for writing: ") + filepath);
                return false;
            }

            // 清空字符串表
            string_table_.Clear();

            // 准备各个数据块
            std::vector<std::vector<uint8_t>> chunks(static_cast<size_t>(ChunkType::CHUNK_COUNT));
            std::vector<ChunkHeader> chunk_headers(static_cast<size_t>(ChunkType::CHUNK_COUNT));

            // 序列化各个部分
            SerializeMetadata(asset, chunks[static_cast<size_t>(ChunkType::METADATA)]);
            SerializeMeshes(asset, chunks[static_cast<size_t>(ChunkType::MESHES)]);
            SerializeMaterials(asset, chunks[static_cast<size_t>(ChunkType::MATERIALS)]);
            SerializeTextures(asset, chunks[static_cast<size_t>(ChunkType::TEXTURES)]);
            SerializeSkeletons(asset, chunks[static_cast<size_t>(ChunkType::SKELETONS)]);
            SerializeAnimations(asset, chunks[static_cast<size_t>(ChunkType::ANIMATIONS)]);
            SerializeSceneNodes(asset, chunks[static_cast<size_t>(ChunkType::SCENE_NODES)]);

            // 序列化字符串表
            string_table_.Serialize(chunks[static_cast<size_t>(ChunkType::STRING_TABLE)]);

            // 计算偏移和大小
            uint64_t current_offset = sizeof(AssetFileHeader) +
                                      sizeof(ChunkHeader) * static_cast<size_t>(ChunkType::CHUNK_COUNT);

            for (size_t i = 0; i < chunks.size(); ++i) {
                auto& header = chunk_headers[i];
                header.type = static_cast<ChunkType>(i);
                header.offset = current_offset;
                header.size = chunks[i].size();
                header.uncompressed_size = header.size; // 暂不压缩

                // 计算item_count
                switch (header.type) {
                    case ChunkType::MESHES:
                        header.item_count = static_cast<uint32_t>(asset.meshes.size());
                        break;
                    case ChunkType::MATERIALS:
                        header.item_count = static_cast<uint32_t>(asset.materials.size());
                        break;
                    case ChunkType::TEXTURES:
                        header.item_count = static_cast<uint32_t>(asset.textures.size());
                        break;
                    case ChunkType::SKELETONS:
                        header.item_count = static_cast<uint32_t>(asset.skeletons.size());
                        break;
                    case ChunkType::ANIMATIONS:
                        header.item_count = static_cast<uint32_t>(asset.animations.size());
                        break;
                    case ChunkType::SCENE_NODES:
                        header.item_count = static_cast<uint32_t>(asset.nodes.size());
                        break;
                    default:
                        header.item_count = 1;
                }

                current_offset += header.size;
            }

            // 写入文件头
            AssetFileHeader file_header;
            file_header.magic = AssetFileHeader::MAGIC;
            file_header.version = AssetFileHeader::VERSION;
            file_header.flags = enable_compression_ ? 1 : 0;
            file_header.chunk_count = static_cast<uint32_t>(ChunkType::CHUNK_COUNT);
            file_header.total_size = current_offset;
            file_header.checksum = 0; // TODO: 计算校验和

            file.write(reinterpret_cast<const char*>(&file_header), sizeof(file_header));

            // 写入块头
            for (const auto& header : chunk_headers) {
                file.write(reinterpret_cast<const char*>(&header), sizeof(header));
            }

            // 写入块数据
            for (const auto& chunk : chunks) {
                if (!chunk.empty()) {
                    file.write(reinterpret_cast<const char*>(chunk.data()), chunk.size());
                }
            }

            return true;
        }

        bool AssetSerializer::DeserializeFromFile(ProcessedAsset& asset, const char* filepath) {
            std::ifstream file(filepath, std::ios::binary | std::ios::ate);
            if (!file.is_open()) {
                SetError(std::string("Failed to open file for reading: ") + filepath);
                return false;
            }

            // 获取文件大小
            size_t file_size = file.tellg();
            file.seekg(0, std::ios::beg);

            // 读取文件头
            AssetFileHeader file_header;
            file.read(reinterpret_cast<char*>(&file_header), sizeof(file_header));

            if (file_header.magic != AssetFileHeader::MAGIC) {
                SetError("Invalid file magic number");
                return false;
            }

            if (file_header.version != AssetFileHeader::VERSION) {
                SetError("Unsupported file version");
                return false;
            }

            // 读取块头
            std::vector<ChunkHeader> chunk_headers(file_header.chunk_count);
            file.read(reinterpret_cast<char*>(chunk_headers.data()),
                      sizeof(ChunkHeader) * file_header.chunk_count);

            // 清空资产数据
            asset = ProcessedAsset();
            string_table_.Clear();

            // 处理每个数据块
            for (const auto& header : chunk_headers) {
                if (header.size == 0) continue;

                // 读取块数据
                std::vector<uint8_t> chunk_data(header.size);
                file.seekg(header.offset);
                file.read(reinterpret_cast<char*>(chunk_data.data()), header.size);

                // 反序列化块
                bool success = false;
                switch (header.type) {
                    case ChunkType::STRING_TABLE:
                        success = string_table_.Deserialize(chunk_data.data(), chunk_data.size());
                        break;
                    case ChunkType::METADATA:
                        success = DeserializeMetadata(asset, chunk_data.data(), chunk_data.size());
                        break;
                    case ChunkType::MESHES:
                        success = DeserializeMeshes(asset, chunk_data.data(), chunk_data.size());
                        break;
                    case ChunkType::MATERIALS:
                        success = DeserializeMaterials(asset, chunk_data.data(), chunk_data.size());
                        break;
                    case ChunkType::TEXTURES:
                        success = DeserializeTextures(asset, chunk_data.data(), chunk_data.size());
                        break;
                    case ChunkType::SKELETONS:
                        success = DeserializeSkeletons(asset, chunk_data.data(), chunk_data.size());
                        break;
                    case ChunkType::ANIMATIONS:
                        success = DeserializeAnimations(asset, chunk_data.data(), chunk_data.size());
                        break;
                    case ChunkType::SCENE_NODES:
                        success = DeserializeSceneNodes(asset, chunk_data.data(), chunk_data.size());
                        break;
                    default:
                        SetError("Unknown chunk type");
                        return false;
                }

                if (!success) {
                    SetError(std::string("Failed to deserialize chunk type: ") +
                             std::to_string(static_cast<int>(header.type)));
                    return false;
                }
            }

            return true;
        }

// 序列化网格数据
        void AssetSerializer::SerializeMeshes(const ProcessedAsset& asset, std::vector<uint8_t>& buffer) {
            Write(buffer, static_cast<uint32_t>(asset.meshes.size()));

            for (const auto& [handle, mesh] : asset.meshes) {
                // 写入句柄
                Write(buffer, handle.id);
                Write(buffer, handle.generation);

                // 写入顶点格式
                Write(buffer, mesh.format.attributes);
                Write(buffer, mesh.format.stride);
                Write(buffer, static_cast<uint32_t>(mesh.format.attribute_map.size()));

                for (const auto& [attr, info] : mesh.format.attribute_map) {
                    Write(buffer, attr);
                    Write(buffer, info.offset);
                    Write(buffer, info.components);
                    Write(buffer, info.type);
                    Write(buffer, info.normalized);
                }

                // 写入顶点数据
                Write(buffer, mesh.vertex_count);
                Write(buffer, static_cast<uint32_t>(mesh.vertex_buffer.size()));
                if (!mesh.vertex_buffer.empty()) {
                    size_t offset = buffer.size();
                    buffer.resize(offset + mesh.vertex_buffer.size());
                    std::memcpy(buffer.data() + offset, mesh.vertex_buffer.data(),
                                mesh.vertex_buffer.size());
                }

                // 写入索引数据
                Write(buffer, static_cast<uint32_t>(mesh.index_buffer.size()));
                if (!mesh.index_buffer.empty()) {
                    size_t offset = buffer.size();
                    buffer.resize(offset + mesh.index_buffer.size() * sizeof(uint32_t));
                    std::memcpy(buffer.data() + offset, mesh.index_buffer.data(),
                                mesh.index_buffer.size() * sizeof(uint32_t));
                }

                // 写入子网格
                Write(buffer, static_cast<uint32_t>(mesh.submeshes.size()));
                for (const auto& submesh : mesh.submeshes) {
                    Write(buffer, submesh.index_offset);
                    Write(buffer, submesh.index_count);
                    Write(buffer, submesh.base_vertex);
                    Write(buffer, submesh.material.id);
                    Write(buffer, submesh.material.generation);
                    Write(buffer, submesh.aabb_min.x);
                    Write(buffer, submesh.aabb_min.y);
                    Write(buffer, submesh.aabb_min.z);
                    Write(buffer, submesh.aabb_max.x);
                    Write(buffer, submesh.aabb_max.y);
                    Write(buffer, submesh.aabb_max.z);
                }

                // 写入骨骼信息
                Write(buffer, mesh.skeleton.has_value());
                if (mesh.skeleton.has_value()) {
                    Write(buffer, mesh.skeleton->id);
                    Write(buffer, mesh.skeleton->generation);

                    Write(buffer, static_cast<uint32_t>(mesh.inverse_bind_poses.size()));
                    for (const auto& mat : mesh.inverse_bind_poses) {
                        for (int i = 0; i < 4; ++i) {
                            for (int j = 0; j < 4; ++j) {
                                Write(buffer, mat[i][j]);
                            }
                        }
                    }
                }
            }
        }

// 序列化材质数据
        void AssetSerializer::SerializeMaterials(const ProcessedAsset& asset, std::vector<uint8_t>& buffer) {
            Write(buffer, static_cast<uint32_t>(asset.materials.size()));

            for (const auto& [handle, material] : asset.materials) {
                Write(buffer, handle.id);
                Write(buffer, handle.generation);

                Write(buffer, string_table_.AddString(material.name));

                // PBR参数
                Write(buffer, material.base_color_factor.x);
                Write(buffer, material.base_color_factor.y);
                Write(buffer, material.base_color_factor.z);
                Write(buffer, material.base_color_factor.w);
                Write(buffer, material.metallic_factor);
                Write(buffer, material.roughness_factor);
                Write(buffer, material.emissive_factor.x);
                Write(buffer, material.emissive_factor.y);
                Write(buffer, material.emissive_factor.z);

                // 纹理引用
                auto write_texture_handle = [&](const std::optional<TextureHandle>& tex) {
                    Write(buffer, tex.has_value());
                    if (tex.has_value()) {
                        Write(buffer, tex->id);
                        Write(buffer, tex->generation);
                    }
                };

                write_texture_handle(material.base_color_texture);
                write_texture_handle(material.metallic_roughness_texture);
                write_texture_handle(material.normal_texture);
                write_texture_handle(material.occlusion_texture);
                write_texture_handle(material.emissive_texture);

                // 渲染状态
                Write(buffer, static_cast<uint32_t>(material.alpha_mode));
                Write(buffer, material.alpha_cutoff);
                Write(buffer, material.double_sided);
                Write(buffer, material.shader_variant_flags);
            }
        }

// 序列化纹理数据
        void AssetSerializer::SerializeTextures(const ProcessedAsset& asset, std::vector<uint8_t>& buffer) {
            Write(buffer, static_cast<uint32_t>(asset.textures.size()));

            for (const auto& [handle, texture] : asset.textures) {
                Write(buffer, handle.id);
                Write(buffer, handle.generation);

                Write(buffer, string_table_.AddString(texture.uri));
                Write(buffer, texture.width);
                Write(buffer, texture.height);
                Write(buffer, texture.channels);
                Write(buffer, static_cast<uint32_t>(texture.format));
                Write(buffer, texture.is_srgb);
                Write(buffer, texture.mip_levels);
                Write(buffer, texture.generate_mipmaps);
            }
        }

// 序列化骨骼数据
        void AssetSerializer::SerializeSkeletons(const ProcessedAsset& asset, std::vector<uint8_t>& buffer) {
            Write(buffer, static_cast<uint32_t>(asset.skeletons.size()));

            for (const auto& [handle, skeleton] : asset.skeletons) {
                Write(buffer, handle.id);
                Write(buffer, handle.generation);

                if (skeleton) {
                    // 使用辅助函数序列化骨骼
                    auto data = SerializeOzzObject(*skeleton);
                    Write(buffer, static_cast<uint32_t>(data.size()));

                    size_t offset = buffer.size();
                    buffer.resize(offset + data.size());
                    std::memcpy(buffer.data() + offset, data.data(), data.size());
                } else {
                    Write(buffer, uint32_t(0));
                }
            }
        }

        void AssetSerializer::SerializeAnimations(const ProcessedAsset& asset, std::vector<uint8_t>& buffer) {
            Write(buffer, static_cast<uint32_t>(asset.animations.size()));

            for (const auto& [handle, anim] : asset.animations) {
                // --- 1. 序列化基本信息和句柄 ---
                Write(buffer, handle.id);
                Write(buffer, handle.generation);
                Write(buffer, string_table_.AddString(anim.name));
                Write(buffer, anim.duration);

                // --- 2. 序列化目标资源引用 ---
                Write(buffer, anim.target_skeleton.has_value());
                if (anim.target_skeleton.has_value()) {
                    Write(buffer, anim.target_skeleton->id);
                    Write(buffer, anim.target_skeleton->generation);
                }
                Write(buffer, anim.target_node.has_value());
                if (anim.target_node.has_value()) {
                    Write(buffer, anim.target_node.value());
                }
                Write(buffer, anim.target_mesh.has_value());
                if (anim.target_mesh.has_value()) {
                    Write(buffer, anim.target_mesh->id);
                    Write(buffer, anim.target_mesh->generation);
                }

                // --- 3. 序列化OZZ骨骼动画数据 ---
                bool has_skeletal = anim.skeletal_animation != nullptr;
                Write(buffer, has_skeletal);
                if (has_skeletal) {
                    auto data = SerializeOzzObject(*anim.skeletal_animation);
                    Write(buffer, static_cast<uint32_t>(data.size()));
                    if (!data.empty()) {
                        size_t offset = buffer.size();
                        buffer.resize(offset + data.size());
                        std::memcpy(buffer.data() + offset, data.data(), data.size());
                    }
                }

                // --- 4. 【核心改动】序列化节点变换动画的 map ---
                bool has_node_animations = !anim.node_animations.empty();
                Write(buffer, has_node_animations);
                if (has_node_animations) {
                    Write(buffer, static_cast<uint32_t>(anim.node_animations.size()));
                    for (const auto& [node_idx, data] : anim.node_animations) {
                        Write(buffer, node_idx); // 写入键 (node_index)

                        // 序列化值 (NodeTransformData)
                        Write(buffer, static_cast<uint32_t>(data.position_times.size()));
                        for (float t : data.position_times) Write(buffer, t);
                        for (const auto& v : data.position_values) { Write(buffer, v.x); Write(buffer, v.y); Write(buffer, v.z); }

                        Write(buffer, static_cast<uint32_t>(data.rotation_times.size()));
                        for (float t : data.rotation_times) Write(buffer, t);
                        for (const auto& q : data.rotation_values) { Write(buffer, q.x); Write(buffer, q.y); Write(buffer, q.z); Write(buffer, q.w); }

                        Write(buffer, static_cast<uint32_t>(data.scale_times.size()));
                        for (float t : data.scale_times) Write(buffer, t);
                        for (const auto& v : data.scale_values) { Write(buffer, v.x); Write(buffer, v.y); Write(buffer, v.z); }

                        Write(buffer, static_cast<uint8_t>(data.interpolation));
                    }
                }

            }
        }
// 序列化场景节点
        void AssetSerializer::SerializeSceneNodes(const ProcessedAsset& asset, std::vector<uint8_t>& buffer) {
            // 节点数据
            Write(buffer, static_cast<uint32_t>(asset.nodes.size()));

            for (const auto& node : asset.nodes) {
                Write(buffer, string_table_.AddString(node.name));
                Write(buffer, node.parent_index);

                Write(buffer, static_cast<uint32_t>(node.children.size()));
                for (int child : node.children) Write(buffer, child);

                // 变换
                Write(buffer, node.local_transform.translation.x);
                Write(buffer, node.local_transform.translation.y);
                Write(buffer, node.local_transform.translation.z);
                Write(buffer, node.local_transform.rotation.x);
                Write(buffer, node.local_transform.rotation.y);
                Write(buffer, node.local_transform.rotation.z);
                Write(buffer, node.local_transform.rotation.w);
                Write(buffer, node.local_transform.scale.x);
                Write(buffer, node.local_transform.scale.y);
                Write(buffer, node.local_transform.scale.z);

                // 资源引用
                Write(buffer, node.mesh.has_value());
                if (node.mesh.has_value()) {
                    Write(buffer, node.mesh->id);
                    Write(buffer, node.mesh->generation);
                }

                Write(buffer, node.skeleton.has_value());
                if (node.skeleton.has_value()) {
                    Write(buffer, node.skeleton->id);
                    Write(buffer, node.skeleton->generation);
                }

                Write(buffer, node.camera_index.has_value());
                if (node.camera_index.has_value()) {
                    Write(buffer, node.camera_index.value());
                }

                Write(buffer, node.light_index.has_value());
                if (node.light_index.has_value()) {
                    Write(buffer, node.light_index.value());
                }

                Write(buffer, node.is_animation_target);
            }

            // 根节点
            Write(buffer, static_cast<uint32_t>(asset.root_nodes.size()));
            for (int root : asset.root_nodes) Write(buffer, root);
        }

// 序列化元数据
        void AssetSerializer::SerializeMetadata(const ProcessedAsset& asset, std::vector<uint8_t>& buffer) {
            Write(buffer, string_table_.AddString(asset.metadata.source_file));
            Write(buffer, string_table_.AddString(asset.metadata.generator));
            Write(buffer, string_table_.AddString(asset.metadata.copyright));
            Write(buffer, asset.metadata.version);

            // 统计信息
            Write(buffer, asset.metadata.stats.total_vertices);
            Write(buffer, asset.metadata.stats.total_triangles);
            Write(buffer, asset.metadata.stats.total_bones);
            Write(buffer, asset.metadata.stats.total_animations);
            Write(buffer, asset.metadata.stats.total_memory_bytes);
        }

// 反序列化网格数据
        bool AssetSerializer::DeserializeMeshes(ProcessedAsset& asset, const uint8_t* data, size_t size) {
            const uint8_t* ptr = data;
            size_t remaining = size;

            uint32_t mesh_count;
            if (!Read(ptr, remaining, mesh_count)) return false;

            for (uint32_t i = 0; i < mesh_count; ++i) {
                MeshHandle handle;
                if (!Read(ptr, remaining, handle.id)) return false;
                if (!Read(ptr, remaining, handle.generation)) return false;

                MeshData& mesh = asset.meshes[handle];

                // 读取顶点格式
                if (!Read(ptr, remaining, mesh.format.attributes)) return false;
                if (!Read(ptr, remaining, mesh.format.stride)) return false;

                uint32_t attr_count;
                if (!Read(ptr, remaining, attr_count)) return false;

                for (uint32_t j = 0; j < attr_count; ++j) {
                    VertexFormat::Attribute attr;
                    VertexFormat::AttributeInfo info;

                    if (!Read(ptr, remaining, attr)) return false;
                    if (!Read(ptr, remaining, info.offset)) return false;
                    if (!Read(ptr, remaining, info.components)) return false;
                    if (!Read(ptr, remaining, info.type)) return false;
                    if (!Read(ptr, remaining, info.normalized)) return false;

                    mesh.format.attribute_map[attr] = info;
                }

                // 读取顶点数据
                if (!Read(ptr, remaining, mesh.vertex_count)) return false;

                uint32_t vertex_buffer_size;
                if (!Read(ptr, remaining, vertex_buffer_size)) return false;

                if (vertex_buffer_size > 0) {
                    if (remaining < vertex_buffer_size) return false;

                    mesh.vertex_buffer.resize(vertex_buffer_size);
                    std::memcpy(mesh.vertex_buffer.data(), ptr, vertex_buffer_size);
                    ptr += vertex_buffer_size;
                    remaining -= vertex_buffer_size;
                }

                // 读取索引数据
                uint32_t index_count;
                if (!Read(ptr, remaining, index_count)) return false;

                if (index_count > 0) {
                    size_t index_size = index_count * sizeof(uint32_t);
                    if (remaining < index_size) return false;

                    mesh.index_buffer.resize(index_count);
                    std::memcpy(mesh.index_buffer.data(), ptr, index_size);
                    ptr += index_size;
                    remaining -= index_size;
                }

                // 读取子网格
                uint32_t submesh_count;
                if (!Read(ptr, remaining, submesh_count)) return false;

                mesh.submeshes.resize(submesh_count);
                for (auto& submesh : mesh.submeshes) {
                    if (!Read(ptr, remaining, submesh.index_offset)) return false;
                    if (!Read(ptr, remaining, submesh.index_count)) return false;
                    if (!Read(ptr, remaining, submesh.base_vertex)) return false;
                    if (!Read(ptr, remaining, submesh.material.id)) return false;
                    if (!Read(ptr, remaining, submesh.material.generation)) return false;
                    if (!Read(ptr, remaining, submesh.aabb_min.x)) return false;
                    if (!Read(ptr, remaining, submesh.aabb_min.y)) return false;
                    if (!Read(ptr, remaining, submesh.aabb_min.z)) return false;
                    if (!Read(ptr, remaining, submesh.aabb_max.x)) return false;
                    if (!Read(ptr, remaining, submesh.aabb_max.y)) return false;
                    if (!Read(ptr, remaining, submesh.aabb_max.z)) return false;
                }

                // 读取骨骼信息
                bool has_skeleton;
                if (!Read(ptr, remaining, has_skeleton)) return false;

                if (has_skeleton) {
                    SkeletonHandle skel_handle;
                    if (!Read(ptr, remaining, skel_handle.id)) return false;
                    if (!Read(ptr, remaining, skel_handle.generation)) return false;
                    mesh.skeleton = skel_handle;

                    uint32_t bind_pose_count;
                    if (!Read(ptr, remaining, bind_pose_count)) return false;

                    mesh.inverse_bind_poses.resize(bind_pose_count);
                    for (auto& mat : mesh.inverse_bind_poses) {
                        for (int row = 0; row < 4; ++row) {
                            for (int col = 0; col < 4; ++col) {
                                if (!Read(ptr, remaining, mat[row][col])) return false;
                            }
                        }
                    }
                }
            }

            return true;
        }

// 反序列化材质数据
        bool AssetSerializer::DeserializeMaterials(ProcessedAsset& asset, const uint8_t* data, size_t size) {
            const uint8_t* ptr = data;
            size_t remaining = size;

            uint32_t material_count;
            if (!Read(ptr, remaining, material_count)) return false;

            for (uint32_t i = 0; i < material_count; ++i) {
                MaterialHandle handle;
                if (!Read(ptr, remaining, handle.id)) return false;
                if (!Read(ptr, remaining, handle.generation)) return false;

                MaterialData& material = asset.materials[handle];

                uint32_t name_idx;
                if (!Read(ptr, remaining, name_idx)) return false;
                material.name = string_table_.GetString(name_idx);

                // PBR参数
                if (!Read(ptr, remaining, material.base_color_factor.x)) return false;
                if (!Read(ptr, remaining, material.base_color_factor.y)) return false;
                if (!Read(ptr, remaining, material.base_color_factor.z)) return false;
                if (!Read(ptr, remaining, material.base_color_factor.w)) return false;
                if (!Read(ptr, remaining, material.metallic_factor)) return false;
                if (!Read(ptr, remaining, material.roughness_factor)) return false;
                if (!Read(ptr, remaining, material.emissive_factor.x)) return false;
                if (!Read(ptr, remaining, material.emissive_factor.y)) return false;
                if (!Read(ptr, remaining, material.emissive_factor.z)) return false;

                // 纹理引用
                auto read_texture_handle = [&](std::optional<TextureHandle>& tex) -> bool {
                    bool has_value;
                    if (!Read(ptr, remaining, has_value)) return false;

                    if (has_value) {
                        TextureHandle handle;
                        if (!Read(ptr, remaining, handle.id)) return false;
                        if (!Read(ptr, remaining, handle.generation)) return false;
                        tex = handle;
                    }

                    return true;
                };

                if (!read_texture_handle(material.base_color_texture)) return false;
                if (!read_texture_handle(material.metallic_roughness_texture)) return false;
                if (!read_texture_handle(material.normal_texture)) return false;
                if (!read_texture_handle(material.occlusion_texture)) return false;
                if (!read_texture_handle(material.emissive_texture)) return false;

                // 渲染状态
                uint32_t alpha_mode;
                if (!Read(ptr, remaining, alpha_mode)) return false;
                material.alpha_mode = static_cast<MaterialData::AlphaMode>(alpha_mode);

                if (!Read(ptr, remaining, material.alpha_cutoff)) return false;
                if (!Read(ptr, remaining, material.double_sided)) return false;
                if (!Read(ptr, remaining, material.shader_variant_flags)) return false;
            }

            return true;
        }

// 反序列化纹理数据
        bool AssetSerializer::DeserializeTextures(ProcessedAsset& asset, const uint8_t* data, size_t size) {
            const uint8_t* ptr = data;
            size_t remaining = size;

            uint32_t texture_count;
            if (!Read(ptr, remaining, texture_count)) return false;

            for (uint32_t i = 0; i < texture_count; ++i) {
                TextureHandle handle;
                if (!Read(ptr, remaining, handle.id)) return false;
                if (!Read(ptr, remaining, handle.generation)) return false;

                TextureData& texture = asset.textures[handle];

                uint32_t uri_idx;
                if (!Read(ptr, remaining, uri_idx)) return false;
                texture.uri = string_table_.GetString(uri_idx);

                if (!Read(ptr, remaining, texture.width)) return false;
                if (!Read(ptr, remaining, texture.height)) return false;
                if (!Read(ptr, remaining, texture.channels)) return false;

                uint32_t format;
                if (!Read(ptr, remaining, format)) return false;
                texture.format = static_cast<TextureData::Format>(format);

                if (!Read(ptr, remaining, texture.is_srgb)) return false;
                if (!Read(ptr, remaining, texture.mip_levels)) return false;
                if (!Read(ptr, remaining, texture.generate_mipmaps)) return false;
            }

            return true;
        }

// 反序列化骨骼数据
        bool AssetSerializer::DeserializeSkeletons(ProcessedAsset& asset, const uint8_t* data, size_t size) {
            const uint8_t* ptr = data;
            size_t remaining = size;

            uint32_t skeleton_count;
            if (!Read(ptr, remaining, skeleton_count)) return false;

            for (uint32_t i = 0; i < skeleton_count; ++i) {
                SkeletonHandle handle;
                if (!Read(ptr, remaining, handle.id)) return false;
                if (!Read(ptr, remaining, handle.generation)) return false;

                uint32_t data_size;
                if (!Read(ptr, remaining, data_size)) return false;

                if (data_size > 0) {
                    if (remaining < data_size) return false;

                    // 使用辅助函数反序列化骨骼
                    auto skeleton = ozz::make_unique<ozz::animation::Skeleton>();
                    if (!DeserializeOzzObject(ptr, data_size, *skeleton)) {
                        return false;
                    }

                    asset.skeletons[handle] = std::move(skeleton);

                    ptr += data_size;
                    remaining -= data_size;
                }
            }

            return true;
        }

// 反序列化动画数据

        /**
 * @brief 从缓冲区反序列化所有动画数据。
 *
 * 按与序列化时完全相反的顺序读取数据，重建 AnimationData 对象，
 * 包括其内部的 ozz::animation::Animation 对象和两个动画映射。
 *
 * @param asset 将被填充动画数据的 ProcessedAsset 对象。
 * @param data 包含动画数据的原始字节指针。
 * @param size 数据的大小。
 * @return 如果成功反序列化所有数据，返回 true，否则返回 false。
 */
        bool AssetSerializer::DeserializeAnimations(ProcessedAsset& asset, const uint8_t* data, size_t size) {
            const uint8_t* ptr = data;
            size_t remaining = size;

            uint32_t anim_count;
            if (!Read(ptr, remaining, anim_count)) return false;

            for (uint32_t i = 0; i < anim_count; ++i) {
                // --- 1. 反序列化基本信息和句柄 ---
                AnimationHandle handle;
                if (!Read(ptr, remaining, handle.id)) return false;
                if (!Read(ptr, remaining, handle.generation)) return false;
                AnimationData& anim = asset.animations[handle];

                uint32_t name_idx;
                if (!Read(ptr, remaining, name_idx)) return false;
                anim.name = string_table_.GetString(name_idx);

                if (!Read(ptr, remaining, anim.duration)) return false;

                // --- 2. 反序列化目标资源引用 ---
                bool has_value;
                if (!Read(ptr, remaining, has_value)) return false;
                if (has_value) {
                    SkeletonHandle skel_handle;
                    if (!Read(ptr, remaining, skel_handle.id)) return false;
                    if (!Read(ptr, remaining, skel_handle.generation)) return false;
                    anim.target_skeleton = skel_handle;
                }

                if (!Read(ptr, remaining, has_value)) return false;
                if (has_value) {
                    uint32_t node_idx;
                    if (!Read(ptr, remaining, node_idx)) return false;
                    anim.target_node = node_idx;
                }

                if (!Read(ptr, remaining, has_value)) return false;
                if (has_value) {
                    MeshHandle mesh_handle;
                    if (!Read(ptr, remaining, mesh_handle.id)) return false;
                    if (!Read(ptr, remaining, mesh_handle.generation)) return false;
                    anim.target_mesh = mesh_handle;
                }

                // --- 3. 反序列化OZZ骨骼动画数据 ---
                if (!Read(ptr, remaining, has_value)) return false;
                if (has_value) {
                    uint32_t data_size;
                    if (!Read(ptr, remaining, data_size)) return false;
                    if (data_size > 0) {
                        if (remaining < data_size) return false;
                        anim.skeletal_animation = std::make_unique<ozz::animation::Animation>();
                        if (!DeserializeOzzObject(ptr, data_size, *anim.skeletal_animation)) {
                            return false;
                        }
                        ptr += data_size;
                        remaining -= data_size;
                    }
                }

                // --- 4. 【核心改动】反序列化节点变换动画的 map ---
                if (!Read(ptr, remaining, has_value)) return false;
                if (has_value) {
                    uint32_t map_size;
                    if (!Read(ptr, remaining, map_size)) return false;
                    for (uint32_t j = 0; j < map_size; ++j) {
                        uint32_t node_idx;
                        if (!Read(ptr, remaining, node_idx)) return false;
                        auto& node_data = anim.node_animations[node_idx]; // 获取或创建 map 中的元素

                        uint32_t count;
                        // 位置
                        if (!Read(ptr, remaining, count)) return false;
                        node_data.position_times.resize(count);
                        for (auto& t : node_data.position_times) { if (!Read(ptr, remaining, t)) return false; }
                        node_data.position_values.resize(count);
                        for (auto& v : node_data.position_values) { if (!Read(ptr, remaining, v.x) || !Read(ptr, remaining, v.y) || !Read(ptr, remaining, v.z)) return false; }

                        // 旋转
                        if (!Read(ptr, remaining, count)) return false;
                        node_data.rotation_times.resize(count);
                        for (auto& t : node_data.rotation_times) { if (!Read(ptr, remaining, t)) return false; }
                        node_data.rotation_values.resize(count);
                        for (auto& q : node_data.rotation_values) { if (!Read(ptr, remaining, q.x) || !Read(ptr, remaining, q.y) || !Read(ptr, remaining, q.z) || !Read(ptr, remaining, q.w)) return false; }

                        // 缩放
                        if (!Read(ptr, remaining, count)) return false;
                        node_data.scale_times.resize(count);
                        for (auto& t : node_data.scale_times) { if (!Read(ptr, remaining, t)) return false; }
                        node_data.scale_values.resize(count);
                        for (auto& v : node_data.scale_values) { if (!Read(ptr, remaining, v.x) || !Read(ptr, remaining, v.y) || !Read(ptr, remaining, v.z)) return false; }

                        // 插值类型
                        uint8_t interp;
                        if (!Read(ptr, remaining, interp)) return false;
                        node_data.interpolation = static_cast<NodeTransformData::InterpolationType>(interp);
                    }
                }

            }

            return true;
        }

        // 反序列化场景节点
        bool AssetSerializer::DeserializeSceneNodes(ProcessedAsset& asset, const uint8_t* data, size_t size) {
            const uint8_t* ptr = data;
            size_t remaining = size;

            // 节点数据
            uint32_t node_count;
            if (!Read(ptr, remaining, node_count)) return false;

            asset.nodes.resize(node_count);
            for (auto& node : asset.nodes) {
                uint32_t name_idx;
                if (!Read(ptr, remaining, name_idx)) return false;
                node.name = string_table_.GetString(name_idx);

                if (!Read(ptr, remaining, node.parent_index)) return false;

                uint32_t child_count;
                if (!Read(ptr, remaining, child_count)) return false;

                node.children.resize(child_count);
                for (auto& child : node.children) {
                    if (!Read(ptr, remaining, child)) return false;
                }

                // 变换
                if (!Read(ptr, remaining, node.local_transform.translation.x)) return false;
                if (!Read(ptr, remaining, node.local_transform.translation.y)) return false;
                if (!Read(ptr, remaining, node.local_transform.translation.z)) return false;
                if (!Read(ptr, remaining, node.local_transform.rotation.x)) return false;
                if (!Read(ptr, remaining, node.local_transform.rotation.y)) return false;
                if (!Read(ptr, remaining, node.local_transform.rotation.z)) return false;
                if (!Read(ptr, remaining, node.local_transform.rotation.w)) return false;
                if (!Read(ptr, remaining, node.local_transform.scale.x)) return false;
                if (!Read(ptr, remaining, node.local_transform.scale.y)) return false;
                if (!Read(ptr, remaining, node.local_transform.scale.z)) return false;

                // 资源引用
                bool has_mesh;
                if (!Read(ptr, remaining, has_mesh)) return false;
                if (has_mesh) {
                    MeshHandle mesh_handle;
                    if (!Read(ptr, remaining, mesh_handle.id)) return false;
                    if (!Read(ptr, remaining, mesh_handle.generation)) return false;
                    node.mesh = mesh_handle;
                }

                bool has_skeleton;
                if (!Read(ptr, remaining, has_skeleton)) return false;
                if (has_skeleton) {
                    SkeletonHandle skel_handle;
                    if (!Read(ptr, remaining, skel_handle.id)) return false;
                    if (!Read(ptr, remaining, skel_handle.generation)) return false;
                    node.skeleton = skel_handle;
                }

                bool has_camera;
                if (!Read(ptr, remaining, has_camera)) return false;
                if (has_camera) {
                    uint32_t camera_idx;
                    if (!Read(ptr, remaining, camera_idx)) return false;
                    node.camera_index = camera_idx;
                }

                bool has_light;
                if (!Read(ptr, remaining, has_light)) return false;
                if (has_light) {
                    uint32_t light_idx;
                    if (!Read(ptr, remaining, light_idx)) return false;
                    node.light_index = light_idx;
                }

                if (!Read(ptr, remaining, node.is_animation_target)) return false;
            }

            // 根节点
            uint32_t root_count;
            if (!Read(ptr, remaining, root_count)) return false;

            asset.root_nodes.resize(root_count);
            for (auto& root : asset.root_nodes) {
                if (!Read(ptr, remaining, root)) return false;
            }

            return true;
        }

// 反序列化元数据
        bool AssetSerializer::DeserializeMetadata(ProcessedAsset& asset, const uint8_t* data, size_t size) {
            const uint8_t* ptr = data;
            size_t remaining = size;

            uint32_t idx;

            if (!Read(ptr, remaining, idx)) return false;
            asset.metadata.source_file = string_table_.GetString(idx);

            if (!Read(ptr, remaining, idx)) return false;
            asset.metadata.generator = string_table_.GetString(idx);

            if (!Read(ptr, remaining, idx)) return false;
            asset.metadata.copyright = string_table_.GetString(idx);

            if (!Read(ptr, remaining, asset.metadata.version)) return false;

            // 统计信息
            if (!Read(ptr, remaining, asset.metadata.stats.total_vertices)) return false;
            if (!Read(ptr, remaining, asset.metadata.stats.total_triangles)) return false;
            if (!Read(ptr, remaining, asset.metadata.stats.total_bones)) return false;
            if (!Read(ptr, remaining, asset.metadata.stats.total_animations)) return false;
            if (!Read(ptr, remaining, asset.metadata.stats.total_memory_bytes)) return false;

            return true;
        }

// 错误处理
        void AssetSerializer::SetError(const std::string& error) {
            last_error_ = error;
        }

        void AssetSerializer::WriteString(std::vector<uint8_t>& buffer, const char* str) {
            uint32_t idx = string_table_.AddString(str);
            Write(buffer, idx);
        }

        void AssetSerializer::WriteString(std::vector<uint8_t>& buffer, const ozz::string& str) {
            WriteString(buffer, str.c_str());
        }

        bool AssetSerializer::ReadString(const uint8_t*& data, size_t& remaining, std::string& str) {
            uint32_t idx;
            if (!Read(data, remaining, idx)) return false;

            const char* s = string_table_.GetString(idx);
            if (!s) return false;

            str = s;
            return true;
        }

// 压缩函数（暂不实现）
        bool AssetSerializer::CompressChunk(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
            // TODO: 实现压缩（如LZ4）
            output = input;
            return true;
        }

        bool AssetSerializer::DecompressChunk(const uint8_t* input, size_t input_size,
                                              std::vector<uint8_t>& output, size_t output_size) {
            // TODO: 实现解压
            output.resize(output_size);
            std::memcpy(output.data(), input, input_size);
            return true;
        }

// ProcessedAsset的序列化方法实现
        bool ProcessedAsset::Serialize(const char* output_path) const {
            AssetSerializer serializer;
            return serializer.SerializeToFile(*this, output_path);
        }

        bool ProcessedAsset::Deserialize(const char* input_path) {
            AssetSerializer serializer;
            return serializer.DeserializeFromFile(*this, input_path);
        }

    } // namespace asset
} // namespace spartan