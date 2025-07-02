// 完整替换 src/sample_skinning.cc

#include "application.h"
#include "imgui.h"
#include "renderer.h"
#include "utils.h"
#include "ozz/options/options.h"
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/base/log.h"
#include "ozz/base/maths/box.h"
#include "ozz/base/maths/math_ex.h"
#include "ozz/base/maths/simd_math.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/maths/vec_float.h"
#include <SDL2/SDL.h>
#include <cmath>
#include <vector>
#include <algorithm> // For std::min

#include "batch_animation_system.h"

OZZ_OPTIONS_DECLARE_STRING(skeleton,
                           "Path to the skeleton (ozz archive format).",
                           "media/skeleton.ozz", false)

OZZ_OPTIONS_DECLARE_STRING(animation,
                           "Path to the animation (ozz archive format).",
                           "media/animation.ozz", false)

OZZ_OPTIONS_DECLARE_STRING(mesh,
                           "Path to the skinned mesh (ozz archive format).",
                           "media/mesh.ozz", false)

class SkinningSampleApplication : public ozz::sample::Application {
public:
    // 构造函数：初始化默认渲染选项
    SkinningSampleApplication() : draw_skeleton_(false), draw_mesh_(true), num_instances_(100) {}

protected:
    virtual bool GetCameraInitialSetup(ozz::math::Float3* _center,
                                       ozz::math::Float2* _angles,
                                       float* _distance) const override {
        _center->x = 0.f; _center->y = 15.f;
        _angles->x = -ozz::math::kPi / 4.f; _angles->y = ozz::math::kPi / 4.f;
        *_distance = 50.f;
        return true;
    }

    virtual bool OnUpdate(float _dt, float) override {
        batch_anim_system_.Update(_dt);
        return true;
    }

    virtual bool OnDisplay(ozz::sample::Renderer* _renderer) override {
        bool success = true;

        const auto& all_instances_model_matrices = batch_anim_system_.model_space_matrices();
        if (all_instances_model_matrices.empty()) {
            return true;
        }

        // ====================== 1. 绘制骨骼 (如果启用) ======================
        // 只绘制第一个实例的骨骼作为代表
        if (draw_skeleton_) {
            const auto first_instance_slice = ozz::make_span(all_instances_model_matrices)
                    .subspan(0, batch_anim_system_.skeleton().num_joints());
            success &= _renderer->DrawPosture(batch_anim_system_.skeleton(), first_instance_slice, ozz::math::Float4x4::identity());
        }

        if (!draw_mesh_) {
            return success;
        }

        // ====================== 2. 分批次渲染蒙皮网格 ======================
        const int num_joints_per_instance = batch_anim_system_.skeleton().num_joints();
        const int BATCH_SIZE = 256;

        ozz::vector<float> packed_matrices_for_this_batch;

        //非常核心，这里必须填满
        packed_matrices_for_this_batch.resize(VTF_SIZE_512*VTF_SIZE_512*4, 0.0f);
        for (const auto& mesh : batch_anim_system_.meshes()) {
            if (mesh.triangle_index_count() == 0 || !mesh.skinned()) continue;

            // 外循环遍历【实例批次】
            for (int i = 0; i < num_instances_; i += BATCH_SIZE) {
                const int current_batch_size = std::min(BATCH_SIZE, num_instances_ - i);

                // 为当前批次的所有实例、当前这一个mesh部件，准备蒙皮矩阵
                const size_t num_skinning_joints = mesh.joint_remaps.size();

                int offset = 0;
                for (int batch_instance_idx = 0; batch_instance_idx < current_batch_size; ++batch_instance_idx) {
                    const int global_instance_idx = i + batch_instance_idx;
                    const auto models_slice = ozz::make_span(all_instances_model_matrices)
                            .subspan(global_instance_idx * num_joints_per_instance, num_joints_per_instance);

                    for (size_t j = 0; j < num_skinning_joints; ++j) {
                        const ozz::math::Float4x4 skinning_mat =
                                models_slice[mesh.joint_remaps[j]] * mesh.inverse_bind_poses[j];
                        const float* matrix_floats = reinterpret_cast<const float*>(skinning_mat.cols);

                        // 直接拷贝到目标位置
                        std::copy(matrix_floats, matrix_floats + 16, packed_matrices_for_this_batch.begin() + offset);
                        offset += 16;
                    }
                }

                // 准备当前批次的世界矩阵
                auto world_matrices_span = ozz::make_span(instance_world_matrices_).subspan(i, current_batch_size);

                // 调用渲染器，绘制当前这一小批次的数据
                success &= _renderer->DrawSkinnedMeshVtf(
                        mesh,
                        ozz::make_span(packed_matrices_for_this_batch),
                        ozz::vector<ozz::math::Float4x4>(world_matrices_span.begin(), world_matrices_span.end()),
                        render_options_
                );
            }
        }

        return success;
    }

    virtual bool OnInitialize() override {
        if (!batch_anim_system_.Initialize(OPTIONS_skeleton, OPTIONS_animation, OPTIONS_mesh, num_instances_)) {
            ozz::log::Err() << "Failed to initialize animation system." << std::endl;
            return false;
        }

        instance_world_matrices_.resize(num_instances_);
        const float spacing = 2.0f;
        const int grid_size = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(num_instances_))));
        for (int i = 0; i < num_instances_; ++i) {
            float x = (i % grid_size - grid_size / 2.0f) * spacing;
            float z = (i / grid_size - grid_size / 2.0f) * spacing;
            instance_world_matrices_[i] = ozz::math::Float4x4::Translation(ozz::math::simd_float4::Load(x, 0.f, z, 1.f));
        }

        return true;
    }

    // ====================== 重新加入完整的GUI逻辑 ======================
    virtual bool OnGui(ozz::sample::ImGui* _im_gui) override {
        {
            static bool open = true;
            ozz::sample::ImGui::OpenClose oc(_im_gui, "Instanced Rendering", &open);
            if (open) {
                char label[64];
                std::snprintf(label, sizeof(label), "Instances: %d", num_instances_);
                _im_gui->DoLabel(label);
            }
        }
        {
            static bool ocd_open = true;
            ozz::sample::ImGui::OpenClose ocd(_im_gui, "Display options", &ocd_open);
            if (ocd_open) {
                _im_gui->DoCheckBox("Draw skeleton (first instance)", &draw_skeleton_);
                _im_gui->DoCheckBox("Draw mesh", &draw_mesh_);

                static bool ocr_open = true;
                ozz::sample::ImGui::OpenClose ocr(_im_gui, "Rendering options", &ocr_open);
                if (ocr_open) {
                    _im_gui->DoCheckBox("Show triangles", &render_options_.triangles);
                    _im_gui->DoCheckBox("Show texture (not implemented)", &render_options_.texture);
                    _im_gui->DoCheckBox("Show vertices", &render_options_.vertices);
                    _im_gui->DoCheckBox("Show normals", &render_options_.normals);
                    _im_gui->DoCheckBox("Show tangents", &render_options_.tangents);
                    _im_gui->DoCheckBox("Show binormals", &render_options_.binormals);
                    _im_gui->DoCheckBox("Show colors", &render_options_.colors);
                    _im_gui->DoCheckBox("Wireframe", &render_options_.wireframe);
                }
            }
        }
        return true;
    }

    virtual void GetSceneBounds(ozz::math::Box* _bound) const override {
        if (num_instances_ > 0 && !instance_world_matrices_.empty()) {
            const float spacing = 2.0f;
            const int grid_size = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(num_instances_))));
            float max_extent = grid_size / 2.0f * spacing;
            _bound->min = ozz::math::Float3(-max_extent, 0.f, -max_extent);
            _bound->max = ozz::math::Float3(max_extent, 2.f, max_extent);
        } else {
            ozz::sample::ComputeSkeletonBounds(batch_anim_system_.skeleton(), ozz::math::Float4x4::identity(), _bound);
        }
    }

private:
    ozz::sample::BatchAnimationSystem batch_anim_system_;
    int num_instances_;
    bool draw_skeleton_;
    bool draw_mesh_;
    ozz::sample::Renderer::Options render_options_; // 加回渲染选项
    ozz::vector<ozz::math::Float4x4> instance_world_matrices_;
};

int main(int _argc, char* _argv[]) {
    const char* title = "animation sample";
    return SkinningSampleApplication().Run(_argc, _argv, "1.0", title);
}