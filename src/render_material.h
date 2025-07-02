#ifndef OZZ_SAMPLES_FRAMEWORK_RENDER_MATERIAL_H_
#define OZZ_SAMPLES_FRAMEWORK_RENDER_MATERIAL_H_

#include "ozz/base/containers/string.h"
#include "ozz/base/containers/vector.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/maths/vec_float.h"

namespace ozz {
    namespace sample {

// 纯粹的渲染材质结构，支持PBR（基于物理的渲染）工作流
        struct RenderMaterial {
            // 纹理定义仅包含路径和UV集索引
            struct Texture {
                // 约定：路径为相对于预设的“纹理根目录”的相对路径
                ozz::string path;
                // 使用第几套UV坐标 (0 对应 uvs0, 1 对应 uvs1)
                int uv_set_index = 0;
            };

            // 材质名称，主要用于在编辑器和调试工具中进行识别
            ozz::string name;

            // PBR 金属-粗糙度 工作流参数
            ozz::math::Float4 base_color_factor = {1.f, 1.f, 1.f, 1.f}; // 基础颜色因子
            float metallic_factor = 1.f;      // 金属度因子 (0=绝缘体, 1=金属)
            float roughness_factor = 1.f;     // 粗糙度因子 (0=镜面, 1=漫反射)
            ozz::math::Float3 emissive_factor = {0.f, 0.f, 0.f}; // 自发光颜色

            // 透明度与剔除控制
            enum class AlphaMode { OPAQUE, MASK, BLEND };
            AlphaMode alpha_mode = AlphaMode::OPAQUE; // 混合模式
            float alpha_cutoff = 0.5f;               // Alpha测试阈值，仅在 MASK 模式下有效
            bool double_sided = false;               // 是否进行双面渲染

            // 纹理贴图引用
            Texture base_color_texture;
            Texture metallic_roughness_texture; // 金属度(R通道), 粗糙度(G通道)
            Texture normal_texture;
            Texture occlusion_texture;           // 环境光遮蔽
            Texture emissive_texture;
        };

// 材质集：一个*.materials.ozz文件包含一个此对象，代表一个资产所需的所有材质
        struct MaterialSet {
            ozz::vector<RenderMaterial> materials;
        };

    } // namespace sample

// Ozz 序列化支持的接口声明
    namespace io {
        OZZ_IO_TYPE_TAG("ozz-sample-RenderMaterial-Texture", sample::RenderMaterial::Texture)
        OZZ_IO_TYPE_VERSION(1, sample::RenderMaterial::Texture)
        template <>
        struct Extern<sample::RenderMaterial::Texture> {
            static void Save(OArchive& _archive, const sample::RenderMaterial::Texture* _textures, size_t _count);
            static void Load(IArchive& _archive, sample::RenderMaterial::Texture* _textures, size_t _count, uint32_t _version);
        };

        OZZ_IO_TYPE_TAG("ozz-sample-RenderMaterial", sample::RenderMaterial)
        OZZ_IO_TYPE_VERSION(1, sample::RenderMaterial)
        template <>
        struct Extern<sample::RenderMaterial> {
            static void Save(OArchive& _archive, const sample::RenderMaterial* _materials, size_t _count);
            static void Load(IArchive& _archive, sample::RenderMaterial* _materials, size_t _count, uint32_t _version);
        };

        OZZ_IO_TYPE_TAG("ozz-sample-MaterialSet", sample::MaterialSet)
        OZZ_IO_TYPE_VERSION(1, sample::MaterialSet)
        template <>
        struct Extern<sample::MaterialSet> {
            static void Save(OArchive& _archive, const sample::MaterialSet* _sets, size_t _count);
            static void Load(IArchive& _archive, sample::MaterialSet* _sets, size_t _count, uint32_t _version);
        };
    } // namespace io
} // namespace ozz
#endif // OZZ_SAMPLES_FRAMEWORK_RENDER_MATERIAL_H_
