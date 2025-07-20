// GltfToolsAnimation.cpp - 统一骨架动画管线实现（修正版）

// 禁用tiny_gltf中json.hpp的警告
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
#include <stack>
#include "GltfTools.h"
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

// 统一骨架数据结构，用于存储处理过程中的所有中间数据
// 辅助函数：从GLTF节点获取局部变换矩阵
        glm::mat4 GltfProcessor::Impl::GetNodeLocalMatrix(const tinygltf::Node& node) const {
            glm::mat4 local_transform(1.0f);

            if (!node.matrix.empty()) {
                // 使用矩阵
                for (int i = 0; i < 16; ++i) {
                    local_transform[i / 4][i % 4] = static_cast<float>(node.matrix[i]);
                }
            } else {
                // 从TRS构建矩阵
                glm::mat4 T(1.0f), R(1.0f), S(1.0f);

                if (!node.translation.empty()) {
                    T = glm::translate(glm::mat4(1.0f), glm::vec3(
                            static_cast<float>(node.translation[0]),
                            static_cast<float>(node.translation[1]),
                            static_cast<float>(node.translation[2])
                    ));
                }

                if (!node.rotation.empty()) {
                    glm::quat q(
                            static_cast<float>(node.rotation[3]), // w
                            static_cast<float>(node.rotation[0]), // x
                            static_cast<float>(node.rotation[1]), // y
                            static_cast<float>(node.rotation[2])  // z
                    );
                    R = glm::mat4_cast(q);
                }

                if (!node.scale.empty()) {
                    S = glm::scale(glm::mat4(1.0f), glm::vec3(
                            static_cast<float>(node.scale[0]),
                            static_cast<float>(node.scale[1]),
                            static_cast<float>(node.scale[2])
                    ));
                }

                local_transform = T * R * S;
            }

            return local_transform;
        }

// 步骤1：收集"关节"节点的最小集合
        void GltfProcessor::Impl::CollectUnifiedJoints(UnifiedSkeletonData& data) {
            std::cout << "\n=== 步骤1：收集统一关节集合 ===" << std::endl;

            // 1. 收集所有skin中的关节
            for (size_t skin_idx = 0; skin_idx < model.skins.size(); ++skin_idx) {
                const auto& skin = model.skins[skin_idx];
                for (int joint_idx : skin.joints) {
                    data.unified_joints.insert(joint_idx);
                    std::cout << "  从skin " << skin_idx << " 添加关节: " << joint_idx
                              << " (" << model.nodes[joint_idx].name << ")" << std::endl;
                }
            }

            // 2. 收集所有动画中的目标节点（包括火焰等特效节点）
            for (size_t anim_idx = 0; anim_idx < model.animations.size(); ++anim_idx) {
                const auto& anim = model.animations[anim_idx];
                for (const auto& channel : anim.channels) {
                    if (channel.target_path == "translation" ||
                        channel.target_path == "rotation" ||
                        channel.target_path == "scale") {
                        data.unified_joints.insert(channel.target_node);
                        std::cout << "  从动画 '" << anim.name << "' 添加节点: " << channel.target_node
                                  << " (" << model.nodes[channel.target_node].name << ")" << std::endl;
                    }
                }
            }

            // 3. 确保包含所有必要的父节点以构建完整的层级
            std::set<int> joints_to_add;
            for (int joint : data.unified_joints) {
                // 向上遍历，确保所有父节点都被包含
                int current = joint;
                std::vector<int> parent_chain;

                while (current >= 0 && current < model.nodes.size()) {
                    bool found_parent = false;
                    for (size_t i = 0; i < model.nodes.size(); ++i) {
                        const auto& node = model.nodes[i];
                        if (std::find(node.children.begin(), node.children.end(), current) != node.children.end()) {
                            parent_chain.push_back(static_cast<int>(i));
                            current = static_cast<int>(i);
                            found_parent = true;
                            break;
                        }
                    }
                    if (!found_parent) break;
                }

                // 添加所有父节点
                for (int parent : parent_chain) {
                    if (data.unified_joints.count(parent) == 0) {
                        joints_to_add.insert(parent);
                        std::cout << "  添加父节点: " << parent
                                  << " (" << model.nodes[parent].name << ")" << std::endl;
                    }
                }
            }

            // 合并新增的节点
            data.unified_joints.insert(joints_to_add.begin(), joints_to_add.end());

            std::cout << "统一关节集合大小: " << data.unified_joints.size() << std::endl;
        }
// 步骤2：计算所有节点的全局变换（修正版 - 高效算法）
        void GltfProcessor::Impl::CalculateGlobalTransforms(UnifiedSkeletonData& data) {
            std::cout << "\n=== 步骤2：计算全局变换 ===" << std::endl;

            data.global_transforms.resize(model.nodes.size(), glm::mat4(1.0f));

            // 构建父节点映射
            std::vector<int> parent_map(model.nodes.size(), -1);
            for (size_t i = 0; i < model.nodes.size(); ++i) {
                for (int child_idx : model.nodes[i].children) {
                    parent_map[child_idx] = static_cast<int>(i);
                }
            }

            // 使用栈进行非递归深度优先遍历
            std::vector<bool> visited(model.nodes.size(), false);
            std::stack<int> stack;

            // 将所有根节点压入栈
            for (size_t i = 0; i < model.nodes.size(); ++i) {
                if (parent_map[i] == -1) {
                    stack.push(static_cast<int>(i));
                }
            }

            // 处理所有节点
            while (!stack.empty()) {
                int node_idx = stack.top();

                if (!visited[node_idx]) {
                    // 如果父节点未处理，先处理父节点
                    if (parent_map[node_idx] != -1 && !visited[parent_map[node_idx]]) {
                        stack.push(parent_map[node_idx]);
                        continue;
                    }

                    // 计算局部变换
                    glm::mat4 local_transform = GetNodeLocalMatrix(model.nodes[node_idx]);

                    // 计算全局变换
                    if (parent_map[node_idx] >= 0) {
                        data.global_transforms[node_idx] = data.global_transforms[parent_map[node_idx]] * local_transform;
                    } else {
                        data.global_transforms[node_idx] = local_transform;
                    }

                    visited[node_idx] = true;
                    stack.pop();

                    // 将子节点压入栈
                    for (int child_idx : model.nodes[node_idx].children) {
                        stack.push(child_idx);
                    }
                } else {
                    stack.pop();
                }
            }

            std::cout << "计算了 " << data.global_transforms.size() << " 个节点的全局变换" << std::endl;
        }


        void GltfProcessor::Impl::BuildUnifiedSkeleton(UnifiedSkeletonData& data) {
            // 使用新的函数，它会处理附属物节点
            BuildUnifiedSkeletonWithAttachments(data);
        }

// 步骤4：生成 GLTF 节点到 OZZ 关节的正确映射
        void GltfProcessor::Impl::GenerateGltfToOzzMapping(UnifiedSkeletonData& data) {
            std::cout << "\n=== 步骤4：生成节点映射 ===" << std::endl;

            auto skeleton_it = output->skeletons.find(data.skeleton_handle);
            if (skeleton_it == output->skeletons.end() || !skeleton_it->second) {
                ReportError("Unified skeleton not found");
                return;
            }

            const auto& skeleton = skeleton_it->second;

            // 创建GLTF节点名称到索引的映射
            std::unordered_map<std::string, int> name_to_gltf_node;
            for (size_t i = 0; i < model.nodes.size(); ++i) {
                std::string node_name;

                // 优先使用覆盖的名称
                if (data.node_name_overrides.count(i) > 0) {
                    node_name = data.node_name_overrides[i];
                } else if (model.nodes[i].name.empty()) {
                    node_name = "joint_" + std::to_string(i);
                } else {
                    node_name = model.nodes[i].name;
                }

                name_to_gltf_node[node_name] = static_cast<int>(i);
            }

            // 通过名称匹配建立映射
            for (int ozz_joint_idx = 0; ozz_joint_idx < skeleton->num_joints(); ++ozz_joint_idx) {
                const char* joint_name = skeleton->joint_names()[ozz_joint_idx];

                auto it = name_to_gltf_node.find(joint_name);
                if (it != name_to_gltf_node.end()) {
                    data.gltf_node_to_ozz_joint[it->second] = ozz_joint_idx;
                    std::cout << "映射: GLTF节点 " << it->second << " ('" << joint_name
                              << "') -> OZZ关节 " << ozz_joint_idx << std::endl;
                } else {
                    std::cout << "警告: 找不到名为 '" << joint_name << "' 的GLTF节点" << std::endl;
                }
            }

            std::cout << "生成了 " << data.gltf_node_to_ozz_joint.size() << " 个映射" << std::endl;
        }

// 步骤5：计算统一骨架的逆绑定矩阵
        void GltfProcessor::Impl::CalculateInverseBindMatrices(UnifiedSkeletonData& data) {
            std::cout << "\n=== 步骤5：计算/映射 逆绑定矩阵 ===" << std::endl;

            auto skeleton_it = output->skeletons.find(data.skeleton_handle);
            if (skeleton_it == output->skeletons.end() || !skeleton_it->second) {
                ReportError("在计算逆绑定矩阵时找不到统一骨架");
                return;
            }
            const auto& skeleton = skeleton_it->second;
            data.inverse_bind_matrices.resize(skeleton->num_joints(), glm::mat4(1.0f));

            // 创建一个集合，用于跟踪哪些关节已经从GLTF文件加载了IBM
            std::unordered_set<int> joints_with_loaded_ibm;

            // --- 核心改动 1: 优先从GLTF的skin中读取官方的IBM ---
            for (const auto& skin : model.skins) {
                if (skin.inverseBindMatrices < 0) continue;

                // 读取文件中定义的IBM数据
                auto ibms_from_gltf = GetAccessorData<glm::mat4>(skin.inverseBindMatrices);

                // 遍历这个skin所包含的关节
                for (size_t i = 0; i < skin.joints.size(); ++i) {
                    if (i >= ibms_from_gltf.size()) continue;

                    int gltf_node_idx = skin.joints[i];

                    // 找到这个GLTF节点在我们新骨架中的索引
                    auto ozz_it = data.gltf_node_to_ozz_joint.find(gltf_node_idx);
                    if (ozz_it != data.gltf_node_to_ozz_joint.end()) {
                        int ozz_joint_idx = ozz_it->second;

                        // 将从文件读取的、正确的IBM存入我们的数据结构
                        data.inverse_bind_matrices[ozz_joint_idx] = ibms_from_gltf[i];
                        joints_with_loaded_ibm.insert(ozz_joint_idx);
                    }
                }
            }
            std::cout << "  从GLTF文件成功加载了 " << joints_with_loaded_ibm.size() << " 个关节的逆绑定矩阵。" << std::endl;


            // --- 核心改动 2: 为那些不是蒙皮骨骼的节点（例如火焰）计算一个默认的IBM ---
            int fallback_count = 0;
            for (const auto& [gltf_node_idx, ozz_joint_idx] : data.gltf_node_to_ozz_joint) {
                // 如果这个关节的IBM没有从文件加载，我们就为它计算一个
                if (joints_with_loaded_ibm.find(ozz_joint_idx) == joints_with_loaded_ibm.end()) {
                    if (gltf_node_idx < static_cast<int>(data.global_transforms.size())) {
                        data.inverse_bind_matrices[ozz_joint_idx] = glm::inverse(data.global_transforms[gltf_node_idx]);
                        fallback_count++;
                    }
                }
            }
            std::cout << "  为 " << fallback_count << " 个非蒙皮节点（如火焰）计算了备用逆绑定矩阵。" << std::endl;
        }

// 步骤6：为统一骨架处理所有网格（修正版 - 正确的顶点数据复制）
        void GltfProcessor::Impl::ProcessMeshesForUnifiedSkeleton(UnifiedSkeletonData& data) {
            std::cout << "\n=== 步骤6：处理网格关联 ===" << std::endl;

            // 找出哪个GLTF节点附加了哪个网格
            std::unordered_map<int, int> mesh_to_node;
            for (size_t node_idx = 0; node_idx < model.nodes.size(); ++node_idx) {
                if (model.nodes[node_idx].mesh >= 0) {
                    mesh_to_node[model.nodes[node_idx].mesh] = static_cast<int>(node_idx);
                }
            }

            // 处理每个网格
            for (auto& [mesh_handle, mesh_data] : output->meshes) {
                // 找到这个网格对应的GLTF网格索引
                int gltf_mesh_idx = -1;
                for (const auto& [idx, handle] : mesh_handle_map) {
                    if (handle == mesh_handle) {
                        gltf_mesh_idx = idx;
                        break;
                    }
                }

                if (gltf_mesh_idx < 0) continue;

                // 找到附加这个网格的节点
                auto node_it = mesh_to_node.find(gltf_mesh_idx);
                if (node_it == mesh_to_node.end()) continue;

                int node_idx = node_it->second;

                // 检查这个节点是否在关节映射中
                if (model.nodes[node_idx].skin >= 0) {
                    mesh_data.skeleton = data.skeleton_handle;
                    mesh_data.inverse_bind_poses = data.inverse_bind_matrices;

                    std::cout << "关联网格到统一骨架，节点: " << node_idx << std::endl;



                    // +++++++++++++++++++++++++++++ 新增代码块开始 +++++++++++++++++++++++++++++
                    // 检查网格是否真的有蒙皮数据需要重映射
                    if ((mesh_data.format.attributes & VertexFormat::JOINTS0) != 0) {
                        std::cout << "  开始重映射蒙皮索引..." << std::endl;

                        const auto& skin = model.skins[model.nodes[node_idx].skin];
                        auto joints_info_it = mesh_data.format.attribute_map.find(VertexFormat::JOINTS0);

                        if (joints_info_it != mesh_data.format.attribute_map.end()) {
                            const auto& joints_info = joints_info_it->second;
                            int remapped_count = 0;

                            // 遍历每个顶点
                            for (uint32_t v = 0; v < mesh_data.vertex_count; ++v) {
                                uint8_t* vertex_ptr = mesh_data.vertex_buffer.data() + v * mesh_data.format.stride;

                                // 定位到JOINTS_0数据
                                glm::u16vec4* original_joints = reinterpret_cast<glm::u16vec4*>(vertex_ptr + joints_info.offset);
                                glm::u16vec4 new_joints;

                                // 遍历每个顶点的4个骨骼影响
                                for (int j = 0; j < 4; ++j) {
                                    // 1. 获取原始的骨骼索引 (这是指向 skin.joints 数组的索引)
                                    uint16_t original_joint_index = (*original_joints)[j];

                                    if (original_joint_index < skin.joints.size()) {
                                        // 2. 从 skin.joints 数组中获取 GLTF 节点 ID
                                        int gltf_node_id = skin.joints[original_joint_index];

                                        // 3. 从我们的映射表中找到该 GLTF 节点对应的新 OZZ 骨架索引
                                        auto ozz_it = data.gltf_node_to_ozz_joint.find(gltf_node_id);
                                        if (ozz_it != data.gltf_node_to_ozz_joint.end()) {
                                            new_joints[j] = static_cast<uint16_t>(ozz_it->second);
                                        } else {
                                            new_joints[j] = 0; // 如果找不到，默认为根骨骼
                                        }
                                    } else {
                                        new_joints[j] = 0;
                                    }
                                }

                                // 4. 将新的 OZZ 骨架索引写回顶点缓冲区
                                *original_joints = new_joints;
                                remapped_count++;
                            }
                            std::cout << "  成功重映射 " << remapped_count << " 个顶点的蒙皮索引。" << std::endl;
                        }
                    }
                    // +++++++++++++++++++++++++++++ 新增代码块结束 +++++++++++++++++++++++++++++


                    // 处理刚性蒙皮
                    if ((mesh_data.format.attributes & VertexFormat::JOINTS0) == 0) {
                        std::cout << "  处理刚性蒙皮，添加JOINTS和WEIGHTS属性" << std::endl;

                        // 保存旧的格式信息
                        auto old_format = mesh_data.format;
                        auto old_stride = mesh_data.format.stride;

                        // 更新顶点格式
                        mesh_data.format.attributes |= (VertexFormat::JOINTS0 | VertexFormat::WEIGHTS0);

                        // 重新计算属性映射和stride
                        uint32_t new_stride = 0;
                        mesh_data.format.attribute_map.clear();

                        auto add_attribute = [&](VertexFormat::Attribute attr, uint32_t size, uint32_t type, uint32_t components) {
                            if (mesh_data.format.attributes & attr) {
                                mesh_data.format.attribute_map[attr] = {new_stride, components, type, false};
                                new_stride += size;
                            }
                        };

                        add_attribute(VertexFormat::POSITION, sizeof(float) * 3, GL_FLOAT, 3);
                        add_attribute(VertexFormat::NORMAL, sizeof(float) * 3, GL_FLOAT, 3);
                        add_attribute(VertexFormat::TANGENT, sizeof(float) * 4, GL_FLOAT, 4);
                        add_attribute(VertexFormat::UV0, sizeof(float) * 2, GL_FLOAT, 2);
                        add_attribute(VertexFormat::UV1, sizeof(float) * 2, GL_FLOAT, 2);
                        add_attribute(VertexFormat::COLOR0, sizeof(float) * 4, GL_FLOAT, 4);
                        add_attribute(VertexFormat::JOINTS0, sizeof(uint16_t) * 4, GL_UNSIGNED_SHORT, 4);
                        add_attribute(VertexFormat::WEIGHTS0, sizeof(float) * 4, GL_FLOAT, 4);

                        mesh_data.format.stride = new_stride;

                        // 重建顶点缓冲区
                        std::vector<uint8_t> new_vertex_buffer(mesh_data.vertex_count * new_stride);
                        uint32_t ozz_joint = data.gltf_node_to_ozz_joint[node_idx];

                        for (uint32_t v = 0; v < mesh_data.vertex_count; ++v) {
                            uint8_t* old_vertex = mesh_data.vertex_buffer.data() + v * old_stride;
                            uint8_t* new_vertex = new_vertex_buffer.data() + v * new_stride;

                            // 复制现有的顶点属性
                            for (const auto& [attr, old_info] : old_format.attribute_map) {
                                if (mesh_data.format.attribute_map.count(attr) > 0) {
                                    const auto& new_info = mesh_data.format.attribute_map[attr];
                                    size_t copy_size = 0;

                                    switch (attr) {
                                        case VertexFormat::POSITION:
                                        case VertexFormat::NORMAL:
                                            copy_size = sizeof(float) * 3;
                                            break;
                                        case VertexFormat::TANGENT:
                                        case VertexFormat::WEIGHTS0:
                                        case VertexFormat::COLOR0:
                                            copy_size = sizeof(float) * 4;
                                            break;
                                        case VertexFormat::UV0:
                                        case VertexFormat::UV1:
                                            copy_size = sizeof(float) * 2;
                                            break;
                                        case VertexFormat::JOINTS0:
                                            copy_size = sizeof(uint16_t) * 4;
                                            break;
                                    }

                                    // 执行实际的内存复制
                                    if (copy_size > 0) {
                                        std::memcpy(new_vertex + new_info.offset, old_vertex + old_info.offset, copy_size);
                                    }
                                }
                            }

                            // 添加新的关节和权重数据
                            if (mesh_data.format.attribute_map.count(VertexFormat::JOINTS0) > 0) {
                                const auto& joints_info = mesh_data.format.attribute_map[VertexFormat::JOINTS0];
                                glm::u16vec4 joints(ozz_joint, 0, 0, 0);
                                std::memcpy(new_vertex + joints_info.offset, &joints, sizeof(glm::u16vec4));
                            }

                            if (mesh_data.format.attribute_map.count(VertexFormat::WEIGHTS0) > 0) {
                                const auto& weights_info = mesh_data.format.attribute_map[VertexFormat::WEIGHTS0];
                                glm::vec4 weights(1.0f, 0.0f, 0.0f, 0.0f);
                                std::memcpy(new_vertex + weights_info.offset, &weights, sizeof(glm::vec4));
                            }
                        }

                        mesh_data.vertex_buffer = std::move(new_vertex_buffer);
                    }
                }
            }
        }


        void GltfProcessor::Impl::ProcessAnimationsForUnifiedSkeleton(UnifiedSkeletonData& data) {
            std::cout << "\n=== 步骤7：处理动画 ===" << std::endl;

            auto skeleton_it = output->skeletons.find(data.skeleton_handle);
            if (skeleton_it == output->skeletons.end() || !skeleton_it->second) {
                return;

                std::cout << "检测到纯节点动画，处理为节点动画数据..." << std::endl;

                return;
            }

            const auto& skeleton = skeleton_it->second;

            for (size_t anim_idx = 0; anim_idx < model.animations.size(); ++anim_idx) {
                const auto& gltf_anim = model.animations[anim_idx];

                std::cout << "处理动画: '" << gltf_anim.name << "'" << std::endl;

                // 所有动画都通过骨骼动画处理
                ozz::animation::offline::RawAnimation raw_animation;
                raw_animation.name = gltf_anim.name.c_str();
                raw_animation.tracks.resize(skeleton->num_joints());

                // 收集时间点
                std::set<float> all_times;
                for (const auto& channel : gltf_anim.channels) {
                    if (channel.sampler < 0 || channel.sampler >= static_cast<int>(gltf_anim.samplers.size()))
                        continue;

                    const auto& sampler = gltf_anim.samplers[channel.sampler];
                    auto times = GetAccessorData<float>(sampler.input);
                    all_times.insert(times.begin(), times.end());
                }

                if (!all_times.empty()) {
                    raw_animation.duration = *all_times.rbegin();
                } else {
                    raw_animation.duration = 1.0f;
                }

                // 填充动画轨道
                for (const auto& channel : gltf_anim.channels) {
                    if (channel.sampler < 0 || channel.sampler >= static_cast<int>(gltf_anim.samplers.size()))
                        continue;

                    // 查找OZZ关节索引
                    auto joint_it = data.gltf_node_to_ozz_joint.find(channel.target_node);
                    if (joint_it == data.gltf_node_to_ozz_joint.end()) {
                        std::cout << "  警告: 节点 " << channel.target_node
                                  << " 没有对应的OZZ关节映射" << std::endl;
                        continue;
                    }

                    int ozz_joint_idx = joint_it->second;
                    if (ozz_joint_idx < 0 || ozz_joint_idx >= skeleton->num_joints()) {
                        std::cout << "  警告: OZZ关节索引 " << ozz_joint_idx << " 超出范围" << std::endl;
                        continue;
                    }

                    const auto& sampler = gltf_anim.samplers[channel.sampler];
                    auto& track = raw_animation.tracks[ozz_joint_idx];
                    auto times = GetAccessorData<float>(sampler.input);

                    if (times.empty()) continue;

                    std::cout << "  处理通道: " << channel.target_path
                              << " 对于节点 " << channel.target_node
                              << " -> OZZ关节 " << ozz_joint_idx << std::endl;

                    if (channel.target_path == "translation") {
                        auto translations = GetAccessorData<glm::vec3>(sampler.output);
                        if (translations.size() == times.size()) {
                            track.translations.resize(times.size());
                            for (size_t i = 0; i < times.size(); ++i) {
                                track.translations[i] = {times[i], ToOZZ(translations[i])};
                            }
                        }
                    } else if (channel.target_path == "rotation") {
                        auto rotations = GetAccessorData<glm::vec4>(sampler.output);
                        if (rotations.size() == times.size()) {
                            track.rotations.resize(times.size());
                            for (size_t i = 0; i < times.size(); ++i) {
                                glm::quat q(rotations[i].w, rotations[i].x, rotations[i].y, rotations[i].z);
                                q = glm::normalize(q);
                                track.rotations[i] = {times[i], ozz::math::Quaternion(q.x, q.y, q.z, q.w)};
                            }
                        }
                    } else if (channel.target_path == "scale") {
                        auto scales = GetAccessorData<glm::vec3>(sampler.output);
                        if (scales.size() == times.size()) {
                            track.scales.resize(times.size());
                            for (size_t i = 0; i < times.size(); ++i) {
                                track.scales[i] = {times[i], ToOZZ(scales[i])};
                            }
                        }
                    }
                }

                // 为没有动画的轨道填充静态数据
                for (int j = 0; j < skeleton->num_joints(); ++j) {
                    auto& track = raw_animation.tracks[j];

                    if (track.translations.empty() || track.rotations.empty() || track.scales.empty()) {
                        // 获取绑定姿势
                        ozz::math::Transform bind_pose;
                        bind_pose.translation = ozz::math::Float3(0.0f, 0.0f, 0.0f);
                        bind_pose.rotation = ozz::math::Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
                        bind_pose.scale = ozz::math::Float3(1.0f, 1.0f, 1.0f);

                        // 从GLTF节点获取绑定姿势
                        for (const auto& [gltf_idx, ozz_idx] : data.gltf_node_to_ozz_joint) {
                            if (ozz_idx == j && gltf_idx < static_cast<int>(model.nodes.size())) {
                                bind_pose = GetNodeTransform(model.nodes[gltf_idx]);
                                break;
                            }
                        }

                        if (track.translations.empty()) {
                            track.translations.push_back({0.0f, bind_pose.translation});
                        }
                        if (track.rotations.empty()) {
                            track.rotations.push_back({0.0f, bind_pose.rotation});
                        }
                        if (track.scales.empty()) {
                            track.scales.push_back({0.0f, bind_pose.scale});
                        }
                    }
                }

                // 构建最终动画
                if (raw_animation.Validate()) {
                    ozz::animation::offline::AnimationBuilder builder;
                    auto animation = builder(raw_animation);

                    if (animation) {
                        AnimationHandle handle = output->handle_generator.Generate<AnimationTag>();
                        AnimationData& anim_data = output->animations[handle];

                        anim_data.name = gltf_anim.name.c_str();
                        anim_data.duration = raw_animation.duration;
                        anim_data.target_skeleton = data.skeleton_handle;
                        anim_data.skeletal_animation.reset(animation.release());

                        // +++ 新增：自动检测并存储根运动骨骼索引 +++
                        for (const auto& channel : gltf_anim.channels) {
                            if (channel.target_path == "translation") {
                                auto it = data.gltf_node_to_ozz_joint.find(channel.target_node);
                                if (it != data.gltf_node_to_ozz_joint.end()) {
                                    // 找到了第一个带位移动画的关节，标记它
                                    anim_data.root_motion_joint_index = it->second;
                                    std::cout << "  动画 '" << anim_data.name.c_str()
                                              << "' 的根运动关节被识别为: OZZ关节 "
                                              << it->second << std::endl;
                                    break; // 找到第一个就停止
                                }
                            }
                        }

                        output->metadata.stats.total_animations++;

                        std::cout << "  成功创建动画，时长: " << anim_data.duration << "秒" << std::endl;
                    }
                } else {
                    std::cout << "  警告: 动画验证失败" << std::endl;
                }
            }
        }

        void GltfProcessor::Impl::ProcessPureNodeAnimations() {
            for (size_t anim_idx = 0; anim_idx < model.animations.size(); ++anim_idx) {
                const auto& gltf_anim = model.animations[anim_idx];

                std::cout << "处理纯节点动画: '" << gltf_anim.name << "'" << std::endl;

                AnimationHandle handle = output->handle_generator.Generate<AnimationTag>();
                AnimationData& anim_data = output->animations[handle];

                anim_data.name = gltf_anim.name.c_str();

                // 计算动画时长
                float max_time = 0.0f;
                for (const auto& channel : gltf_anim.channels) {
                    if (channel.sampler >= 0 && channel.sampler < static_cast<int>(gltf_anim.samplers.size())) {
                        const auto& sampler = gltf_anim.samplers[channel.sampler];
                        auto times = GetAccessorData<float>(sampler.input);
                        if (!times.empty()) {
                            max_time = std::max(max_time, times.back());
                        }
                    }
                }
                anim_data.duration = max_time > 0.0f ? max_time : 1.0f;

                // 填充节点动画数据
                for (const auto& channel : gltf_anim.channels) {
                    if (channel.sampler < 0 || channel.sampler >= static_cast<int>(gltf_anim.samplers.size())) {
                        continue;
                    }

                    int target_node = channel.target_node;
                    const auto& sampler = gltf_anim.samplers[channel.sampler];

                    auto times = GetAccessorData<float>(sampler.input);
                    if (times.empty()) continue;

                    // 获取或创建节点变换数据
                    auto& node_data = anim_data.node_animations[target_node];

                    std::cout << "  处理节点 " << target_node << " 的 " << channel.target_path << " 动画" << std::endl;

                    if (channel.target_path == "translation") {
                        auto translations = GetAccessorData<glm::vec3>(sampler.output);
                        if (translations.size() == times.size()) {
                            node_data.position_times = times;
                            node_data.position_values.clear();
                            node_data.position_values.reserve(translations.size());
                            for (const auto& trans : translations) {
                                node_data.position_values.push_back(ToOZZ(trans));
                            }
                        }
                    }
                    else if (channel.target_path == "rotation") {
                        auto rotations = GetAccessorData<glm::vec4>(sampler.output);
                        if (rotations.size() == times.size()) {
                            node_data.rotation_times = times;
                            node_data.rotation_values.clear();
                            node_data.rotation_values.reserve(rotations.size());
                            for (const auto& rot : rotations) {
                                glm::quat q(rot.w, rot.x, rot.y, rot.z);
                                q = glm::normalize(q);
                                node_data.rotation_values.push_back(ozz::math::Quaternion(q.x, q.y, q.z, q.w));
                            }
                        }
                    }
                    else if (channel.target_path == "scale") {
                        auto scales = GetAccessorData<glm::vec3>(sampler.output);
                        if (scales.size() == times.size()) {
                            node_data.scale_times = times;
                            node_data.scale_values.clear();
                            node_data.scale_values.reserve(scales.size());
                            for (const auto& scale : scales) {
                                node_data.scale_values.push_back(ToOZZ(scale));
                            }
                        }
                    }

                    // 设置插值类型
                    if (sampler.interpolation == "LINEAR") {
                        node_data.interpolation = NodeTransformData::InterpolationType::LINEAR;
                    } else if (sampler.interpolation == "STEP") {
                        node_data.interpolation = NodeTransformData::InterpolationType::STEP;
                    } else if (sampler.interpolation == "CUBICSPLINE") {
                        node_data.interpolation = NodeTransformData::InterpolationType::CUBIC_SPLINE;
                    }
                }

                output->metadata.stats.total_animations++;
                std::cout << "  成功创建纯节点动画，时长: " << anim_data.duration << "秒" << std::endl;
            }
        }

        // 主处理函数
        bool GltfProcessor::ProcessFile(const char* input_path, ProcessedAsset& output) {
            auto start_time = std::chrono::high_resolution_clock::now();

            impl_->output = &output;
            impl_->config = &config_;
            impl_->progress_callback = &progress_callback_;
            impl_->last_error = &last_error_;
            impl_->context.Clear();

            // 加载GLTF文件
            impl_->ReportProgress(0.1f, "Loading GLTF file");
            if (!impl_->LoadGltfFile(input_path)) {
                return false;
            }

            // 处理纹理
            impl_->ReportProgress(0.2f, "Processing textures");
            if (!impl_->ProcessTextures()) {
                return false;
            }

            // 处理材质
            impl_->ReportProgress(0.3f, "Processing materials");
            if (!impl_->ProcessMaterials()) {
                return false;
            }

            // 处理骨骼
            impl_->ReportProgress(0.6f, "Processing skeletons");
            if (!impl_->ProcessSkeletons()) {
                return false;
            }

            // 处理网格
            impl_->ReportProgress(0.4f, "Processing meshes");
            if (!impl_->ProcessMeshes()) {
                return false;
            }

            // 处理动画
            impl_->ReportProgress(0.7f, "Processing animations");
            if (!impl_->ProcessAnimations()) {
                return false;
            }

            // 处理场景节点
            impl_->ReportProgress(0.8f, "Processing scene nodes");
            if (!impl_->ProcessSceneNodes()) {
                return false;
            }

            // 验证和完成
            impl_->ReportProgress(0.9f, "Validating and finalizing");
            if (!impl_->ValidateAndFinalize()) {
                return false;
            }

            // 设置元数据
            output.metadata.source_file = input_path;
            output.metadata.generator = "Spartan GLTF Processor v1.0";

            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

            ozz::log::Out() << "GLTF processing completed in " << duration.count() << "ms" << std::endl;
            ozz::log::Out() << "Stats: " << output.metadata.stats.total_vertices << " vertices, "
                            << output.metadata.stats.total_triangles << " triangles, "
                            << output.metadata.stats.total_bones << " bones, "
                            << output.metadata.stats.total_animations << " animations" << std::endl;

            impl_->ReportProgress(1.0f, "Complete");

            return true;
        }

// 修改后的ProcessAnimations - 作为统一管线的入口
        bool GltfProcessor::Impl::ProcessAnimations() {
            std::cout << "\n=== 处理动画（统一骨架管线）===" << std::endl;

            // 创建统一骨架数据
            UnifiedSkeletonData unified_skeleton_data;

            // 执行七个步骤
            CollectUnifiedJoints(unified_skeleton_data);

            if (!unified_skeleton_data.unified_joints.empty()) {
                CalculateGlobalTransforms(unified_skeleton_data);
                BuildUnifiedSkeleton(unified_skeleton_data);
                GenerateGltfToOzzMapping(unified_skeleton_data);
                CalculateInverseBindMatrices(unified_skeleton_data);
                ProcessMeshesForUnifiedSkeleton(unified_skeleton_data);
                ProcessAnimationsForUnifiedSkeleton(unified_skeleton_data);
                unified_skeleton_nodes = unified_skeleton_data.unified_joints;
            }

            return true;
        }

// ProcessSkeletons现在为空，因为骨架处理已经在ProcessAnimations中完成
        bool GltfProcessor::Impl::ProcessSkeletons() {
            // 骨架已在 ProcessAnimations 中通过统一管线处理
            return true;
        }

// 处理场景节点
        bool GltfProcessor::Impl::ProcessSceneNodes() {
            output->nodes.resize(model.nodes.size());

            // 获取统一骨架句柄
            asset::SkeletonHandle unified_skeleton_handle;
            if (!output->skeletons.empty()) {
                unified_skeleton_handle = output->skeletons.begin()->first;
            }

            // 复制节点数据
            for (size_t i = 0; i < model.nodes.size(); ++i) {
                const auto& gltf_node = model.nodes[i];
                auto& scene_node = output->nodes[i];

                scene_node.name = gltf_node.name.c_str();
                scene_node.local_transform = GetNodeTransform(gltf_node);

                // 设置父节点
                for (size_t j = 0; j < model.nodes.size(); ++j) {
                    const auto& parent = model.nodes[j];
                    if (std::find(parent.children.begin(), parent.children.end(), i) != parent.children.end()) {
                        scene_node.parent_index = static_cast<int>(j);
                        break;
                    }
                }

                // 设置子节点
                scene_node.children = gltf_node.children;

                // 设置网格引用 - 这很重要！
                if (gltf_node.mesh >= 0) {
                    auto it = mesh_handle_map.find(gltf_node.mesh);
                    if (it != mesh_handle_map.end()) {
                        scene_node.mesh = it->second;
                        std::cout << "节点 " << i << " (" << gltf_node.name
                                  << ") 关联网格 " << gltf_node.mesh << std::endl;
                    }
                }

                // 骨架引用
                if (unified_skeleton_handle.IsValid()) {
                    if (unified_skeleton_nodes.count(i) > 0 || gltf_node.skin >= 0) {
                        scene_node.skeleton = unified_skeleton_handle;
                    }
                }

                // 检查节点是否在动画目标中
                for (const auto& [anim_handle, anim_data] : output->animations) {
                    for (const auto& channel : model.animations[0].channels) { // 假设只有一个动画
                        if (channel.target_node == static_cast<int>(i)) {
                            scene_node.is_animation_target = true;
                            break;
                        }
                    }
                }
            }

            // 找到根节点
            for (size_t i = 0; i < output->nodes.size(); ++i) {
                if (output->nodes[i].parent_index == -1) {
                    output->root_nodes.push_back(static_cast<int>(i));
                }
            }

            return true;
        }


// 识别需要升格为骨骼的附属物节点
        void GltfProcessor::Impl::IdentifyAttachmentNodes(UnifiedSkeletonData& data) {
            std::cout << "\n=== 识别附属物节点 ===" << std::endl;

            // 构建父子关系映射
            std::unordered_map<int, int> parent_map;
            for (size_t i = 0; i < model.nodes.size(); ++i) {
                for (int child_idx : model.nodes[i].children) {
                    parent_map[child_idx] = static_cast<int>(i);
                }
            }

            // ==========================================================
            // ===               +++ 替换下面的整个循环 +++             ===
            // ==========================================================
            // 旧的错误逻辑是检查父节点是不是骨骼，这是不准确的。
            // 新的正确逻辑是：一个节点是附属物，当且仅当它自身不在 skin.joints 列表中，
            // 但它的父节点却在。这才是“附属”的真正定义。

            std::set<int> all_skin_joints;
            for (const auto& skin : model.skins) {
                all_skin_joints.insert(skin.joints.begin(), skin.joints.end());
            }

            for (const auto& anim : model.animations) {
                for (const auto& channel : anim.channels) {
                    if (channel.target_path == "translation" ||
                        channel.target_path == "rotation" ||
                        channel.target_path == "scale") {

                        int target_node = channel.target_node;

                        // 检查这个节点本身是否是 skin 的一部分
                        if (all_skin_joints.count(target_node) > 0) {
                            continue; // 它是真正的骨骼，不是附属物
                        }

                        // 向上追溯，看它的父节点是否是 skin 的一部分
                        int current = target_node;
                        while (parent_map.count(current) > 0) {
                            int parent = parent_map[current];
                            if (all_skin_joints.count(parent) > 0) {
                                // 父节点是骨骼，但它自己不是 -> 它是附属物
                                data.attachment_nodes.insert(target_node);
                                data.attachment_to_parent[target_node] = parent;
                                std::cout << "  标记节点 " << target_node << " 为附属物，父骨骼: " << parent << std::endl;
                                break;
                            }
                            current = parent;
                        }
                    }
                }
            }
            // ==========================================================
            // ===                  +++ 替换结束 +++                    ===
            // ==========================================================


            std::cout << "识别到 " << data.attachment_nodes.size() << " 个附属物节点" << std::endl;
        }

// 规范化关节名称
        std::string GltfProcessor::Impl::NormalizeJointName(const std::string& name, int node_idx, const UnifiedSkeletonData& data) {
            std::string normalized = name;

            // 如果名称为空，生成默认名称
            if (normalized.empty()) {
                normalized = "node_" + std::to_string(node_idx);
            }

            // 移除空格和非法字符
            normalized.erase(std::remove_if(normalized.begin(), normalized.end(),
                                            [](char c) { return std::isspace(c) || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|'; }),
                             normalized.end());

            // 添加前缀以确保唯一性
            if (data.attachment_nodes.count(node_idx) > 0) {
                normalized = "attach_" + normalized;
            } else {
                normalized = "jnt_" + normalized;
            }

            return normalized;
        }

// 构建包含附属物的统一骨架
        void GltfProcessor::Impl::BuildUnifiedSkeletonWithAttachments(UnifiedSkeletonData& data) {
            std::cout << "\n=== 构建统一骨架 ===" << std::endl;

            // 不需要调用 IdentifyAttachmentNodes，直接使用所有 unified_joints

            // 构建父节点映射
            std::vector<int> parent_map(model.nodes.size(), -1);
            for (size_t i = 0; i < model.nodes.size(); ++i) {
                for (int child_idx : model.nodes[i].children) {
                    parent_map[child_idx] = static_cast<int>(i);
                }
            }

            // 找到根关节
            std::vector<int> root_joints;
            for (int joint_idx : data.unified_joints) {
                if (parent_map[joint_idx] == -1 || data.unified_joints.count(parent_map[joint_idx]) == 0) {
                    root_joints.push_back(joint_idx);
                    std::cout << "找到根关节: " << joint_idx
                              << " (" << model.nodes[joint_idx].name << ")" << std::endl;
                }
            }

            // 创建原始骨架
            ozz::animation::offline::RawSkeleton raw_skeleton;
            raw_skeleton.roots.resize(root_joints.size());

            // 递归构建骨架层级
            std::function<void(int, ozz::animation::offline::RawSkeleton::Joint*)> build_joint_hierarchy =
                    [&](int node_idx, ozz::animation::offline::RawSkeleton::Joint* joint) {
                        const auto& node = model.nodes[node_idx];

                        // 使用原始名称或生成默认名称
                        if (node.name.empty()) {
                            joint->name = ("joint_" + std::to_string(node_idx)).c_str();
                        } else {
                            joint->name = node.name.c_str();
                        }

                        std::cout << "构建关节: " << node_idx << " -> " << joint->name << std::endl;

                        // 设置局部变换
                        joint->transform = GetNodeTransform(node);

                        // 递归处理子节点
                        for (int child_idx : node.children) {
                            if (data.unified_joints.count(child_idx) > 0) {
                                joint->children.resize(joint->children.size() + 1);
                                build_joint_hierarchy(child_idx, &joint->children.back());
                            }
                        }
                    };

            // 构建所有根关节的层级
            for (size_t i = 0; i < root_joints.size(); ++i) {
                build_joint_hierarchy(root_joints[i], &raw_skeleton.roots[i]);
            }

            // 验证并构建最终骨架
            if (raw_skeleton.Validate()) {
                ozz::animation::offline::SkeletonBuilder builder;
                auto skeleton = builder(raw_skeleton);

                data.skeleton_handle = output->handle_generator.Generate<SkeletonTag>();
                output->skeletons[data.skeleton_handle] = std::move(skeleton);

                output->metadata.stats.total_bones = output->skeletons[data.skeleton_handle]->num_joints();

                std::cout << "成功构建统一骨架，包含 "
                          << output->skeletons[data.skeleton_handle]->num_joints()
                          << " 个关节" << std::endl;
            } else {
                ReportError("Failed to validate unified skeleton");
            }
        }
// 采样节点在特定时间的变换
        glm::mat4 GltfProcessor::Impl::SampleNodeTransform(const tinygltf::Animation& anim,
                                                           int node_idx,
                                                           float time) const {
            glm::vec3 translation(0.0f);
            glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
            glm::vec3 scale(1.0f);

            // 从节点获取默认变换
            if (node_idx >= 0 && node_idx < static_cast<int>(model.nodes.size())) {
                const auto& node = model.nodes[node_idx];
                if (!node.translation.empty()) {
                    translation = glm::vec3(
                            static_cast<float>(node.translation[0]),
                            static_cast<float>(node.translation[1]),
                            static_cast<float>(node.translation[2])
                    );
                }
                if (!node.rotation.empty()) {
                    rotation = glm::quat(
                            static_cast<float>(node.rotation[3]),  // w
                            static_cast<float>(node.rotation[0]),  // x
                            static_cast<float>(node.rotation[1]),  // y
                            static_cast<float>(node.rotation[2])   // z
                    );
                }
                if (!node.scale.empty()) {
                    scale = glm::vec3(
                            static_cast<float>(node.scale[0]),
                            static_cast<float>(node.scale[1]),
                            static_cast<float>(node.scale[2])
                    );
                }
            }

            // 查找并应用动画
            for (const auto& channel : anim.channels) {
                if (channel.target_node != node_idx) continue;
                if (channel.sampler < 0 || channel.sampler >= static_cast<int>(anim.samplers.size())) continue;

                const auto& sampler = anim.samplers[channel.sampler];
                auto times = GetAccessorData<float>(sampler.input);

                if (times.empty()) continue;

                // 查找时间点
                size_t idx = 0;
                for (size_t i = 0; i < times.size() - 1; ++i) {
                    if (time >= times[i] && time <= times[i + 1]) {
                        idx = i;
                        break;
                    }
                }

                // 计算插值因子
                float t = 0.0f;
                if (idx < times.size() - 1 && times[idx + 1] > times[idx]) {
                    t = (time - times[idx]) / (times[idx + 1] - times[idx]);
                }

                // 应用动画值
                if (channel.target_path == "translation") {
                    auto translations = GetAccessorData<glm::vec3>(sampler.output);
                    if (idx < translations.size()) {
                        if (idx < translations.size() - 1) {
                            translation = glm::mix(translations[idx], translations[idx + 1], t);
                        } else {
                            translation = translations[idx];
                        }
                    }
                } else if (channel.target_path == "rotation") {
                    auto rotations = GetAccessorData<glm::vec4>(sampler.output);
                    if (idx < rotations.size()) {
                        glm::quat q0(rotations[idx].w, rotations[idx].x, rotations[idx].y, rotations[idx].z);
                        if (idx < rotations.size() - 1) {
                            glm::quat q1(rotations[idx + 1].w, rotations[idx + 1].x, rotations[idx + 1].y, rotations[idx + 1].z);
                            rotation = glm::slerp(q0, q1, t);
                        } else {
                            rotation = q0;
                        }
                    }
                } else if (channel.target_path == "scale") {
                    auto scales = GetAccessorData<glm::vec3>(sampler.output);
                    if (idx < scales.size()) {
                        if (idx < scales.size() - 1) {
                            scale = glm::mix(scales[idx], scales[idx + 1], t);
                        } else {
                            scale = scales[idx];
                        }
                    }
                }
            }

            // 构建变换矩阵
            glm::mat4 T = glm::translate(glm::mat4(1.0f), translation);
            glm::mat4 R = glm::mat4_cast(rotation);
            glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);

            return T * R * S;
        }

// 计算骨骼的世界矩阵
        void GltfProcessor::Impl::ComputeSkeletonWorldMatrices(const UnifiedSkeletonData& unified_data,
                                                               const std::unordered_map<int, ozz::math::Transform>& local_transforms,
                                                               std::vector<glm::mat4>& world_matrices) const {
            auto skeleton_it = output->skeletons.find(unified_data.skeleton_handle);
            if (skeleton_it == output->skeletons.end() || !skeleton_it->second) return;

            const auto& skeleton = skeleton_it->second;
            world_matrices.resize(skeleton->num_joints(), glm::mat4(1.0f));

            // 递归计算世界矩阵
            std::function<void(int, const glm::mat4&)> compute_world_matrix =
                    [&](int ozz_joint_idx, const glm::mat4& parent_world) {
                        // 查找对应的GLTF节点
                        int gltf_node_idx = -1;
                        for (const auto& [gltf_idx, ozz_idx] : unified_data.gltf_node_to_ozz_joint) {
                            if (ozz_idx == ozz_joint_idx) {
                                gltf_node_idx = gltf_idx;
                                break;
                            }
                        }

                        // 获取局部变换
                        glm::mat4 local_matrix(1.0f);
                        if (gltf_node_idx >= 0 && local_transforms.count(gltf_node_idx) > 0) {
                            const auto& transform = local_transforms.at(gltf_node_idx);
                            glm::mat4 T = glm::translate(glm::mat4(1.0f), ToGLM(transform.translation));
                            glm::mat4 R = glm::mat4_cast(ToGLM(transform.rotation));
                            glm::mat4 S = glm::scale(glm::mat4(1.0f), ToGLM(transform.scale));
                            local_matrix = T * R * S;
                        }

                        // 计算世界矩阵
                        world_matrices[ozz_joint_idx] = parent_world * local_matrix;

                        // 递归处理子节点
                        const auto& parents = skeleton->joint_parents();
                        for (int i = 0; i < skeleton->num_joints(); ++i) {
                            if (parents[i] == ozz_joint_idx) {
                                compute_world_matrix(i, world_matrices[ozz_joint_idx]);
                            }
                        }
                    };

            // 从根节点开始计算
            const auto& parents = skeleton->joint_parents();
            for (int i = 0; i < skeleton->num_joints(); ++i) {
                if (parents[i] == ozz::animation::Skeleton::kNoParent) {
                    compute_world_matrix(i, glm::mat4(1.0f));
                }
            }
        }

// 烘焙并构建动画
        bool GltfProcessor::Impl::BakeAndBuildAnimation(const tinygltf::Animation& gltf_anim,
                                                        UnifiedSkeletonData& unified_data,
                                                        AnimationHandle& out_handle) {
            std::cout << "\n=== 烘焙动画: " << gltf_anim.name << " ===" << std::endl;

            auto skeleton_it = output->skeletons.find(unified_data.skeleton_handle);
            if (skeleton_it == output->skeletons.end() || !skeleton_it->second) {
                ReportError("Unified skeleton not found for baking");
                return false;
            }

            const auto& skeleton = skeleton_it->second;

            // 1. 收集所有时间点
            std::set<float> all_times;
            for (const auto& channel : gltf_anim.channels) {
                if (channel.sampler < 0 || channel.sampler >= static_cast<int>(gltf_anim.samplers.size())) continue;

                const auto& sampler = gltf_anim.samplers[channel.sampler];
                auto times = GetAccessorData<float>(sampler.input);
                all_times.insert(times.begin(), times.end());
            }

            float duration = all_times.empty() ? 1.0f : *all_times.rbegin();
            float fps = 30.0f; // 采样率
            float time_step = 1.0f / fps;

            std::cout << "动画时长: " << duration << "秒，采样率: " << fps << " FPS" << std::endl;

            // 2. 创建原始动画
            ozz::animation::offline::RawAnimation raw_animation;
            raw_animation.name = gltf_anim.name.c_str();
            raw_animation.duration = duration;
            raw_animation.tracks.resize(skeleton->num_joints());

            // 3. 逐帧烘焙
            int num_samples = static_cast<int>(std::ceil(duration * fps)) + 1;
            std::cout << "总采样数: " << num_samples << std::endl;

            for (int sample = 0; sample < num_samples; ++sample) {
                float time = std::min(sample * time_step, duration);

                // 3a. 采样所有节点的局部变换
                std::unordered_map<int, ozz::math::Transform> local_transforms;

                for (const auto& [gltf_idx, ozz_idx] : unified_data.gltf_node_to_ozz_joint) {
                    glm::mat4 local_matrix = SampleNodeTransform(gltf_anim, gltf_idx, time);

                    // 分解矩阵到TRS
                    glm::vec3 scale;
                    glm::quat rotation;
                    glm::vec3 translation;
                    glm::vec3 skew;
                    glm::vec4 perspective;

                    if (glm::decompose(local_matrix, scale, rotation, translation, skew, perspective)) {
                        ozz::math::Transform transform;
                        transform.translation = ToOZZ(translation);
                        transform.rotation = ToOZZ(rotation);
                        transform.scale = ToOZZ(scale);
                        local_transforms[gltf_idx] = transform;
                    }
                }

                // 3b. 对于附属物节点，需要特殊处理
                for (int attach_node : unified_data.attachment_nodes) {
                    if (local_transforms.count(attach_node) == 0) {
                        // 如果附属物节点没有动画，使用其默认变换
                        const auto& node = model.nodes[attach_node];
                        local_transforms[attach_node] = GetNodeTransform(node);
                    }
                }

                // 3c. 将局部变换添加到动画轨道
                for (const auto& [gltf_idx, ozz_idx] : unified_data.gltf_node_to_ozz_joint) {
                    if (local_transforms.count(gltf_idx) > 0) {
                        const auto& transform = local_transforms[gltf_idx];
                        auto& track = raw_animation.tracks[ozz_idx];

                        track.translations.push_back({time, transform.translation});
                        track.rotations.push_back({time, transform.rotation});
                        track.scales.push_back({time, transform.scale});
                    }
                }
            }

            // 4. 确保所有轨道都有数据
            for (int i = 0; i < skeleton->num_joints(); ++i) {
                auto& track = raw_animation.tracks[i];

                if (track.translations.empty()) {
                    // 使用绑定姿势
                    ozz::math::Transform bind_pose;
                    bind_pose.translation = ozz::math::Float3(0.0f, 0.0f, 0.0f);
                    bind_pose.rotation = ozz::math::Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
                    bind_pose.scale = ozz::math::Float3(1.0f, 1.0f, 1.0f);

                    // 尝试从GLTF节点获取绑定姿势
                    for (const auto& [gltf_idx, ozz_idx] : unified_data.gltf_node_to_ozz_joint) {
                        if (ozz_idx == i && gltf_idx < static_cast<int>(model.nodes.size())) {
                            bind_pose = GetNodeTransform(model.nodes[gltf_idx]);
                            break;
                        }
                    }

                    track.translations.push_back({0.0f, bind_pose.translation});
                    track.rotations.push_back({0.0f, bind_pose.rotation});
                    track.scales.push_back({0.0f, bind_pose.scale});
                }
            }

            // 5. 构建最终动画
            if (raw_animation.Validate()) {
                ozz::animation::offline::AnimationBuilder builder;
                auto animation = builder(raw_animation);

                if (animation) {
                    out_handle = output->handle_generator.Generate<AnimationTag>();
                    AnimationData& anim_data = output->animations[out_handle];

                    anim_data.name = gltf_anim.name.c_str();
                    anim_data.duration = raw_animation.duration;
                    anim_data.target_skeleton = unified_data.skeleton_handle;
                    anim_data.skeletal_animation.reset(animation.release());

                    output->metadata.stats.total_animations++;

                    std::cout << "成功烘焙动画，时长: " << anim_data.duration << "秒" << std::endl;
                    return true;
                }
            }

            ReportError("Failed to bake animation");
            return false;
        }

    } // namespace asset
} // namespace spartan