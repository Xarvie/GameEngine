/*
 * 文件: mesh.cpp
 * 描述: [最终修正版] MeshPart, MeshAsset, 和新 SceneNode 的序列化实现。
 * - 补全了 MeshAsset 的 Save/Load 实现以修复链接错误。
 */
#include "mesh.h"

#include "ozz/base/containers/string_archive.h"
#include "ozz/base/containers/vector_archive.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/maths/math_archive.h"
#include "ozz/base/maths/simd_math_archive.h"

#include "ozz/base/memory/allocator.h"

namespace ozz {
    namespace io {

// SceneNode 的序列化实现
        void Extern<sample::SceneNode>::Save(OArchive& _archive,
                                             const sample::SceneNode* _nodes,
                                             size_t _count) {
            for (size_t i = 0; i < _count; ++i) {
                const auto& node = _nodes[i];
                _archive << node.name;
                _archive << node.transform;
                _archive << node.parent_index;
            }
        }

        void Extern<sample::SceneNode>::Load(IArchive& _archive,
                                             sample::SceneNode* _nodes, size_t _count,
                                             uint32_t _version) {
            (void)_version;
            for (size_t i = 0; i < _count; ++i) {
                auto& node = _nodes[i];
                _archive >> node.name;
                _archive >> node.transform;
                _archive >> node.parent_index;
            }
        }

// MeshPart 的序列化实现
        void Extern<sample::MeshPart>::Save(OArchive& _archive,
                                            const sample::MeshPart* _parts,
                                            size_t _count) {
            for (size_t i = 0; i < _count; ++i) {
                const sample::MeshPart& part = _parts[i];
                _archive << part.material_index;
                _archive << part.index_offset;
                _archive << part.index_count;
                _archive << part.is_static_body;
                _archive << part.scene_node_index;
            }
        }

        void Extern<sample::MeshPart>::Load(IArchive& _archive,
                                            sample::MeshPart* _parts, size_t _count,
                                            uint32_t _version) {
            (void)_version;
            for (size_t i = 0; i < _count; ++i) {
                sample::MeshPart& part = _parts[i];
                _archive >> part.material_index;
                _archive >> part.index_offset;
                _archive >> part.index_count;
                _archive >> part.is_static_body;
                _archive >> part.scene_node_index;
            }
        }

// [修正] MeshAsset 的序列化实现
        void Extern<sample::MeshAsset>::Save(OArchive& _archive,
                                             const sample::MeshAsset* _assets,
                                             size_t _count) {
            for (size_t i = 0; i < _count; ++i) {
                const sample::MeshAsset& asset = _assets[i];
                _archive << asset.vertex_buffer;
                _archive << asset.vertex_stride;
                _archive << asset.index_buffer;
                _archive << asset.parts;
                _archive << asset.inverse_bind_poses;
                _archive << asset.scene_nodes;
            }
        }

        void Extern<sample::MeshAsset>::Load(IArchive& _archive,
                                             sample::MeshAsset* _assets, size_t _count,
                                             uint32_t _version) {
            (void)_version;
            for (size_t i = 0; i < _count; ++i) {
                sample::MeshAsset& asset = _assets[i];
                _archive >> asset.vertex_buffer;
                _archive >> asset.vertex_stride;
                _archive >> asset.index_buffer;
                _archive >> asset.parts;
                _archive >> asset.inverse_bind_poses;
                _archive >> asset.scene_nodes;
            }
        }

    }  // namespace io
}  // namespace ozz
