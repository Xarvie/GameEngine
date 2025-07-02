#include "render_material.h"
#include "ozz/base/containers/string_archive.h"
#include "ozz/base/containers/vector_archive.h"
#include "ozz/base/maths/math_archive.h"

namespace ozz {
    namespace io {

// RenderMaterial::Texture 序列化实现
        void Extern<sample::RenderMaterial::Texture>::Save(OArchive& _archive, const sample::RenderMaterial::Texture* _textures, size_t _count) {
            for (size_t i = 0; i < _count; ++i) {
                const auto& tex = _textures[i];
                _archive << tex.path;
                _archive << tex.uv_set_index;
            }
        }

        void Extern<sample::RenderMaterial::Texture>::Load(IArchive& _archive, sample::RenderMaterial::Texture* _textures, size_t _count, uint32_t _version) {
            (void)_version;
            for (size_t i = 0; i < _count; ++i) {
                auto& tex = _textures[i];
                _archive >> tex.path;
                _archive >> tex.uv_set_index;
            }
        }

// RenderMaterial 序列化实现
        void Extern<sample::RenderMaterial>::Save(OArchive& _archive, const sample::RenderMaterial* _materials, size_t _count) {
            for (size_t i = 0; i < _count; ++i) {
                const auto& mat = _materials[i];
                _archive << mat.name;
                _archive << mat.base_color_factor;
                _archive << mat.metallic_factor;
                _archive << mat.roughness_factor;
                _archive << mat.emissive_factor;

                // 将 enum class 转换为整数进行序列化
                uint32_t alpha_mode = static_cast<uint32_t>(mat.alpha_mode);
                _archive << alpha_mode;
                _archive << mat.alpha_cutoff;
                _archive << mat.double_sided;

                _archive << mat.base_color_texture;
                _archive << mat.metallic_roughness_texture;
                _archive << mat.normal_texture;
                _archive << mat.occlusion_texture;
                _archive << mat.emissive_texture;
            }
        }

        void Extern<sample::RenderMaterial>::Load(IArchive& _archive, sample::RenderMaterial* _materials, size_t _count, uint32_t _version) {
            (void)_version;
            for (size_t i = 0; i < _count; ++i) {
                auto& mat = _materials[i];
                _archive >> mat.name;
                _archive >> mat.base_color_factor;
                _archive >> mat.metallic_factor;
                _archive >> mat.roughness_factor;
                _archive >> mat.emissive_factor;

                // 从整数反序列化为 enum class
                uint32_t alpha_mode;
                _archive >> alpha_mode;
                mat.alpha_mode = static_cast<sample::RenderMaterial::AlphaMode>(alpha_mode);
                _archive >> mat.alpha_cutoff;
                _archive >> mat.double_sided;

                _archive >> mat.base_color_texture;
                _archive >> mat.metallic_roughness_texture;
                _archive >> mat.normal_texture;
                _archive >> mat.occlusion_texture;
                _archive >> mat.emissive_texture;
            }
        }

// MaterialSet 序列化实现
        void Extern<sample::MaterialSet>::Save(OArchive& _archive, const sample::MaterialSet* _sets, size_t _count) {
            for (size_t i = 0; i < _count; ++i) {
                _archive << _sets[i].materials;
            }
        }

        void Extern<sample::MaterialSet>::Load(IArchive& _archive, sample::MaterialSet* _sets, size_t _count, uint32_t _version) {
            (void)_version;
            for (size_t i = 0; i < _count; ++i) {
                _archive >> _sets[i].materials;
            }
        }

    } // namespace io
} // namespace ozz
