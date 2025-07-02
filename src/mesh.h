#ifndef OZZ_SAMPLES_FRAMEWORK_MESH_H_
#define OZZ_SAMPLES_FRAMEWORK_MESH_H_

#include "ozz/base/containers/vector.h"
#include "ozz/base/io/archive_traits.h"
#include "ozz/base/maths/simd_math.h"
#include "ozz/base/maths/vec_float.h"
#include "ozz/base/platform.h"

namespace ozz {
    namespace sample {

        struct Mesh {
            struct Part {
                int vertex_count() const { return static_cast<int>(positions.size()) / 3; }
                int influences_count() const {
                    const int _vertex_count = vertex_count();
                    if (_vertex_count == 0) {
                        return 0;
                    }
                    return static_cast<int>(joint_indices.size()) / _vertex_count;
                }

                typedef ozz::vector<float> Positions;
                Positions positions;
                enum { kPositionsCpnts = 3 };

                typedef ozz::vector<float> Normals;
                Normals normals;
                enum { kNormalsCpnts = 3 };

                typedef ozz::vector<float> Tangents;
                Tangents tangents;
                enum { kTangentsCpnts = 4 };

                typedef ozz::vector<uint8_t> Colors;
                Colors colors;
                enum { kColorsCpnts = 4 };

                typedef ozz::vector<float> UVs;
                UVs uvs0;
                UVs uvs1;
                enum { kUVsCpnts = 2 };

                typedef ozz::vector<uint16_t> MaterialIndices;
                MaterialIndices material_indices;

                typedef ozz::vector<uint16_t> JointIndices;
                JointIndices joint_indices;

                typedef ozz::vector<float> JointWeights;
                JointWeights joint_weights;
            };

            typedef ozz::vector<Part> Parts;
            Parts parts;

            typedef ozz::vector<uint16_t> TriangleIndices;
            TriangleIndices triangle_indices;

            typedef ozz::vector<uint16_t> JointRemaps;
            JointRemaps joint_remaps;

            typedef ozz::vector<ozz::math::Float4x4> InversBindPoses;
            InversBindPoses inverse_bind_poses;

            // ===================================================================
            // **新增：为兼容旧代码而重新加入的辅助函数**
            // 这些函数帮助我们暂时不必修改 renderer_impl.cc 等文件
            // ===================================================================

            // 计算所有Part的总顶点数
            int vertex_count() const {
                int count = 0;
                for (const auto& part : parts) {
                    count += part.vertex_count();
                }
                return count;
            }

            // 获取总的三角形索引数
            int triangle_index_count() const {
                return static_cast<int>(triangle_indices.size());
            }

            // 判断此Mesh是否为蒙皮网格
            bool skinned() const {
                return !inverse_bind_poses.empty();
            }

            // 获取蒙皮所需的关节数量
            int num_joints() const { return static_cast<int>(inverse_bind_poses.size()); }

            // 获取骨骼重映射表中最大的关节索引号
            int highest_joint_index() const {
                return joint_remaps.empty() ? 0 : static_cast<int>(joint_remaps.back());
            }
        };
    }  // namespace sample

    namespace io {
// 将Part的版本号提升到2，以区分新旧格式
        OZZ_IO_TYPE_TAG("ozz-sample-Mesh-Part", sample::Mesh::Part)
        OZZ_IO_TYPE_VERSION(2, sample::Mesh::Part)

        template <>
        struct Extern<sample::Mesh::Part> {
            static void Save(OArchive& _archive, const sample::Mesh::Part* _parts,
                             size_t _count);
            static void Load(IArchive& _archive, sample::Mesh::Part* _parts, size_t _count,
                             uint32_t _version);
        };

        OZZ_IO_TYPE_TAG("ozz-sample-Mesh", sample::Mesh)
        OZZ_IO_TYPE_VERSION(1, sample::Mesh)

        template <>
        struct Extern<sample::Mesh> {
            static void Save(OArchive& _archive, const sample::Mesh* _meshes,
                             size_t _count);
            static void Load(IArchive& _archive, sample::Mesh* _meshes, size_t _count,
                             uint32_t _version);
        };
    }  // namespace io
}  // namespace ozz
#endif  // OZZ_SAMPLES_FRAMEWORK_MESH_H_
