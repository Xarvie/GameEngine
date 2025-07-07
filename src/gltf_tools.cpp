/*
 * 文件: gltf_tools.cpp
 * 描述: GltfTools 处理器的商业级实现 ([最终修正版])。
 * - 分离蒙皮骨架和静态场景图的处理，为高性能渲染管线优化。
 * - [修正] 序列化动画时，将文件名中的空格替换为下划线。
 * - [修正] 修复了scene_local_transforms未初始化的问题
 */
#include "gltf_tools.h"

#include <filesystem>
#include <iostream>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <set>

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)  // unreachable code
#pragma warning(disable : 4267)  // conversion from 'size_t' to 'type'
#endif                           // _MSC_VER
#include "tiny_gltf.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif  // _MSC_VER


#include "ozz/animation/offline/animation_builder.h"
#include "ozz/animation/offline/raw_animation_utils.h"
#include "ozz/animation/offline/skeleton_builder.h"
#include "ozz/animation/runtime/skeleton_utils.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/io/stream.h"
#include "ozz/base/log.h"
#include "ozz/base/maths/math_ex.h"
#include "ozz/base/maths/simd_math.h"
#include "ozz/base/maths/simd_math_archive.h"
#include "ozz/options/options.h"
#include "example_util.h"

namespace GltfTools {

// Processor 构造与析构
    Processor::Processor(const ozz::string& gltf_path, const ozz::string& output_path)
            : gltf_path_(gltf_path),
              output_path_(output_path),
              model_(ozz::make_unique<tinygltf::Model>()),
              mesh_asset_(ozz::make_unique<ozz::sample::MeshAsset>()),
              material_set_(ozz::make_unique<ozz::sample::MaterialSet>()) {}

    Processor::~Processor() = default;

// 主流程
    bool Processor::Run() {
        ozz::log::Out() << "GltfTools Processor starting..." << std::endl;
        if (!LoadGltfModel()) return false;
        if (!PreprocessNames()) return false;
        if (!ExtractSkinningSkeleton()) return false;
        if (!ExtractSceneGraph()) return false;
        if (!ExtractMeshesAndMaterials()) return false;
        if (!ExtractAnimations()) return false;
        if (!SerializeAssets()) return false;
        ozz::log::Out() << "GltfTools Processor finished successfully." << std::endl;
        return true;
    }

// 阶段一: 加载与预处理
    bool Processor::LoadGltfModel() {
        tinygltf::TinyGLTF loader;
        std::string err, warn;
        bool res;

        auto image_loader = [](tinygltf::Image*, const int, std::string*,
                               std::string*, int, int, const unsigned char*, int,
                               void*) { return true; };
        loader.SetImageLoader(image_loader, nullptr);

        ozz::log::Out() << "Loading glTF file: " << gltf_path_ << std::endl;
        std::string std_gltf_path = gltf_path_.c_str();

        res = std_gltf_path.rfind(".glb") != std::string::npos
              ? loader.LoadBinaryFromFile(model_.get(), &err, &warn, std_gltf_path)
              : loader.LoadASCIIFromFile(model_.get(), &err, &warn, std_gltf_path);

        if (!warn.empty()) ozz::log::Log() << "glTF WARNING: " << warn << std::endl;
        if (!err.empty()) ozz::log::Err() << "glTF ERROR: " << err << std::endl;
        if (!res) {
            ozz::log::Err() << "Failed to parse glTF file." << std::endl;
            return false;
        }
        return true;
    }

    bool Processor::PreprocessNames() {
        ozz::log::Out() << "Preprocessing names..." << std::endl;
        std::set<std::string> node_names;
        for (size_t i = 0; i < model_->nodes.size(); ++i) {
            auto& node = model_->nodes[i];
            std::string original_name = node.name;
            if (node.name.empty()) {
                node.name = "node_" + std::to_string(i);
            }
            std::string base_name = node.name;
            int suffix = 0;
            while (node_names.count(node.name)) {
                node.name = base_name + "_" + std::to_string(suffix++);
            }
            if (original_name != node.name) {
                ozz::log::LogV() << "Node #" << i << " renamed from \"" << original_name
                                 << "\" to \"" << node.name << "\" to avoid duplicates." << std::endl;
            }
            node_names.insert(node.name);
        }

        std::set<std::string> anim_names;
        for (size_t i = 0; i < model_->animations.size(); ++i) {
            auto& anim = model_->animations[i];
            std::string original_name = anim.name;
            if (anim.name.empty()) {
                anim.name = "animation_" + std::to_string(i);
            }
            std::string base_name = anim.name;
            int suffix = 0;
            while (anim_names.count(anim.name)) {
                anim.name = base_name + "_" + std::to_string(suffix++);
            }
            if (original_name != anim.name) {
                ozz::log::LogV() << "Animation #" << i << " renamed from \"" << original_name
                                 << "\" to \"" << anim.name << "\" to avoid duplicates." << std::endl;
            }
            anim_names.insert(anim.name);
        }
        return true;
    }

// 阶段二: 提取纯蒙皮骨架
    bool Processor::ExtractSkinningSkeleton() {
        ozz::log::Out() << "Extracting skinning skeleton..." << std::endl;

        std::set<int> skin_joints;
        for (const auto& skin : model_->skins) {
            skin_joints.insert(skin.joints.begin(), skin.joints.end());
        }

        if (skin_joints.empty()) {
            ozz::log::Log() << "No skins found in glTF file. No skeleton will be generated." << std::endl;
            return true;
        }

        std::vector<int> skeleton_roots;
        std::vector<int> parent_map(model_->nodes.size(), -1);
        for (size_t i = 0; i < model_->nodes.size(); ++i) {
            for (int child_idx : model_->nodes[i].children) {
                parent_map[child_idx] = static_cast<int>(i);
            }
        }

        for (int joint_idx : skin_joints) {
            if (parent_map[joint_idx] == -1 || skin_joints.count(parent_map[joint_idx]) == 0) {
                skeleton_roots.push_back(joint_idx);
            }
        }

        ozz::animation::offline::RawSkeleton raw_skeleton;
        raw_skeleton.roots.resize(skeleton_roots.size());
        for (size_t i = 0; i < skeleton_roots.size(); ++i) {
            BuildSkeletonHierarchy(skeleton_roots[i], &raw_skeleton.roots[i], skin_joints);
        }

        if (!raw_skeleton.Validate()) {
            ozz::log::Err() << "Raw skeleton is not valid." << std::endl;
            return false;
        }

        ozz::animation::offline::SkeletonBuilder builder;
        skeleton_ = builder(raw_skeleton);
        if (!skeleton_) {
            ozz::log::Err() << "SkeletonBuilder failed." << std::endl;
            return false;
        }

        gltf_joint_node_to_ozz_joint_map_.clear();
        for (int i = 0; i < skeleton_->num_joints(); ++i) {
            const char* joint_name = skeleton_->joint_names()[i];
            for (int joint_idx : skin_joints) {
                if (model_->nodes[joint_idx].name == joint_name) {
                    gltf_joint_node_to_ozz_joint_map_[joint_idx] = i;
                    break;
                }
            }
        }

        ozz::log::Out() << "Skinning skeleton extracted with " << skeleton_->num_joints() << " joints." << std::endl;
        return true;
    }

    void Processor::BuildSkeletonHierarchy(int _node_index,
                                           ozz::animation::offline::RawSkeleton::Joint* _joint,
                                           const std::set<int>& _allowed_joints) {
        const auto& node = model_->nodes[_node_index];
        _joint->name = node.name.c_str();
        GetNodeTransform(node, &_joint->transform);

        for (int child_idx : node.children) {
            if (_allowed_joints.count(child_idx)) {
                _joint->children.resize(_joint->children.size() + 1);
                BuildSkeletonHierarchy(child_idx, &_joint->children.back(), _allowed_joints);
            }
        }
    }

// 阶段三: 提取静态场景图
    bool Processor::ExtractSceneGraph() {
        ozz::log::Out() << "Extracting scene graph..." << std::endl;

        std::set<int> scene_node_indices;
        std::vector<int> stack;
        int defaultSceneIdx = model_->defaultScene > -1 ? model_->defaultScene : 0;
        if (defaultSceneIdx < (int)model_->scenes.size()) {
            const auto& scene = model_->scenes[defaultSceneIdx];
            stack.insert(stack.end(), scene.nodes.begin(), scene.nodes.end());
        }

        while(!stack.empty()){
            int current_idx = stack.back();
            stack.pop_back();
            if(scene_node_indices.find(current_idx) == scene_node_indices.end()){
                scene_node_indices.insert(current_idx);
                const auto& node = model_->nodes[current_idx];
                stack.insert(stack.end(), node.children.begin(), node.children.end());
            }
        }

        if (scene_node_indices.empty()) {
            ozz::log::Log() << "No nodes in default scene. Scene graph will be empty." << std::endl;
            return true;
        }

        mesh_asset_->scene_nodes.resize(model_->nodes.size());

        std::vector<int> parent_map(model_->nodes.size(), -1);
        for (size_t i = 0; i < model_->nodes.size(); ++i) {
            if (scene_node_indices.count(i)) {
                for (int child_idx : model_->nodes[i].children) {
                    parent_map[child_idx] = static_cast<int>(i);
                }
            }
        }

        for (size_t i = 0; i < model_->nodes.size(); ++i) {
            const auto& gltf_node = model_->nodes[i];
            auto& scene_node = mesh_asset_->scene_nodes[i];

            scene_node.name = gltf_node.name.c_str();
            GetNodeTransform(gltf_node, &scene_node.transform);
            if(scene_node_indices.count(i)) {
                scene_node.parent_index = parent_map[i];
            } else {
                scene_node.parent_index = -1;
            }
        }
        ozz::log::Out() << "Scene graph extracted for " << model_->nodes.size() << " total nodes." << std::endl;
        return true;
    }

// 阶段四: 提取网格和材质
    bool Processor::ExtractMeshesAndMaterials() {
        ozz::log::Out() << "Extracting meshes and materials..." << std::endl;

        for (size_t i = 0; i < model_->materials.size(); ++i) {
            const auto& gltf_mat = model_->materials[i];
            ozz::sample::RenderMaterial material;
            material.name = gltf_mat.name.empty() ? "Material_" + std::to_string(i) : gltf_mat.name;
            const auto& pbr_params = gltf_mat.pbrMetallicRoughness;
            if (pbr_params.baseColorFactor.size() == 4) {
                material.base_color_factor = ozz::math::Float4(
                        static_cast<float>(pbr_params.baseColorFactor[0]),
                        static_cast<float>(pbr_params.baseColorFactor[1]),
                        static_cast<float>(pbr_params.baseColorFactor[2]),
                        static_cast<float>(pbr_params.baseColorFactor[3]));
            }
            material.metallic_factor = static_cast<float>(pbr_params.metallicFactor);
            material.roughness_factor = static_cast<float>(pbr_params.roughnessFactor);

            if (pbr_params.baseColorTexture.index > -1) {
                const auto& tex = model_->textures[pbr_params.baseColorTexture.index];
                if (tex.source > -1) material.base_color_texture_path = model_->images[tex.source].uri.c_str();
            }
            if (pbr_params.metallicRoughnessTexture.index > -1) {
                const auto& tex = model_->textures[pbr_params.metallicRoughnessTexture.index];
                if (tex.source > -1) material.metallic_roughness_texture_path = model_->images[tex.source].uri.c_str();
            }
            if (gltf_mat.normalTexture.index > -1) {
                const auto& tex = model_->textures[gltf_mat.normalTexture.index];
                if (tex.source > -1) material.normal_texture_path = model_->images[tex.source].uri.c_str();
            }
            if (gltf_mat.occlusionTexture.index > -1) {
                const auto& tex = model_->textures[gltf_mat.occlusionTexture.index];
                if (tex.source > -1) material.occlusion_texture_path = model_->images[tex.source].uri.c_str();
            }
            if (gltf_mat.emissiveTexture.index > -1) {
                const auto& tex = model_->textures[gltf_mat.emissiveTexture.index];
                if (tex.source > -1) material.emissive_texture_path = model_->images[tex.source].uri.c_str();
            }
            material_set_->materials.push_back(material);
        }
        if (material_set_->materials.empty()) {
            material_set_->materials.resize(1);
            material_set_->materials[0].name = "Default_Material";
        }

        mesh_asset_->vertex_stride = sizeof(VertexData);

        for (size_t i = 0; i < model_->nodes.size(); ++i) {
            const auto& node = model_->nodes[i];
            if (node.mesh > -1) {
                const auto& mesh = model_->meshes[node.mesh];
                int primitive_idx_counter = 0;
                for (const auto& primitive : mesh.primitives) {
                    ProcessPrimitive(node, primitive, primitive_idx_counter++);
                }
            }
        }

        if (skeleton_ && skeleton_->num_joints() > 0) {
            mesh_asset_->inverse_bind_poses.resize(skeleton_->num_joints(), ozz::math::Float4x4::identity());
            for (const auto& skin : model_->skins) {
                if (skin.inverseBindMatrices < 0) continue;
                auto ibm_span = GetAccessorData<ozz::math::Float4x4>(*model_, skin.inverseBindMatrices);
                for (size_t i = 0; i < skin.joints.size(); ++i) {
                    int gltf_joint_idx = skin.joints[i];
                    if (gltf_joint_node_to_ozz_joint_map_.count(gltf_joint_idx)) {
                        int ozz_joint_idx = gltf_joint_node_to_ozz_joint_map_.at(gltf_joint_idx);
                        if (i < ibm_span.size()) {
                            mesh_asset_->inverse_bind_poses[ozz_joint_idx] = ibm_span[i];
                        }
                    }
                }
            }
        }
        return true;
    }

    void Processor::ProcessPrimitive(const tinygltf::Node& _node, const tinygltf::Primitive& _primitive, int _primitive_index) {
        std::map<UniqueVertex, uint32_t> unique_vertices_map_;
        if (_primitive.attributes.find("POSITION") == _primitive.attributes.end()) return;

        const bool is_skinned = _node.skin > -1;
        const int gltf_node_idx = static_cast<int>(&_node - &model_->nodes[0]);

        mesh_asset_->parts.emplace_back();
        auto& part = mesh_asset_->parts.back();
        part.material_index = _primitive.material > -1 ? _primitive.material : 0;
        part.index_offset = static_cast<uint32_t>(mesh_asset_->index_buffer.size());
        part.is_static_body = !is_skinned;
        part.scene_node_index = is_skinned ? -1 : gltf_node_idx;

        auto get_attrib = [&](const char* name) {
            auto it = _primitive.attributes.find(name);
            return it != _primitive.attributes.end() ? it->second : -1;
        };

        auto positions = GetAccessorData<ozz::math::Float3>(*model_, get_attrib("POSITION"));
        auto normals = GetAccessorData<ozz::math::Float3>(*model_, get_attrib("NORMAL"));
        auto uvs0 = GetAccessorData<ozz::math::Float2>(*model_, get_attrib("TEXCOORD_0"));
        auto tangents = GetAccessorData<ozz::math::Float4>(*model_, get_attrib("TANGENT"));
        auto weights0 = GetAccessorData<ozz::math::Float4>(*model_, get_attrib("WEIGHTS_0"));

        struct U8x4 { uint8_t v[4]; };
        struct U16x4 { uint16_t v[4]; };

        int joints0_accessor_idx = get_attrib("JOINTS_0");
        ozz::span<const U8x4> joints0_u8_vec4;
        ozz::span<const U16x4> joints0_u16_vec4;
        if (joints0_accessor_idx > -1) {
            const auto& acc = model_->accessors[joints0_accessor_idx];
            if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                joints0_u8_vec4 = GetAccessorData<U8x4>(*model_, joints0_accessor_idx);
            } else if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                joints0_u16_vec4 = GetAccessorData<U16x4>(*model_, joints0_accessor_idx);
            }
        }

        std::vector<uint32_t> indices;
        if (_primitive.indices > -1) {
            const auto& acc = model_->accessors[_primitive.indices];
            indices.reserve(acc.count);
            if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                auto data = GetAccessorData<uint32_t>(*model_, _primitive.indices);
                indices.assign(data.begin(), data.end());
            } else if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                auto data = GetAccessorData<uint16_t>(*model_, _primitive.indices);
                indices.assign(data.begin(), data.end());
            } else if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                auto data = GetAccessorData<uint8_t>(*model_, _primitive.indices);
                indices.assign(data.begin(), data.end());
            }
        } else {
            indices.resize(positions.size());
            for (size_t i = 0; i < positions.size(); ++i) indices[i] = static_cast<uint32_t>(i);
        }

        part.index_count = 0;
        for (uint32_t old_index : indices) {
            UniqueVertex unique_v{};
            unique_v.position = positions[old_index];
            unique_v.normal = old_index < normals.size() ? normals[old_index] : ozz::math::Float3{0.f, 1.f, 0.f};
            unique_v.uv0 = old_index < uvs0.size() ? uvs0[old_index] : ozz::math::Float2{0.f, 0.f};
            unique_v.tangent = old_index < tangents.size() ? tangents[old_index] : ozz::math::Float4{1.f, 0.f, 0.f, 1.f};

            if (is_skinned) {
                unique_v.joint_weights = old_index < weights0.size() ? weights0[old_index] : ozz::math::Float4{1.f, 0.f, 0.f, 0.f};
                for (int j = 0; j < 4; ++j) {
                    uint16_t joint_gltf_idx = 0;
                    if (old_index < joints0_u16_vec4.size()) {
                        joint_gltf_idx = joints0_u16_vec4[old_index].v[j];
                    } else if (old_index < joints0_u8_vec4.size()) {
                        joint_gltf_idx = joints0_u8_vec4[old_index].v[j];
                    }
                    int skin_idx = _node.skin;
                    if (skin_idx > -1 && joint_gltf_idx < model_->skins[skin_idx].joints.size()) {
                        int gltf_joint_node_idx = model_->skins[skin_idx].joints[joint_gltf_idx];
                        unique_v.joint_indices[j] = gltf_joint_node_to_ozz_joint_map_.count(gltf_joint_node_idx) ? gltf_joint_node_to_ozz_joint_map_[gltf_joint_node_idx] : 0;
                    } else {
                        unique_v.joint_indices[j] = 0;
                    }
                }
            } else {
                unique_v.joint_weights = ozz::math::Float4{1.f, 0.f, 0.f, 0.f};
                unique_v.joint_indices[0] = 0; unique_v.joint_indices[1] = 0;
                unique_v.joint_indices[2] = 0; unique_v.joint_indices[3] = 0;
            }

            auto iter = unique_vertices_map_.find(unique_v);
            if (iter != unique_vertices_map_.end()) {
                mesh_asset_->index_buffer.push_back(iter->second);
            } else {
                uint32_t new_idx = static_cast<uint32_t>(mesh_asset_->vertex_buffer.size() / sizeof(VertexData));
                unique_vertices_map_[unique_v] = new_idx;
                mesh_asset_->vertex_buffer.insert(mesh_asset_->vertex_buffer.end(),
                                                  reinterpret_cast<const uint8_t*>(&unique_v),
                                                  reinterpret_cast<const uint8_t*>(&unique_v) + sizeof(VertexData));
                mesh_asset_->index_buffer.push_back(new_idx);
            }
            part.index_count++;
        }
    }

// 阶段五: 提取动画
    bool Processor::ExtractAnimations() {
        ozz::log::Out() << "Extracting animations..." << std::endl;

        if (sampling_rate_ == 0.0f) {
            sampling_rate_ = 30.0f;
            ozz::log::LogV() << "Animation sampling rate set to default " << sampling_rate_ << "hz." << std::endl;
        }

        for (const auto& anim : model_->animations) {
            ozz::animation::offline::RawAnimation raw_animation;
            raw_animation.name = anim.name.c_str();
            raw_animation.duration = 0.f;
            raw_animation.tracks.resize(model_->nodes.size());

            std::vector<bool> node_animated(model_->nodes.size(), false);

            for (const auto& channel : anim.channels) {
                if (channel.target_node < 0) continue;

                bool valid_path = false;
                for (const char* path : {"translation", "rotation", "scale"}) {
                    if (channel.target_path == path) {
                        valid_path = true;
                        break;
                    }
                }
                if (!valid_path) continue;

                int node_index = channel.target_node;
                const auto& sampler = anim.samplers[channel.sampler];

                if (channel.target_path == "translation") {
                    SampleChannel(sampler, &raw_animation.duration, &raw_animation.tracks[node_index].translations);
                } else if (channel.target_path == "rotation") {
                    SampleChannel(sampler, &raw_animation.duration, &raw_animation.tracks[node_index].rotations);
                    for (auto& key : raw_animation.tracks[node_index].rotations) {
                        key.value = ozz::math::Normalize(key.value);
                    }
                } else if (channel.target_path == "scale") {
                    SampleChannel(sampler, &raw_animation.duration, &raw_animation.tracks[node_index].scales);
                }
                node_animated[node_index] = true;
            }

            for (size_t i = 0; i < model_->nodes.size(); ++i) {
                if (!node_animated[i]) {
                    const auto& rest_pose_transform = mesh_asset_->scene_nodes[i].transform;

                    auto& translations = raw_animation.tracks[i].translations;
                    if (translations.empty()) {
                        translations.resize(1);
                        translations[0] = {0.f, rest_pose_transform.translation};
                    }
                    auto& rotations = raw_animation.tracks[i].rotations;
                    if (rotations.empty()) {
                        rotations.resize(1);
                        rotations[0] = {0.f, rest_pose_transform.rotation};
                    }
                    auto& scales = raw_animation.tracks[i].scales;
                    if (scales.empty()) {
                        scales.resize(1);
                        scales[0] = {0.f, rest_pose_transform.scale};
                    }
                }
            }

            if (!raw_animation.Validate()) {
                ozz::log::Err() << "Raw animation " << raw_animation.name << " is not valid." << std::endl;
                continue;
            }

            ozz::animation::offline::AnimationBuilder builder;
            auto animation = builder(raw_animation);
            if (animation) {
                animations_.push_back(std::move(animation));
                ozz::log::Out() << "Successfully processed animation: " << raw_animation.name
                                << " (duration: " << raw_animation.duration << "s)" << std::endl;
            }
        }
        return true;
    }

// 阶段六: 序列化
    bool Processor::SerializeAssets() {
        try {
            std::filesystem::path output_p(output_path_.c_str());
            if (output_p.has_parent_path()) {
                std::filesystem::create_directories(output_p.parent_path());
            }
        } catch (const std::filesystem::filesystem_error& e) {
            ozz::log::Err() << "Failed to create output directory: " << e.what() << std::endl;
            return false;
        }

        const ozz::string materials_path = output_path_ + ".materials.ozz";
        ozz::log::Out() << "Serializing materials to " << materials_path << std::endl;
        ozz::io::File materials_file(materials_path.c_str(), "wb");
        if (!materials_file.opened()) { ozz::log::Err() << "Failed to open " << materials_path << std::endl; return false; }
        ozz::io::OArchive materials_archive(&materials_file);
        materials_archive << *material_set_;

        const ozz::string mesh_path = output_path_ + ".mesh.ozz";
        ozz::log::Out() << "Serializing mesh asset to " << mesh_path << std::endl;
        ozz::io::File mesh_file(mesh_path.c_str(), "wb");
        if (!mesh_file.opened()) { ozz::log::Err() << "Failed to open " << mesh_path << std::endl; return false; }
        ozz::io::OArchive mesh_archive(&mesh_file);
        mesh_archive << *mesh_asset_;

        if (skeleton_ && skeleton_->num_joints() > 0) {
            const ozz::string skel_path = output_path_ + ".skeleton.ozz";
            ozz::log::Out() << "Serializing skeleton to " << skel_path << std::endl;
            ozz::io::File file(skel_path.c_str(), "wb");
            if (!file.opened()) { ozz::log::Err() << "Failed to open " << skel_path << std::endl; return false; }
            ozz::io::OArchive archive(&file);
            archive << *skeleton_;
        }

        if (!animations_.empty()) {
            for (size_t i = 0; i < animations_.size(); ++i) {
                const auto& anim = animations_[i];
                std::string anim_name = anim->name();
                std::replace(anim_name.begin(), anim_name.end(), '|', '_');
                std::replace(anim_name.begin(), anim_name.end(), ':', '_');
                // [修正] 将空格替换为下划线
                std::replace(anim_name.begin(), anim_name.end(), ' ', '_');
                ozz::string anim_path = output_path_ + "." + anim_name.c_str() + ".anim.ozz";

                ozz::log::Out() << "Serializing animation to " << anim_path << std::endl;
                ozz::io::File file(anim_path.c_str(), "wb");
                if (!file.opened()) { ozz::log::Err() << "Failed to open " << anim_path << std::endl; continue; }
                ozz::io::OArchive archive(&file);
                archive << *anim;
            }
        }

        return true;
    }

// 辅助函数实现
    template <typename T>
    ozz::span<const T> Processor::GetAccessorData(const tinygltf::Model& _model, int _accessor_index) {
        if (_accessor_index < 0) return ozz::span<const T>();
        const auto& accessor = _model.accessors[_accessor_index];
        const auto& buffer_view = _model.bufferViews[accessor.bufferView];
        const auto& buffer = _model.buffers[buffer_view.buffer];
        const T* begin = reinterpret_cast<const T*>(buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset);
        return ozz::span<const T>(begin, accessor.count);
    }

    bool Processor::GetNodeTransform(const tinygltf::Node& _node, ozz::math::Transform* _transform) {
        *_transform = ozz::math::Transform::identity();
        if (!_node.matrix.empty()) {
            const ozz::math::Float4x4 matrix = {
                    {ozz::math::simd_float4::Load(static_cast<float>(_node.matrix[0]), static_cast<float>(_node.matrix[1]), static_cast<float>(_node.matrix[2]), static_cast<float>(_node.matrix[3])),
                     ozz::math::simd_float4::Load(static_cast<float>(_node.matrix[4]), static_cast<float>(_node.matrix[5]), static_cast<float>(_node.matrix[6]), static_cast<float>(_node.matrix[7])),
                     ozz::math::simd_float4::Load(static_cast<float>(_node.matrix[8]), static_cast<float>(_node.matrix[9]), static_cast<float>(_node.matrix[10]), static_cast<float>(_node.matrix[11])),
                     ozz::math::simd_float4::Load(static_cast<float>(_node.matrix[12]), static_cast<float>(_node.matrix[13]), static_cast<float>(_node.matrix[14]), static_cast<float>(_node.matrix[15]))}};
            if (!ToAffine(matrix, _transform)) {
                ozz::log::Err() << "Failed to extract transformation from node \"" << _node.name << "\"." << std::endl;
                return false;
            }
            return true;
        }
        if (!_node.translation.empty()) {
            _transform->translation = ozz::math::Float3(static_cast<float>(_node.translation[0]), static_cast<float>(_node.translation[1]), static_cast<float>(_node.translation[2]));
        }
        if (!_node.rotation.empty()) {
            // glTF stores quaternions as [x, y, z, w], same as ozz constructor order
            float x = static_cast<float>(_node.rotation[0]);
            float y = static_cast<float>(_node.rotation[1]);
            float z = static_cast<float>(_node.rotation[2]);
            float w = static_cast<float>(_node.rotation[3]);

            // Validate and normalize the quaternion
            float norm_sq = x*x + y*y + z*z + w*w;
            if (norm_sq < 0.0001f) {
                // Invalid quaternion, use identity
                ozz::log::Err() << "Invalid quaternion in node \"" << _node.name
                                << "\": zero magnitude. Using identity rotation." << std::endl;
                _transform->rotation = ozz::math::Quaternion::identity();
            } else if (std::abs(norm_sq - 1.0f) > 0.001f) {
                // Quaternion not normalized, normalize it
                float norm = std::sqrt(norm_sq);
                x /= norm;
                y /= norm;
                z /= norm;
                w /= norm;
                ozz::log::LogV() << "Normalizing quaternion for node \"" << _node.name << "\"" << std::endl;
            }

            _transform->rotation = ozz::math::Quaternion(x, y, z, w);

            // Debug log to check quaternion values
            ozz::log::LogV() << "Node \"" << _node.name << "\" quaternion: ["
                             << _transform->rotation.x << ", " << _transform->rotation.y << ", "
                             << _transform->rotation.z << ", " << _transform->rotation.w << "]" << std::endl;
        }
        if (!_node.scale.empty()) {
            _transform->scale = ozz::math::Float3(static_cast<float>(_node.scale[0]), static_cast<float>(_node.scale[1]), static_cast<float>(_node.scale[2]));
        }
        return true;
    }

    template <typename _KeyframesType>
    bool Processor::SampleChannel(const tinygltf::AnimationSampler& _sampler,
                                  float* _duration, _KeyframesType* _keyframes) {
        if (_sampler.interpolation.empty()) {
            ozz::log::Err() << "Invalid sampler interpolation." << std::endl;
            return false;
        }

        bool valid = false;
        if (_sampler.interpolation == "LINEAR") {
            valid = SampleLinearChannel(_sampler, _duration, _keyframes);
        } else if (_sampler.interpolation == "STEP") {
            valid = SampleStepChannel(_sampler, _duration, _keyframes);
        } else if (_sampler.interpolation == "CUBICSPLINE") {
            valid = SampleCubicSplineChannel(_sampler, _duration, _keyframes);
        } else {
            ozz::log::Err() << "Unknown interpolation type: " << _sampler.interpolation << std::endl;
            return false;
        }

        if (valid) {
            valid = ValidateAndCleanKeyframes(_keyframes);
        }

        return valid;
    }

    template <typename _KeyframesType>
    bool Processor::SampleLinearChannel(const tinygltf::AnimationSampler& _sampler,
                                        float* _duration, _KeyframesType* _keyframes) {
        const auto timestamps = GetAccessorData<float>(*model_, _sampler.input);
        if (timestamps.empty()) return true;

        *_duration = ozz::math::Max(*_duration, timestamps.back());

        const auto values = GetAccessorData<typename _KeyframesType::value_type::Value>(*model_, _sampler.output);
        if (values.size() != timestamps.size()) {
            ozz::log::Err() << "Inconsistent number of keys in animation channel." << std::endl;
            return false;
        }

        _keyframes->reserve(_keyframes->size() + timestamps.size());
        for (size_t i = 0; i < timestamps.size(); ++i) {
            _keyframes->push_back({timestamps[i], values[i]});
        }

        return true;
    }

    template <typename _KeyframesType>
    bool Processor::SampleStepChannel(const tinygltf::AnimationSampler& _sampler,
                                      float* _duration, _KeyframesType* _keyframes) {
        const auto timestamps = GetAccessorData<float>(*model_, _sampler.input);
        if (timestamps.empty()) return true;

        *_duration = ozz::math::Max(*_duration, timestamps.back());

        const auto values = GetAccessorData<typename _KeyframesType::value_type::Value>(*model_, _sampler.output);
        if (values.size() != timestamps.size()) {
            ozz::log::Err() << "Inconsistent number of keys in animation channel." << std::endl;
            return false;
        }

        size_t num_keyframes = timestamps.size() > 1 ? timestamps.size() * 2 - 1 : timestamps.size();
        _keyframes->reserve(_keyframes->size() + num_keyframes);

        for (size_t i = 0; i < timestamps.size(); ++i) {
            _keyframes->push_back({timestamps[i], values[i]});

            if (i < timestamps.size() - 1) {
                float next_time = nexttowardf(timestamps[i + 1], 0.f);
                _keyframes->push_back({next_time, values[i]});
            }
        }

        return true;
    }

    template <typename _KeyframesType>
    bool Processor::SampleCubicSplineChannel(const tinygltf::AnimationSampler& _sampler,
                                             float* _duration, _KeyframesType* _keyframes) {
        const auto timestamps = GetAccessorData<float>(*model_, _sampler.input);
        if (timestamps.empty()) return true;

        const float channel_duration = timestamps.back() - timestamps.front();
        *_duration = ozz::math::Max(*_duration, timestamps.back());

        const auto& output_accessor = model_->accessors[_sampler.output];
        if (output_accessor.count % 3 != 0) {
            ozz::log::Err() << "Invalid cubic spline data format." << std::endl;
            return false;
        }

        size_t gltf_keys_count = output_accessor.count / 3;
        if (timestamps.size() != gltf_keys_count) {
            ozz::log::Err() << "Inconsistent number of keys in cubic spline channel." << std::endl;
            return false;
        }

        typedef typename _KeyframesType::value_type::Value ValueType;
        const auto values = GetAccessorData<ValueType>(*model_, _sampler.output);

        ozz::animation::offline::FixedRateSamplingTime fixed_it(channel_duration, sampling_rate_);
        _keyframes->reserve(_keyframes->size() + fixed_it.num_keys());

        size_t cubic_key0 = 0;
        for (size_t k = 0; k < fixed_it.num_keys(); ++k) {
            const float time = fixed_it.time(k) + timestamps[0];

            while (cubic_key0 + 1 < gltf_keys_count - 1 && timestamps[cubic_key0 + 1] < time) {
                cubic_key0++;
            }

            const float t0 = timestamps[cubic_key0];
            const float t1 = timestamps[cubic_key0 + 1];
            const float alpha = (time - t0) / (t1 - t0);

            const ValueType& p0 = values[cubic_key0 * 3 + 1];
            const ValueType m0 = values[cubic_key0 * 3 + 2] * (t1 - t0);
            const ValueType& p1 = values[(cubic_key0 + 1) * 3 + 1];
            const ValueType m1 = values[(cubic_key0 + 1) * 3] * (t1 - t0);

            ValueType interpolated = SampleHermiteSpline(alpha, p0, m0, p1, m1);
            _keyframes->push_back({time, interpolated});
        }

        return true;
    }

    template <typename T>
    T Processor::SampleHermiteSpline(float _alpha, const T& p0, const T& m0,
                                     const T& p1, const T& m1) {
        const float t = _alpha;
        const float t2 = t * t;
        const float t3 = t2 * t;

        const float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
        const float h10 = t3 - 2.0f * t2 + t;
        const float h01 = -2.0f * t3 + 3.0f * t2;
        const float h11 = t3 - t2;

        return p0 * h00 + m0 * h10 + p1 * h01 + m1 * h11;
    }

    template <typename _KeyframesType>
    bool Processor::ValidateAndCleanKeyframes(_KeyframesType* _keyframes) {
        if (!std::is_sorted(_keyframes->begin(), _keyframes->end(),
                            [](const auto& a, const auto& b) { return a.time < b.time; })) {
            std::sort(_keyframes->begin(), _keyframes->end(),
                      [](const auto& a, const auto& b) { return a.time < b.time; });
        }

        auto new_end = std::unique(_keyframes->begin(), _keyframes->end(),
                                   [](const auto& a, const auto& b) { return a.time == b.time; });
        _keyframes->erase(new_end, _keyframes->end());

        return true;
    }

// 显式实例化模板函数
    template bool Processor::SampleChannel(const tinygltf::AnimationSampler&, float*,
                                           ozz::animation::offline::RawAnimation::JointTrack::Translations*);
    template bool Processor::SampleChannel(const tinygltf::AnimationSampler&, float*,
                                           ozz::animation::offline::RawAnimation::JointTrack::Rotations*);
    template bool Processor::SampleChannel(const tinygltf::AnimationSampler&, float*,
                                           ozz::animation::offline::RawAnimation::JointTrack::Scales*);

}  // namespace GltfTools

// 命令行入口点
OZZ_OPTIONS_DECLARE_STRING(gltf, "Path to the input glTF or GLB file.", "", true);
OZZ_OPTIONS_DECLARE_STRING(output, "Path for the output ozz archives.", "", true);
OZZ_OPTIONS_DECLARE_FLOAT(sampling_rate, "Animation sampling rate in Hz (0 for automatic).", 0.0f, false);

int gltf2Ozz(int _argc, char* _argv[]) {
    ozz::options::ParseResult parse_result = ozz::options::ParseCommandLine(
            _argc, _argv, "3.0", "Ozz Gltf To Unified Runtime Asset Processor");
    if (parse_result != ozz::options::kSuccess) {
        return parse_result == ozz::options::kExitSuccess ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    GltfTools::Processor processor(OPTIONS_gltf.value(), OPTIONS_output.value());

    processor.SetSamplingRate(OPTIONS_sampling_rate.value());

    if (!processor.Run()) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}