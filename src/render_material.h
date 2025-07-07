/*
 * 文件: render_material.h
 * 描述: PBR材质和材质集的运行时数据结构。
 * 此文件保持不变，因为它已经能够很好地描述材质属性。
 */
#ifndef OZZ_SAMPLES_FRAMEWORK_RENDER_MATERIAL_H_
#define OZZ_SAMPLES_FRAMEWORK_RENDER_MATERIAL_H_

#include "ozz/base/containers/string.h"
#include "ozz/base/containers/vector.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/maths/vec_float.h"

namespace ozz {
    namespace sample {

// 定义PBR（基于物理的渲染）材质的核心属性。
        struct RenderMaterial {
            // 材质名称，主要用于调试
            ozz::string name;

            // PBR 金属-粗糙度 工作流参数
            ozz::math::Float4 base_color_factor = {1.f, 1.f, 1.f, 1.f};
            float metallic_factor = 1.f;
            float roughness_factor = 1.f;

            // 纹理路径（相对于资产根目录）
            ozz::string base_color_texture_path;
            ozz::string metallic_roughness_texture_path;
            ozz::string normal_texture_path;
            ozz::string occlusion_texture_path;
            ozz::string emissive_texture_path;
        };

// 一个材质集合，代表一个资产所需的所有材质。
        struct MaterialSet {
            ozz::vector<RenderMaterial> materials;
        };

    }  // namespace sample

// Ozz 序列化支持的接口声明
    namespace io {
        OZZ_IO_TYPE_TAG("ozz-sample-RenderMaterial", sample::RenderMaterial)
        OZZ_IO_TYPE_VERSION(1, sample::RenderMaterial)
        template <>
        struct Extern<sample::RenderMaterial> {
            static void Save(OArchive& _archive, const sample::RenderMaterial* _materials,
                             size_t _count);
            static void Load(IArchive& _archive, sample::RenderMaterial* _materials,
                             size_t _count, uint32_t _version);
        };

        OZZ_IO_TYPE_TAG("ozz-sample-MaterialSet", sample::MaterialSet)
        OZZ_IO_TYPE_VERSION(1, sample::MaterialSet)
        template <>
        struct Extern<sample::MaterialSet> {
            static void Save(OArchive& _archive, const sample::MaterialSet* _sets,
                             size_t _count);
            static void Load(IArchive& _archive, sample::MaterialSet* _sets,
                             size_t _count, uint32_t _version);
        };
    }  // namespace io
}  // namespace ozz
#endif  // OZZ_SAMPLES_FRAMEWORK_RENDER_MATERIAL_H_