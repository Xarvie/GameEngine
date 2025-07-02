#ifndef OZZ_SAMPLES_FRAMEWORK_ANIMATION_INSTANCE_H_
#define OZZ_SAMPLES_FRAMEWORK_ANIMATION_INSTANCE_H_

#include "utils.h"
#include "mesh.h"
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/maths/vec_float.h"
#include "ozz/base/containers/vector.h"

namespace ozz {
    namespace sample {

        class AdvancedAnimationInstance {
        public:
            AdvancedAnimationInstance();
            ~AdvancedAnimationInstance();

            bool Initialize(const char* skeleton_path, const char* animation_path, const char* mesh_path);

            void Update(float _dt);

            const ozz::animation::Skeleton& skeleton() const { return skeleton_; }
            const ozz::animation::Animation* animation() const { return animation_; }
            const ozz::vector<ozz::sample::Mesh>& meshes() const { return meshes_; }
            const ozz::vector<ozz::math::Float4x4>& model_space_matrices() const { return models_; }

            PlaybackController& controller() { return controller_; }
            const PlaybackController& controller() const { return controller_; }
            ozz::vector<ozz::math::Float4x4>& skinning_matrices() { return skinning_matrices_; }
            ozz::vector<float>& GetPackedSkinningMatrices() { return packed_skinning_matrices_; }

        private:
            PlaybackController controller_;
            ozz::animation::Skeleton skeleton_;
            const ozz::animation::Animation* animation_;
            ozz::animation::SamplingJob::Context context_;

            ozz::vector<ozz::math::SoaTransform> locals_;
            ozz::vector<ozz::math::Float4x4> models_;

            ozz::vector<ozz::sample::Mesh> meshes_;
            ozz::vector<ozz::math::Float4x4> skinning_matrices_;

            ozz::animation::Animation loaded_animation_;
            ozz::vector<float> packed_skinning_matrices_;
        };

    }
}
#endif