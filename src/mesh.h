/*
 * 文件: mesh.h
 * 描述: [高性能重构版] 核心运行时几何资产的数据结构。
 * - 新增 SceneNode 用于描述静态层级。
 * - MeshPart 职责细分，区分蒙皮与静态。
 */
#ifndef OZZ_SAMPLES_FRAMEWORK_MESH_H_
#define OZZ_SAMPLES_FRAMEWORK_MESH_H_

#include "ozz/base/containers/string.h"
#include "ozz/base/containers/vector.h"
#include "ozz/base/io/archive_traits.h"
#include "ozz/base/maths/simd_math.h"
#include "ozz/base/maths/vec_float.h"
#include "ozz/base/platform.h"
#include "ozz/base/maths/transform.h"

namespace ozz {
    namespace sample {

// [新增] 用于描述静态场景图节点的结构
// 这是一个轻量级结构，用于高效地表示场景层级。
        struct SceneNode {
            ozz::string name;
            ozz::math::Transform transform;  // 节点的局部变换
            int32_t parent_index;            // 指向父节点的索引, -1 表示根节点
        };

// 轻量级的"渲染指令"，定义了一个使用单一材质的渲染批次。
        struct MeshPart {
            uint32_t material_index;  // 指向 MaterialSet 中的材质索引
            uint32_t index_offset;    // 在 MeshAsset 的全局 index_buffer 中的起始偏移
            uint32_t index_count;     // 这个批次使用的索引数量

            // 标志此部分是否为静态网格。
            // true:  是静态(刚体)部分, 受场景图节点驱动。
            // false: 是蒙皮部分, 受骨架驱动。
            bool is_static_body;

            // [新增] 仅当 is_static_body 为 true 时有效。
            // 指向其所属的 SceneNode 在 MeshAsset::scene_nodes 中的索引。
            int32_t scene_node_index = -1;
        };

// 最终导出的 *.mesh.ozz 文件内容。
// 这是为高性能渲染管线设计的核心几何体资产结构。
        struct MeshAsset {
            // 几何数据
            ozz::vector<uint8_t> vertex_buffer;
            size_t vertex_stride = 0;
            ozz::vector<uint32_t> index_buffer;

            // 渲染部件列表
            ozz::vector<MeshPart> parts;

            // 逆绑定姿势矩阵。
            // 它的尺寸将等于蒙皮骨架中的骨骼总数。
            ozz::vector<ozz::math::Float4x4> inverse_bind_poses;

            // [新增] 存储所有节点的静态场景图层级结构
            ozz::vector<SceneNode> scene_nodes;
        };

    }  // namespace sample

// Ozz 序列化支持的接口声明
    namespace io {

        OZZ_IO_TYPE_TAG("ozz-sample-SceneNode", sample::SceneNode)
        OZZ_IO_TYPE_VERSION(1, sample::SceneNode)
        template <>
        struct Extern<sample::SceneNode> {
            static void Save(OArchive& _archive, const sample::SceneNode* _nodes,
                             size_t _count);
            static void Load(IArchive& _archive, sample::SceneNode* _nodes, size_t _count,
                             uint32_t _version);
        };

        OZZ_IO_TYPE_TAG("ozz-sample-Mesh-Part", sample::MeshPart)
        OZZ_IO_TYPE_VERSION(2, sample::MeshPart) // 版本号提升
        template <>
        struct Extern<sample::MeshPart> {
            static void Save(OArchive& _archive, const sample::MeshPart* _parts,
                             size_t _count);
            static void Load(IArchive& _archive, sample::MeshPart* _parts, size_t _count,
                             uint32_t _version);
        };

        OZZ_IO_TYPE_TAG("ozz-sample-MeshAsset", sample::MeshAsset)
        OZZ_IO_TYPE_VERSION(2, sample::MeshAsset) // 版本号提升
        template <>
        struct Extern<sample::MeshAsset> {
            static void Save(OArchive& _archive, const sample::MeshAsset* _assets,
                             size_t _count);
            static void Load(IArchive& _archive, sample::MeshAsset* _assets,
                             size_t _count, uint32_t _version);
        };

    }  // namespace io
}  // namespace ozz
#endif  // OZZ_SAMPLES_FRAMEWORK_MESH_H_
