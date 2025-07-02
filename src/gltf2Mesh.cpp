//----------------------------------------------------------------------------//
//                                                                            //
// GLTF2 to ozz-animation mesh converter                                      //
// Based on ozz-animation fbx2mesh.cc structure                              //
//                                                                            //
//----------------------------------------------------------------------------//

#include <algorithm>
#include <limits>
#include <fstream>
#include <iostream>
#include <cstring>

#include "mesh.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/base/containers/map.h"
#include "ozz/base/containers/vector.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/io/stream.h"
#include "ozz/base/log.h"
#include "ozz/base/maths/math_ex.h"
#include "ozz/base/maths/simd_math.h"
#include "ozz/base/memory/allocator.h"
#include "ozz/options/options.h"

// Include GLTF2 library - tinygltf with implementation
#define TINYGLTF_IMPLEMENTATION
// No support for image loading or writing (we only need mesh data)
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#include <tiny_gltf.h>

// Declares command line options with unique names to avoid conflicts.
OZZ_OPTIONS_DECLARE_STRING(gltf_file, "Specifies input GLTF2 file.", "", true)
OZZ_OPTIONS_DECLARE_STRING(gltf_skeleton,
                           "Specifies the skeleton that the skin is bound to.",
                           "", true)
OZZ_OPTIONS_DECLARE_STRING(gltf_mesh, "Specifies ozz mesh output file.", "", true)
OZZ_OPTIONS_DECLARE_BOOL(gltf_split,
                         "Split the skinned mesh into parts (number of joint "
                         "influences per vertex).",
                         true, false)
OZZ_OPTIONS_DECLARE_INT(
        gltf_max_influences,
        "Maximum number of joint influences per vertex (0 means no limitation).", 0,
        false)

namespace {

// Control point to vertex buffer remapping.
    typedef ozz::vector<uint16_t> ControlPointRemap;
    typedef ozz::vector<ControlPointRemap> ControlPointsRemap;

// Triangle indices naive sort function.
    int SortTriangles(const void* _left, const void* _right) {
        const uint16_t* left = static_cast<const uint16_t*>(_left);
        const uint16_t* right = static_cast<const uint16_t*>(_right);
        return (left[0] + left[1] + left[2]) - (right[0] + right[1] + right[2]);
    }

// Helper function to safely get buffer data with bounds checking
    bool GetBufferData(const tinygltf::Model& model,
                       const tinygltf::Accessor& accessor,
                       const unsigned char** out_data,
                       size_t* out_stride) {
        if (accessor.bufferView < 0 || accessor.bufferView >= model.bufferViews.size()) {
            ozz::log::Err() << "Invalid buffer view index: " << accessor.bufferView << std::endl;
            return false;
        }

        const tinygltf::BufferView& buffer_view = model.bufferViews[accessor.bufferView];

        if (buffer_view.buffer < 0 || buffer_view.buffer >= model.buffers.size()) {
            ozz::log::Err() << "Invalid buffer index: " << buffer_view.buffer << std::endl;
            return false;
        }

        const tinygltf::Buffer& buffer = model.buffers[buffer_view.buffer];

        // Check bounds
        size_t total_offset = buffer_view.byteOffset + accessor.byteOffset;
        size_t stride = accessor.ByteStride(buffer_view);
        size_t component_size = 0;

        // Calculate component size
        switch (accessor.componentType) {
            case TINYGLTF_COMPONENT_TYPE_BYTE:
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                component_size = 1;
                break;
            case TINYGLTF_COMPONENT_TYPE_SHORT:
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                component_size = 2;
                break;
            case TINYGLTF_COMPONENT_TYPE_INT:
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            case TINYGLTF_COMPONENT_TYPE_FLOAT:
                component_size = 4;
                break;
            default:
                component_size = 4;
                break;
        }

        // Calculate number of components
        size_t num_components = 1;
        switch (accessor.type) {
            case TINYGLTF_TYPE_SCALAR: num_components = 1; break;
            case TINYGLTF_TYPE_VEC2: num_components = 2; break;
            case TINYGLTF_TYPE_VEC3: num_components = 3; break;
            case TINYGLTF_TYPE_VEC4: num_components = 4; break;
            case TINYGLTF_TYPE_MAT2: num_components = 4; break;
            case TINYGLTF_TYPE_MAT3: num_components = 9; break;
            case TINYGLTF_TYPE_MAT4: num_components = 16; break;
            default: num_components = 1; break;
        }

        size_t total_size = total_offset + (accessor.count - 1) * stride +
                            component_size * num_components;

        if (total_size > buffer.data.size()) {
            ozz::log::Err() << "Buffer access out of bounds. Required: " << total_size
                            << ", Available: " << buffer.data.size() << std::endl;
            return false;
        }

        if (buffer.data.empty()) {
            ozz::log::Err() << "Buffer data is empty. This might indicate that external .bin file failed to load." << std::endl;
            return false;
        }

        *out_data = buffer.data.data() + total_offset;
        *out_stride = stride;
        return true;
    }

// Helper function to get accessor data from GLTF with proper error handling
    template<typename T>
    bool GetAccessorData(const tinygltf::Model& model,
                         int accessor_index,
                         ozz::vector<T>& data) {
        if (accessor_index < 0 || accessor_index >= model.accessors.size()) {
            return false;
        }

        const tinygltf::Accessor& accessor = model.accessors[accessor_index];

        const unsigned char* buffer_data;
        size_t stride;
        if (!GetBufferData(model, accessor, &buffer_data, &stride)) {
            return false;
        }

        data.resize(accessor.count);

        for (size_t i = 0; i < accessor.count; ++i) {
            const T* src = reinterpret_cast<const T*>(buffer_data + i * stride);
            data[i] = *src;
        }

        return true;
    }

// Specialized version for vector types
    template<>
    bool GetAccessorData<ozz::math::Float3>(const tinygltf::Model& model,
                                            int accessor_index,
                                            ozz::vector<ozz::math::Float3>& data) {
        if (accessor_index < 0 || accessor_index >= model.accessors.size()) {
            return false;
        }

        const tinygltf::Accessor& accessor = model.accessors[accessor_index];

        const unsigned char* buffer_data;
        size_t stride;
        if (!GetBufferData(model, accessor, &buffer_data, &stride)) {
            return false;
        }

        data.resize(accessor.count);

        for (size_t i = 0; i < accessor.count; ++i) {
            const float* src = reinterpret_cast<const float*>(buffer_data + i * stride);
            data[i] = ozz::math::Float3(src[0], src[1], src[2]);
        }

        return true;
    }

// Specialized version for Float2
    template<>
    bool GetAccessorData<ozz::math::Float2>(const tinygltf::Model& model,
                                            int accessor_index,
                                            ozz::vector<ozz::math::Float2>& data) {
        if (accessor_index < 0 || accessor_index >= model.accessors.size()) {
            return false;
        }

        const tinygltf::Accessor& accessor = model.accessors[accessor_index];

        const unsigned char* buffer_data;
        size_t stride;
        if (!GetBufferData(model, accessor, &buffer_data, &stride)) {
            return false;
        }

        data.resize(accessor.count);

        for (size_t i = 0; i < accessor.count; ++i) {
            const float* src = reinterpret_cast<const float*>(buffer_data + i * stride);
            data[i] = ozz::math::Float2(src[0], src[1]);
        }

        return true;
    }

} // namespace

bool BuildVertices(const tinygltf::Model& model,
                   const tinygltf::Primitive& primitive,
                   ControlPointsRemap* _remap,
                   ozz::sample::Mesh* _output_mesh) {

    // Get indices
    ozz::vector<uint16_t> indices;
    if (primitive.indices >= 0) {
        if (!GetAccessorData<uint16_t>(model, primitive.indices, indices)) {
            ozz::log::Err() << "Failed to read indices." << std::endl;
            return false;
        }
    }

    // Get positions (required)
    ozz::vector<ozz::math::Float3> positions;
    auto pos_it = primitive.attributes.find("POSITION");
    if (pos_it == primitive.attributes.end()) {
        ozz::log::Err() << "No position attribute found." << std::endl;
        return false;
    }
    if (!GetAccessorData<ozz::math::Float3>(model, pos_it->second, positions)) {
        ozz::log::Err() << "Failed to read positions." << std::endl;
        return false;
    }

    // Get normals
    ozz::vector<ozz::math::Float3> normals;
    auto norm_it = primitive.attributes.find("NORMAL");
    if (norm_it != primitive.attributes.end()) {
        GetAccessorData<ozz::math::Float3>(model, norm_it->second, normals);
    }

    // Generate normals if not present
    if (normals.empty()) {
        normals.resize(positions.size(), ozz::math::Float3::y_axis());
        // TODO: Implement normal generation from geometry
    }

    // Get UVs
    ozz::vector<ozz::math::Float2> uvs;
    auto uv_it = primitive.attributes.find("TEXCOORD_0");
    if (uv_it != primitive.attributes.end()) {
        GetAccessorData<ozz::math::Float2>(model, uv_it->second, uvs);
    }

    // Get tangents
    ozz::vector<ozz::math::Float4> tangents;
    auto tangent_it = primitive.attributes.find("TANGENT");
    if (tangent_it != primitive.attributes.end()) {
        // TODO: Implement Float4 accessor for tangents
    }

    // Get colors
    ozz::vector<ozz::math::Float4> colors;
    auto color_it = primitive.attributes.find("COLOR_0");
    if (color_it != primitive.attributes.end()) {
        // TODO: Implement color accessor
    }

    // Allocate control point remapping
    const int vertex_count = positions.size();
    _remap->resize(vertex_count);

    // Resize triangle indices
    const size_t triangle_count = indices.size() / 3;
    _output_mesh->triangle_indices.resize(indices.size());

    // Copy indices directly (GLTF already provides per-vertex data)
    for (size_t i = 0; i < indices.size(); ++i) {
        _output_mesh->triangle_indices[i] = indices[i];
    }

    // Build mesh part
    ozz::sample::Mesh::Part& part = _output_mesh->parts[0];

    // Reserve vertex buffers
    part.positions.reserve(vertex_count * ozz::sample::Mesh::Part::kPositionsCpnts);
    part.normals.reserve(vertex_count * ozz::sample::Mesh::Part::kNormalsCpnts);

    if (!uvs.empty()) {
        part.uvs.reserve(vertex_count * ozz::sample::Mesh::Part::kUVsCpnts);
    }
    if (!tangents.empty()) {
        part.tangents.reserve(vertex_count * ozz::sample::Mesh::Part::kTangentsCpnts);
    }
    if (!colors.empty()) {
        part.colors.reserve(vertex_count * ozz::sample::Mesh::Part::kColorsCpnts);
    }

    // Copy vertex data
    for (int i = 0; i < vertex_count; ++i) {
        // Setup control point remapping (1:1 for GLTF)
        _remap->at(i).push_back(i);

        // Positions
        part.positions.push_back(positions[i].x);
        part.positions.push_back(positions[i].y);
        part.positions.push_back(positions[i].z);

        // Normals
        part.normals.push_back(normals[i].x);
        part.normals.push_back(normals[i].y);
        part.normals.push_back(normals[i].z);

        // UVs
        if (!uvs.empty()) {
            part.uvs.push_back(uvs[i].x);
            part.uvs.push_back(uvs[i].y);
        }

        // Tangents
        if (!tangents.empty()) {
            part.tangents.push_back(tangents[i].x);
            part.tangents.push_back(tangents[i].y);
            part.tangents.push_back(tangents[i].z);
            part.tangents.push_back(tangents[i].w);
        }

        // Colors
        if (!colors.empty()) {
            const uint8_t color[4] = {
                    static_cast<uint8_t>(ozz::math::Clamp(0.f, colors[i].x * 255.f, 255.f)),
                    static_cast<uint8_t>(ozz::math::Clamp(0.f, colors[i].y * 255.f, 255.f)),
                    static_cast<uint8_t>(ozz::math::Clamp(0.f, colors[i].z * 255.f, 255.f)),
                    static_cast<uint8_t>(ozz::math::Clamp(0.f, colors[i].w * 255.f, 255.f)),
            };
            part.colors.push_back(color[0]);
            part.colors.push_back(color[1]);
            part.colors.push_back(color[2]);
            part.colors.push_back(color[3]);
        }
    }

    // Sort triangle indices to optimize vertex cache
    std::qsort(ozz::array_begin(_output_mesh->triangle_indices),
               _output_mesh->triangle_indices.size() / 3, sizeof(uint16_t) * 3,
               &SortTriangles);

    return true;
}

bool BuildSkin(const tinygltf::Model& model,
               const tinygltf::Primitive& primitive,
               const tinygltf::Skin& skin,
               const ControlPointsRemap& _remap,
               const ozz::animation::Skeleton& _skeleton,
               ozz::sample::Mesh* _output_mesh) {

    assert(_output_mesh->parts.size() == 1 &&
           _output_mesh->parts[0].vertex_count() != 0);

    ozz::sample::Mesh::Part& part = _output_mesh->parts[0];

    // Build joints names map
    typedef ozz::cstring_map<uint16_t> JointsMap;
    JointsMap joints_map;
    for (int i = 0; i < _skeleton.num_joints(); ++i) {
        joints_map[_skeleton.joint_names()[i]] = static_cast<uint16_t>(i);
    }

    // Get joint indices and weights from GLTF
    auto joints_it = primitive.attributes.find("JOINTS_0");
    auto weights_it = primitive.attributes.find("WEIGHTS_0");

    if (joints_it == primitive.attributes.end() ||
        weights_it == primitive.attributes.end()) {
        ozz::log::Err() << "No skinning data found." << std::endl;
        return false;
    }

    // Read joint indices and weights with proper buffer handling
    const tinygltf::Accessor& joint_accessor = model.accessors[joints_it->second];
    const tinygltf::Accessor& weight_accessor = model.accessors[weights_it->second];

    const unsigned char* joint_data;
    const unsigned char* weight_data;
    size_t joint_stride, weight_stride;

    if (!GetBufferData(model, joint_accessor, &joint_data, &joint_stride)) {
        ozz::log::Err() << "Failed to get joint data from buffer." << std::endl;
        return false;
    }

    if (!GetBufferData(model, weight_accessor, &weight_data, &weight_stride)) {
        ozz::log::Err() << "Failed to get weight data from buffer." << std::endl;
        return false;
    }

    // Get vertex count and setup skin mapping
    const size_t vertex_count = part.vertex_count();

    // Define skin mapping structure (reuse from FBX version)
    struct SkinMapping {
        uint16_t index;
        float weight;
    };
    typedef ozz::vector<ozz::vector<SkinMapping>> VertexSkinMappings;
    VertexSkinMappings vertex_skin_mappings(vertex_count);

    // Process each vertex
    for (size_t v = 0; v < vertex_count; ++v) {
        // Read joint indices (typically 4 per vertex)
        const uint16_t* joint_indices = nullptr;
        const uint8_t* joint_indices_byte = nullptr;

        if (joint_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
            joint_indices = reinterpret_cast<const uint16_t*>(joint_data + v * joint_stride);
        } else if (joint_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
            joint_indices_byte = reinterpret_cast<const uint8_t*>(joint_data + v * joint_stride);
        }

        // Read weights
        const float* weights = reinterpret_cast<const float*>(weight_data + v * weight_stride);

        // Store influences for this vertex
        for (int i = 0; i < 4; ++i) {  // Typically 4 influences per vertex
            uint16_t joint_index;
            if (joint_indices) {
                joint_index = joint_indices[i];
            } else if (joint_indices_byte) {
                joint_index = static_cast<uint16_t>(joint_indices_byte[i]);
            } else {
                joint_index = 0;
            }

            float weight = weights[i];

            if (weight > 0.0f && joint_index < skin.joints.size()) {
                // Map GLTF joint index to skeleton joint index
                int skeleton_joint_index = skin.joints[joint_index];
                if (skeleton_joint_index < model.nodes.size()) {
                    const std::string& joint_name = model.nodes[skeleton_joint_index].name;

                    JointsMap::const_iterator it = joints_map.find(joint_name.c_str());
                    if (it != joints_map.end()) {
                        SkinMapping mapping = {it->second, weight};
                        vertex_skin_mappings[v].push_back(mapping);
                    }
                }
            }
        }
    }

    // Get inverse bind matrices
    _output_mesh->inverse_bind_poses.resize(_skeleton.num_joints());
    for (int i = 0; i < _skeleton.num_joints(); ++i) {
        _output_mesh->inverse_bind_poses[i] = ozz::math::Float4x4::identity();
    }

    if (skin.inverseBindMatrices >= 0) {
        const tinygltf::Accessor& accessor = model.accessors[skin.inverseBindMatrices];

        const unsigned char* buffer_data;
        size_t stride;
        if (!GetBufferData(model, accessor, &buffer_data, &stride)) {
            ozz::log::Err() << "Failed to get inverse bind matrices data." << std::endl;
            return false;
        }

        for (size_t i = 0; i < skin.joints.size() && i < accessor.count; ++i) {
            const float* matrix_data = reinterpret_cast<const float*>(
                    buffer_data + i * 16 * sizeof(float));

            // Map GLTF joint to skeleton joint
            int skeleton_joint_index = skin.joints[i];
            if (skeleton_joint_index < model.nodes.size()) {
                const std::string& joint_name = model.nodes[skeleton_joint_index].name;

                JointsMap::const_iterator it = joints_map.find(joint_name.c_str());
                if (it != joints_map.end()) {
                    uint16_t ozz_joint_index = it->second;

                    // Convert from column-major (GLTF) to ozz format
                    ozz::math::Float4x4& bind_matrix = _output_mesh->inverse_bind_poses[ozz_joint_index];
                    bind_matrix.cols[0] = ozz::math::simd_float4::Load(matrix_data[0], matrix_data[1], matrix_data[2], matrix_data[3]);
                    bind_matrix.cols[1] = ozz::math::simd_float4::Load(matrix_data[4], matrix_data[5], matrix_data[6], matrix_data[7]);
                    bind_matrix.cols[2] = ozz::math::simd_float4::Load(matrix_data[8], matrix_data[9], matrix_data[10], matrix_data[11]);
                    bind_matrix.cols[3] = ozz::math::simd_float4::Load(matrix_data[12], matrix_data[13], matrix_data[14], matrix_data[15]);
                }
            }
        }
    }

    // Sort joint indexes according to weights and find max influences
    size_t max_influences = 0;
    for (size_t i = 0; i < vertex_count; ++i) {
        auto& influences = vertex_skin_mappings[i];

        // Update max_influences
        max_influences = ozz::math::Max(max_influences, influences.size());

        // Normalize weights
        float sum = 0.0f;
        for (auto& influence : influences) {
            sum += influence.weight;
        }

        if (sum > 0.0f) {
            const float inv_sum = 1.0f / sum;
            for (auto& influence : influences) {
                influence.weight *= inv_sum;
            }
        }

        // Sort weights (highest first)
        std::sort(influences.begin(), influences.end(),
                  [](const SkinMapping& a, const SkinMapping& b) {
                      return a.weight > b.weight;
                  });
    }

    // Allocate joint indices and weights
    part.joint_indices.resize(vertex_count * max_influences);
    part.joint_weights.resize(vertex_count * max_influences);

    // Build output vertex data
    for (size_t i = 0; i < vertex_count; ++i) {
        const auto& influences = vertex_skin_mappings[i];
        uint16_t* indices = &part.joint_indices[i * max_influences];
        float* weights = &part.joint_weights[i * max_influences];

        size_t influence_count = influences.size();

        // Handle vertices with no influences
        if (influence_count == 0) {
            indices[0] = 0;  // Assign to root joint
            weights[0] = 1.0f;
            influence_count = 1;
        } else {
            // Store influences
            for (size_t j = 0; j < influence_count; ++j) {
                indices[j] = influences[j].index;
                weights[j] = influences[j].weight;
            }
        }

        // Fill unused slots
        for (size_t j = influence_count; j < max_influences; ++j) {
            indices[j] = 0;
            weights[j] = 0.0f;
        }
    }

    return true;
}

// Limits the number of joints influencing a vertex.
bool LimitInfluences(ozz::sample::Mesh& _skinned_mesh, int _limit) {
    assert(_skinned_mesh.parts.size() == 1);

    ozz::sample::Mesh::Part& in_part = _skinned_mesh.parts.front();

    // Check if it's actually needed to limit the number of influences.
    const int max_influences = in_part.influences_count();
    assert(max_influences > 0);
    if (max_influences <= _limit) {
        return true;
    }

    // Iterate all vertices to remove unwanted weights and renormalizes.
    // Note that weights are already sorted, so the last ones are the less
    // influencing.
    const size_t vertex_count = in_part.vertex_count();
    for (size_t i = 0, offset = 0; i < vertex_count; ++i, offset += _limit) {
        // Remove exceeding influences
        for (int j = 0; j < _limit; ++j) {
            in_part.joint_indices[offset + j] =
                    in_part.joint_indices[i * max_influences + j];
            in_part.joint_weights[offset + j] =
                    in_part.joint_weights[i * max_influences + j];
        }
        // Renormalizes weights.
        float sum = 0.f;
        for (int j = 0; j < _limit; ++j) {
            sum += in_part.joint_weights[offset + j];
        }
        for (int j = 0; j < _limit; ++j) {
            in_part.joint_weights[offset + j] *= 1.f / sum;
        }
    }

    // Resizes data
    in_part.joint_indices.resize(vertex_count * _limit);
    in_part.joint_weights.resize(vertex_count * _limit);
    return true;
}

// Finds used joints and remaps joint indices to the minimal range.
bool RemapIndices(ozz::sample::Mesh* _skinned_mesh) {
    assert(_skinned_mesh->parts.size() == 1);

    ozz::sample::Mesh::Part& in_part = _skinned_mesh->parts.front();
    assert(in_part.influences_count() > 0);

    // Collects all unique indices.
    ozz::sample::Mesh::Part::JointIndices local_indices = in_part.joint_indices;
    std::sort(local_indices.begin(), local_indices.end());
    local_indices.erase(std::unique(local_indices.begin(), local_indices.end()),
                        local_indices.end());

    // Build mapping table of mesh original joints to the new ones. Unused joints
    // are set to 0.
    ozz::sample::Mesh::Part::JointIndices original_remap(
            _skinned_mesh->num_joints(), 0);
    for (size_t i = 0; i < local_indices.size(); ++i) {
        original_remap[local_indices[i]] =
                static_cast<ozz::sample::Mesh::Part::JointIndices::value_type>(i);
    }

    // Reset all joints in the mesh.
    for (size_t i = 0; i < in_part.joint_indices.size(); ++i) {
        in_part.joint_indices[i] = original_remap[in_part.joint_indices[i]];
    }

    // Builds joint mapping for the mesh.
    _skinned_mesh->joint_remaps = local_indices;

    // Remaps bind poses and removes unused joints.
    for (size_t i = 0; i < local_indices.size(); ++i) {
        _skinned_mesh->inverse_bind_poses[i] =
                _skinned_mesh->inverse_bind_poses[local_indices[i]];
    }
    _skinned_mesh->inverse_bind_poses.resize(local_indices.size());

    return true;
}

// Split the skinned mesh into parts.
bool SplitParts(const ozz::sample::Mesh& _skinned_mesh,
                ozz::sample::Mesh* _partitionned_mesh) {
    assert(_skinned_mesh.parts.size() == 1);
    assert(_partitionned_mesh->parts.size() == 0);

    const ozz::sample::Mesh::Part& in_part = _skinned_mesh.parts.front();
    const size_t vertex_count = in_part.vertex_count();

    // Creates one mesh part per influence.
    const int max_influences = in_part.influences_count();
    assert(max_influences > 0);

    // Bucket-sort vertices per influence count.
    typedef ozz::vector<ozz::vector<size_t>> BuckedVertices;
    BuckedVertices bucked_vertices;
    bucked_vertices.resize(max_influences);
    if (max_influences > 1) {
        for (size_t i = 0; i < vertex_count; ++i) {
            const float* weights = &in_part.joint_weights[i * max_influences];
            int j = 0;
            for (; j < max_influences && weights[j] > 0.f; ++j) {
            }
            const int influences = j - 1;
            bucked_vertices[influences].push_back(i);
        }
    } else {
        for (size_t i = 0; i < vertex_count; ++i) {
            bucked_vertices[0].push_back(i);
        }
    }

    // Group vertices if there's not enough of them for a given part.
    const size_t kMinBucketSize = 32;

    for (size_t i = 0; i < bucked_vertices.size() - 1; ++i) {
        BuckedVertices::reference bucket = bucked_vertices[i];
        if (bucket.size() < kMinBucketSize) {
            // Transfers vertices to next bucket if there aren't enough.
            BuckedVertices::reference next_bucket = bucked_vertices[i + 1];
            next_bucket.reserve(next_bucket.size() + bucket.size());
            for (size_t j = 0; j < bucket.size(); ++j) {
                next_bucket.push_back(bucket[j]);
            }
            bucket.clear();
        }
    }

    // Fills mesh parts.
    _partitionned_mesh->parts.reserve(max_influences);
    for (int i = 0; i < max_influences; ++i) {
        const ozz::vector<size_t>& bucket = bucked_vertices[i];
        const size_t bucket_vertex_count = bucket.size();
        if (bucket_vertex_count == 0) {
            continue;
        }

        // Adds a new part.
        _partitionned_mesh->parts.resize(_partitionned_mesh->parts.size() + 1);
        ozz::sample::Mesh::Part& out_part = _partitionned_mesh->parts.back();

        // Resize output part.
        const int influences = i + 1;
        out_part.positions.resize(bucket_vertex_count *
                                  ozz::sample::Mesh::Part::kPositionsCpnts);
        out_part.normals.resize(bucket_vertex_count *
                                ozz::sample::Mesh::Part::kNormalsCpnts);
        if (in_part.uvs.size()) {
            out_part.uvs.resize(bucket_vertex_count *
                                ozz::sample::Mesh::Part::kUVsCpnts);
        }
        if (in_part.colors.size()) {
            out_part.colors.resize(bucket_vertex_count *
                                   ozz::sample::Mesh::Part::kColorsCpnts);
        }
        if (in_part.tangents.size()) {
            out_part.tangents.resize(bucket_vertex_count *
                                     ozz::sample::Mesh::Part::kTangentsCpnts);
        }
        out_part.joint_indices.resize(bucket_vertex_count * influences);
        out_part.joint_weights.resize(bucket_vertex_count * influences);

        // Fills output of this part.
        for (size_t j = 0; j < bucket_vertex_count; ++j) {
            const size_t bucket_vertex_index = bucket[j];

            // Copy vertex data (positions, normals, uvs, colors, tangents)
            // ... (implementation similar to FBX version)
            // Copy joint data
            const uint16_t* in_indices =
                    &in_part.joint_indices[bucket_vertex_index * max_influences];
            uint16_t* out_indices = &out_part.joint_indices[j * influences];
            for (int k = 0; k < influences; ++k) {
                out_indices[k] = in_indices[k];
            }

            if (influences > 1) {
                const float* in_weights =
                        &in_part.joint_weights[bucket_vertex_index * max_influences];
                float* out_weights = &out_part.joint_weights[j * influences];
                for (int k = 0; k < influences; ++k) {
                    out_weights[k] = in_weights[k];
                }
            }
        }
    }

    // Copy bind pose matrices and joint remaps
    _partitionned_mesh->inverse_bind_poses = _skinned_mesh.inverse_bind_poses;
    _partitionned_mesh->joint_remaps = _skinned_mesh.joint_remaps;

    return true;
}

// Removes the less significant weight.
bool StripWeights(ozz::sample::Mesh* _mesh) {
    for (size_t i = 0; i < _mesh->parts.size(); ++i) {
        ozz::sample::Mesh::Part& part = _mesh->parts[i];
        const int influence_count = part.influences_count();
        const int vertex_count = part.vertex_count();
        if (influence_count <= 1) {
            part.joint_weights.clear();
        } else {
            const ozz::vector<float> copy = part.joint_weights;
            part.joint_weights.clear();
            part.joint_weights.reserve(vertex_count * (influence_count - 1));

            for (int j = 0; j < vertex_count; ++j) {
                for (int k = 0; k < influence_count - 1; ++k) {
                    part.joint_weights.push_back(copy[j * influence_count + k]);
                }
            }
        }
        assert(static_cast<int>(part.joint_weights.size()) ==
               vertex_count * (influence_count - 1));
    }

    return true;
}

int main1(int _argc, const char** _argv) {
    // Parse arguments
    ozz::options::ParseResult parse_result = ozz::options::ParseCommandLine(
            _argc, _argv, "1.0",
            "Imports a skin from a GLTF2 file and converts it to ozz binary format");
    if (parse_result != ozz::options::kSuccess) {
        return parse_result == ozz::options::kExitSuccess ? EXIT_SUCCESS
                                                          : EXIT_FAILURE;
    }

    // Open skeleton file
    ozz::animation::Skeleton skeleton;
    {
        ozz::log::Out() << "Loading skeleton archive " << OPTIONS_gltf_skeleton
                        << "." << std::endl;
        ozz::io::File file(OPTIONS_gltf_skeleton, "rb");
        if (!file.opened()) {
            ozz::log::Err() << "Failed to open skeleton file "
                            << OPTIONS_gltf_skeleton << "." << std::endl;
            return EXIT_FAILURE;
        }
        ozz::io::IArchive archive(&file);
        if (!archive.TestTag<ozz::animation::Skeleton>()) {
            ozz::log::Err() << "Failed to load skeleton instance from file "
                            << OPTIONS_gltf_skeleton << "." << std::endl;
            return EXIT_FAILURE;
        }
        archive >> skeleton;
    }

    // Load GLTF2 file with proper error handling for external files
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    // Check if file exists first
    std::ifstream file_check(OPTIONS_gltf_file);
    if (!file_check.good()) {
        ozz::log::Err() << "Cannot open GLTF file: " << OPTIONS_gltf_file << std::endl;
        return EXIT_FAILURE;
    }
    file_check.close();

    bool ret = false;
    const char* filename = OPTIONS_gltf_file;
    std::string filename_str(filename);

    // Extract directory path for resolving external .bin files
    std::string base_dir = "";
    size_t last_slash = filename_str.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        base_dir = filename_str.substr(0, last_slash + 1);
    }

    if (filename_str.find(".glb") != std::string::npos) {
        // GLB files are self-contained binary format
        ret = loader.LoadBinaryFromFile(&model, &err, &warn, filename_str);
    } else {
        // GLTF files may reference external .bin files
        ret = loader.LoadASCIIFromFile(&model, &err, &warn, filename_str);

        // Verify that external buffers were loaded successfully
        for (size_t i = 0; i < model.buffers.size(); ++i) {
            const tinygltf::Buffer& buffer = model.buffers[i];
            if (buffer.data.empty() && !buffer.uri.empty()) {
                // Check if it's a data URI
                if (buffer.uri.find("data:") != 0) {
                    // It's an external file, check if it exists
                    std::string bin_path = base_dir + buffer.uri;
                    std::ifstream bin_check(bin_path);
                    if (!bin_check.good()) {
                        ozz::log::Err() << "Cannot find external buffer file: " << bin_path << std::endl;
                        ozz::log::Err() << "Make sure the .bin file is in the same directory as the .gltf file." << std::endl;
                        return EXIT_FAILURE;
                    }
                    bin_check.close();
                }
            }
        }
    }

    if (!warn.empty()) {
        ozz::log::LogV() << "GLTF Warning: " << warn << std::endl;
    }

    if (!err.empty()) {
        ozz::log::Err() << "GLTF Error: " << err << std::endl;
    }

    if (!ret) {
        ozz::log::Err() << "Failed to parse GLTF file " << OPTIONS_gltf_file
                        << std::endl;
        return EXIT_FAILURE;
    }

    // Process all meshes
    ozz::vector<ozz::sample::Mesh> meshes;

    for (size_t mesh_idx = 0; mesh_idx < model.meshes.size(); ++mesh_idx) {
        const tinygltf::Mesh& gltf_mesh = model.meshes[mesh_idx];

        for (size_t prim_idx = 0; prim_idx < gltf_mesh.primitives.size(); ++prim_idx) {
            const tinygltf::Primitive& primitive = gltf_mesh.primitives[prim_idx];

            // Only process triangles
            if (primitive.mode != TINYGLTF_MODE_TRIANGLES) {
                ozz::log::LogV() << "Skipping non-triangle primitive." << std::endl;
                continue;
            }

            ozz::sample::Mesh output_mesh;
            output_mesh.parts.resize(1);

            ControlPointsRemap remap;
            if (!BuildVertices(model, primitive, &remap, &output_mesh)) {
                ozz::log::Err() << "Failed to read vertices." << std::endl;
                return EXIT_FAILURE;
            }

            // Check for skinning data
            auto joints_it = primitive.attributes.find("JOINTS_0");
            if (joints_it != primitive.attributes.end() && !model.skins.empty()) {
                // Find the appropriate skin (simplified - assumes first skin)
                const tinygltf::Skin& skin = model.skins[0];

                if (!BuildSkin(model, primitive, skin, remap, skeleton, &output_mesh)) {
                    ozz::log::Err() << "Failed to read skinning data." << std::endl;
                    return EXIT_FAILURE;
                }

                // Apply post-processing steps (same as FBX version)
                if (OPTIONS_gltf_max_influences > 0) {
                    if (!LimitInfluences(output_mesh, OPTIONS_gltf_max_influences)) {
                        ozz::log::Err() << "Failed to limit number of joint influences."
                                        << std::endl;
                        return EXIT_FAILURE;
                    }
                }

                if (!RemapIndices(&output_mesh)) {
                    ozz::log::Err() << "Failed to remap joint indices." << std::endl;
                    return EXIT_FAILURE;
                }

                if (OPTIONS_gltf_split) {
                    ozz::sample::Mesh partitioned_mesh;
                    if (!SplitParts(output_mesh, &partitioned_mesh)) {
                        ozz::log::Err() << "Failed to partition meshes." << std::endl;
                        return EXIT_FAILURE;
                    }
                    output_mesh = partitioned_mesh;
                }

                if (!StripWeights(&output_mesh)) {
                    ozz::log::Err() << "Failed to strip weights." << std::endl;
                    return EXIT_FAILURE;
                }
            }

            meshes.push_back(output_mesh);
        }
    }

    if (meshes.empty()) {
        ozz::log::Err() << "No meshes processed." << std::endl;
        return EXIT_FAILURE;
    }

    // Write output file
    ozz::io::File mesh_file(OPTIONS_gltf_mesh, "wb");
    if (!mesh_file.opened()) {
        ozz::log::Err() << "Failed to open output file: " << OPTIONS_gltf_mesh
                        << std::endl;
        return EXIT_FAILURE;
    }

    {
        ozz::io::OArchive archive(&mesh_file);
        for (size_t m = 0; m < meshes.size(); ++m) {
            archive << meshes[m];
        }
    }

    ozz::log::Log() << "Mesh binary archive successfully outputted for file "
                    << OPTIONS_gltf_file << "." << std::endl;

    return EXIT_SUCCESS;
}