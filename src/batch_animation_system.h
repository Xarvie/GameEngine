// 完整替换 src/batch_animation_system.h

#ifndef OZZ_SAMPLES_FRAMEWORK_BATCH_ANIMATION_SYSTEM_H_
#define OZZ_SAMPLES_FRAMEWORK_BATCH_ANIMATION_SYSTEM_H_

#include "utils.h"
#include "mesh.h"
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/maths/vec_float.h"
#include "ozz/base/containers/vector.h"
#include "ozz/base/memory/unique_ptr.h"

namespace ozz {
    namespace sample {

        class BatchAnimationSystem {
        public:
            BatchAnimationSystem();
            ~BatchAnimationSystem();

            bool Initialize(const char* skeleton_path, const char* animation_path, const char* mesh_path, int num_instances);

            void Update(float _dt);

            // === 接口 ===
            const ozz::animation::Skeleton& skeleton() const { return skeleton_; }
            const ozz::animation::Animation& animation() const { return animation_; }
            const ozz::vector<ozz::sample::Mesh>& meshes() const { return meshes_; }
            // 返回包含所有实例的所有骨骼模型空间矩阵的缓冲区
            const ozz::vector<ozz::math::Float4x4>& model_space_matrices() const { return models_buffer_; }

        private:
            // 为每个实例存储一个控制器和采样上下文
            struct Instance {
                PlaybackController controller;
                ozz::unique_ptr<ozz::animation::SamplingJob::Context> context;
            };
            ozz::vector<Instance> instances_;

            int num_instances_ = 0;

            ozz::animation::Skeleton skeleton_;
            ozz::animation::Animation animation_;
            ozz::vector<ozz::sample::Mesh> meshes_;

            // 巨大的缓冲区，用于存储所有实例的动画计算结果
            ozz::vector<ozz::math::SoaTransform> locals_buffer_;
            ozz::vector<ozz::math::Float4x4> models_buffer_;
        };

    } // namespace sample
} // namespace ozz
#endif // OZZ_SAMPLES_FRAMEWORK_BATCH_ANIMATION_SYSTEM_H_