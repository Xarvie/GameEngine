#include "animation_instance.h"

#include "ozz/base/log.h"
#include "ozz/base/maths/math_ex.h"
#include "ozz/animation/runtime/local_to_model_job.h"

namespace ozz {
    namespace sample {

        AdvancedAnimationInstance::AdvancedAnimationInstance() : animation_(nullptr) {}

        AdvancedAnimationInstance::~AdvancedAnimationInstance() {}

        bool AdvancedAnimationInstance::Initialize(const char* skeleton_path, const char* animation_path, const char* mesh_path) {
            if (!ozz::sample::LoadSkeleton(skeleton_path, &skeleton_)) {
                return false;
            }

            if (!ozz::sample::LoadAnimation(animation_path, &loaded_animation_)) {
                return false;
            }
            animation_ = &loaded_animation_;

            if (skeleton_.num_joints() != animation_->num_tracks()) {
                ozz::log::Err() << "Animation doesn't match skeleton." << std::endl;
                return false;
            }

            locals_.resize(skeleton_.num_soa_joints());
            models_.resize(skeleton_.num_joints());
            context_.Resize(skeleton_.num_joints());

            if (!ozz::sample::LoadMeshes(mesh_path, &meshes_)) {
                return false;
            }

            size_t num_skinning_matrices = 0;
            for (const ozz::sample::Mesh& mesh : meshes_) {
                num_skinning_matrices = ozz::math::Max(num_skinning_matrices, mesh.joint_remaps.size());
                if (skeleton_.num_joints() < mesh.highest_joint_index()) {
                    ozz::log::Err() << "Mesh doesn't match skeleton." << std::endl;
                    return false;
                }
            }
            skinning_matrices_.resize(num_skinning_matrices);
            packed_skinning_matrices_.resize(num_skinning_matrices * 16);
            return true;
        }

        // 在 src/animation_instance.cpp 的 Update() 函数中
        void AdvancedAnimationInstance::Update(float _dt) {
            if (!animation_) {
                return;
            }
            controller_.Update(*animation_, _dt);

            ozz::animation::SamplingJob sampling_job;
            sampling_job.animation = animation_;
            sampling_job.context = &context_;
            sampling_job.ratio = controller_.time_ratio();
            sampling_job.output = make_span(locals_);
            if (!sampling_job.Run()) {
                return;
            }

            ozz::animation::LocalToModelJob ltm_job;
            ltm_job.skeleton = &skeleton_;
            ltm_job.input = make_span(locals_);
            ltm_job.output = make_span(models_);
            ltm_job.Run();

            // === 删除所有我们后来添加的蒙皮矩阵计算和打包代码 ===
        }

    }
}