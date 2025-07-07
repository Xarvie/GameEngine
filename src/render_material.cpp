/*
 * 文件: render_material.cpp
 * 描述: RenderMaterial 和 MaterialSet 的序列化实现。
 * 此文件保持不变。
 */
#include "render_material.h"

#include "ozz/base/containers/string_archive.h"
#include "ozz/base/containers/vector_archive.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/maths/math_archive.h"

namespace ozz {
    namespace io {

// RenderMaterial 的序列化实现
        void Extern<sample::RenderMaterial>::Save(OArchive& _archive,
                                                  const sample::RenderMaterial* _materials,
                                                  size_t _count) {
            for (size_t i = 0; i < _count; ++i) {
                const auto& mat = _materials[i];
                _archive << mat.name;
                _archive << mat.base_color_factor;
                _archive << mat.metallic_factor;
                _archive << mat.roughness_factor;
                _archive << mat.base_color_texture_path;
                _archive << mat.metallic_roughness_texture_path;
                _archive << mat.normal_texture_path;
                _archive << mat.occlusion_texture_path;
                _archive << mat.emissive_texture_path;
            }
        }

        void Extern<sample::RenderMaterial>::Load(IArchive& _archive,
                                                  sample::RenderMaterial* _materials,
                                                  size_t _count, uint32_t _version) {
            (void)_version;
            for (size_t i = 0; i < _count; ++i) {
                auto& mat = _materials[i];
                _archive >> mat.name;
                _archive >> mat.base_color_factor;
                _archive >> mat.metallic_factor;
                _archive >> mat.roughness_factor;
                _archive >> mat.base_color_texture_path;
                _archive >> mat.metallic_roughness_texture_path;
                _archive >> mat.normal_texture_path;
                _archive >> mat.occlusion_texture_path;
                _archive >> mat.emissive_texture_path;
            }
        }

// MaterialSet 的序列化实现
        void Extern<sample::MaterialSet>::Save(OArchive& _archive,
                                               const sample::MaterialSet* _sets,
                                               size_t _count) {
            for (size_t i = 0; i < _count; ++i) {
                _archive << _sets[i].materials;
            }
        }

        void Extern<sample::MaterialSet>::Load(IArchive& _archive,
                                               sample::MaterialSet* _sets, size_t _count,
                                               uint32_t _version) {
            (void)_version;
            for (size_t i = 0; i < _count; ++i) {
                _archive >> _sets[i].materials;
            }
        }

    }  // namespace io
}  // namespace ozz