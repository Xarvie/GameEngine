// 完整替换 src/batch_animation_system.cpp

#include "batch_animation_system.h"
#include "ozz/base/log.h"
#include "ozz/base/maths/math_ex.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include <cstdlib> // for rand()
#include <ctime>   // for time()

namespace ozz {
    namespace sample {

        BatchAnimationSystem::BatchAnimationSystem() {}
        BatchAnimationSystem::~BatchAnimationSystem() {}

        bool BatchAnimationSystem::Initialize(const char* skeleton_path, const char* animation_path, const char* mesh_path, int num_instances) {
            num_instances_ = num_instances;
            if (num_instances_ <= 0) return false;

            if (!ozz::sample::LoadSkeleton(skeleton_path, &skeleton_)) return false;
            if (!ozz::sample::LoadAnimation(animation_path, &animation_)) return false;
            if (!ozz::sample::LoadMeshes(mesh_path, &meshes_) || meshes_.empty()) return false;
            if (skeleton_.num_joints() != animation_.num_tracks()) return false;

            for(const auto& mesh : meshes_) {
                if (skeleton_.num_joints() < mesh.highest_joint_index()) return false;
            }

            const int num_soa_joints = skeleton_.num_soa_joints();
            const int num_model_joints = skeleton_.num_joints();

            // 为所有实例分配资源
            instances_.resize(num_instances_);
            std::srand(std::time(nullptr)); // 初始化随机种子
            for (int i = 0; i < num_instances_; ++i) {
                instances_[i].context = ozz::make_unique<ozz::animation::SamplingJob::Context>();
                instances_[i].context->Resize(num_model_joints);
                // 给每个实例一个随机的初始动画时间，让它们看起来不一样
                instances_[i].controller.set_time_ratio(static_cast<float>(std::rand()) / RAND_MAX);
            }

            locals_buffer_.resize(num_soa_joints * num_instances_);
            models_buffer_.resize(num_model_joints * num_instances_);

            return true;
        }

        void BatchAnimationSystem::Update(float _dt) {
            const int num_soa_joints = skeleton_.num_soa_joints();
            const int num_model_joints = skeleton_.num_joints();

            // 为所有实例更新动画并计算模型空间矩阵
            for (int i = 0; i < num_instances_; ++i) {
                instances_[i].controller.Update(animation_, _dt);

                // 定义当前实例在巨大缓冲区中的数据切片
                auto locals_slice = make_span(locals_buffer_).subspan(i * num_soa_joints, num_soa_joints);
                auto models_slice = make_span(models_buffer_).subspan(i * num_model_joints, num_model_joints);

                // 采样动画
                ozz::animation::SamplingJob sampling_job;
                sampling_job.animation = &animation_;
                sampling_job.context = instances_[i].context.get();
                sampling_job.ratio = instances_[i].controller.time_ratio();
                sampling_job.output = locals_slice;
                if (!sampling_job.Run()) continue;

                // 转换为模型空间矩阵
                ozz::animation::LocalToModelJob ltm_job;
                ltm_job.skeleton = &skeleton_;
                ltm_job.input = locals_slice;
                ltm_job.output = models_slice;
                ltm_job.Run();
            }
        }

    } // namespace sample
} // namespace ozz