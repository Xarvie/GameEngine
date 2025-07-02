#include "mesh.h"

#include "ozz/base/containers/vector_archive.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/maths/math_archive.h"
#include "ozz/base/maths/simd_math_archive.h"
#include "ozz/base/memory/allocator.h"

namespace ozz {
    namespace io {

        void Extern<sample::Mesh::Part>::Save(OArchive& _archive,
                                              const sample::Mesh::Part* _parts,
                                              size_t _count) {
            for (size_t i = 0; i < _count; ++i) {
                const sample::Mesh::Part& part = _parts[i];
                _archive << part.positions;
                _archive << part.normals;
                _archive << part.tangents;
                _archive << part.uvs0;
                _archive << part.uvs1;
                _archive << part.colors;
                _archive << part.joint_indices;
                _archive << part.joint_weights;
                _archive << part.material_indices;
            }
        }

        void Extern<sample::Mesh::Part>::Load(IArchive& _archive,
                                              sample::Mesh::Part* _parts, size_t _count,
                                              uint32_t _version) {
            for (size_t i = 0; i < _count; ++i) {
                sample::Mesh::Part& part = _parts[i];
                _archive >> part.positions;
                _archive >> part.normals;
                _archive >> part.tangents;

                if (_version >= 2) {
                    // 新版本，包含 uvs0, uvs1 和 material_indices
                    _archive >> part.uvs0;
                    _archive >> part.uvs1;
                } else {
                    // 兼容旧版本，它只有一个名为 "uvs" 的 vector
                    // 我们需要一个临时变量来读取它，然后赋值给 uvs0
                    ozz::vector<float> old_uvs;
                    _archive >> old_uvs;
                    part.uvs0 = old_uvs;
                    part.uvs1.clear(); // 确保 uvs1 是空的
                }

                _archive >> part.colors;
                _archive >> part.joint_indices;
                _archive >> part.joint_weights;

                if (_version >= 2) {
                    _archive >> part.material_indices;
                } else {
                    // 旧版本没有材质ID，清空以确保数据一致性
                    part.material_indices.clear();
                }
            }
        }

        void Extern<sample::Mesh>::Save(OArchive& _archive, const sample::Mesh* _meshes,
                                        size_t _count) {
            for (size_t i = 0; i < _count; ++i) {
                const sample::Mesh& mesh = _meshes[i];
                _archive << mesh.parts;
                _archive << mesh.triangle_indices;
                _archive << mesh.joint_remaps;
                _archive << mesh.inverse_bind_poses;
            }
        }

        void Extern<sample::Mesh>::Load(IArchive& _archive, sample::Mesh* _meshes,
                                        size_t _count, uint32_t _version) {
            (void)_version;
            for (size_t i = 0; i < _count; ++i) {
                sample::Mesh& mesh = _meshes[i];
                _archive >> mesh.parts;
                _archive >> mesh.triangle_indices;
                _archive >> mesh.joint_remaps;
                _archive >> mesh.inverse_bind_poses;
            }
        }
    }  // namespace io
}  // namespace ozz
