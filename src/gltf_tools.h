/*
 * 文件: gltf_tools.h
 * 描述: [高性能重构版] GltfTools 处理器的头文件。
 * - 分离蒙皮骨架提取与静态场景图提取。
 */
#ifndef OZZ_SAMPLES_FRAMEWORK_GLTF_TOOLS_H_
#define OZZ_SAMPLES_FRAMEWORK_GLTF_TOOLS_H_

#include <map>
#include <set>
#include <vector>

#include "mesh.h"
#include "render_material.h"
#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/base/containers/string.h"
#include "ozz/base/containers/vector.h"
#include "ozz/base/memory/unique_ptr.h"

// 前向声明以避免在头文件中包含庞大的tinygltf
namespace tinygltf {
    class Model;
    struct Node;
    struct Mesh;
    struct Primitive;
    struct Skin;
    struct Animation;
    struct AnimationSampler;
    struct Accessor;
}  // namespace tinygltf

namespace GltfTools {

// 核心处理器，封装了从glTF到Ozz运行时资产的完整转换逻辑。
    class Processor {
    public:
        Processor(const ozz::string& gltf_path, const ozz::string& output_path);
        ~Processor();

        bool Run();
        void SetSamplingRate(float _rate) { sampling_rate_ = _rate; }

    private:
        Processor(const Processor&) = delete;
        void operator=(const Processor&) = delete;

        // ========================================================================
        // 内部处理阶段 (重构后的流程)
        // ========================================================================
        bool LoadGltfModel();
        bool PreprocessNames();
        bool ExtractSkinningSkeleton();    // 新增: 提取纯蒙皮骨架
        bool ExtractSceneGraph();          // 新增: 提取静态场景图
        bool ExtractMeshesAndMaterials();
        bool ExtractAnimations();
        bool SerializeAssets();

        // ========================================================================
        // 辅助函数
        // ========================================================================
        void BuildSkeletonHierarchy(
                int _node_index, ozz::animation::offline::RawSkeleton::Joint* _joint,
                const std::set<int>& _allowed_joints);

        void ProcessPrimitive(const tinygltf::Node& _node,
                              const tinygltf::Primitive& _primitive,
                              int _primitive_index);

        // 动画采样辅助函数 (保持不变)
        template <typename _KeyframesType>
        bool SampleChannel(const tinygltf::AnimationSampler& _sampler,
                           float* _duration, _KeyframesType* _keyframes);
        template <typename _KeyframesType>
        bool SampleLinearChannel(const tinygltf::AnimationSampler& _sampler,
                                 float* _duration, _KeyframesType* _keyframes);
        template <typename _KeyframesType>
        bool SampleStepChannel(const tinygltf::AnimationSampler& _sampler,
                               float* _duration, _KeyframesType* _keyframes);
        template <typename _KeyframesType>
        bool SampleCubicSplineChannel(const tinygltf::AnimationSampler& _sampler,
                                      float* _duration, _KeyframesType* _keyframes);
        template <typename T>
        T SampleHermiteSpline(float _alpha, const T& p0, const T& m0, const T& p1,
                              const T& m1);
        template <typename _KeyframesType>
        bool ValidateAndCleanKeyframes(_KeyframesType* _keyframes);

        // glTF数据读取辅助函数 (保持不变)
        template <typename T>
        static ozz::span<const T> GetAccessorData(const tinygltf::Model& _model,
                                                  int _accessor_index);
        static bool GetNodeTransform(const tinygltf::Node& _node,
                                     ozz::math::Transform* _transform);

        // ========================================================================
        // 内部数据
        // ========================================================================
        struct UniqueVertex {
            ozz::math::Float3 position;
            ozz::math::Float3 normal;
            ozz::math::Float2 uv0;
            ozz::math::Float4 tangent;
            uint16_t joint_indices[4];
            ozz::math::Float4 joint_weights;

            bool operator<(const UniqueVertex& _other) const {
                return std::memcmp(this, &_other, sizeof(UniqueVertex)) < 0;
            }
        };

        ozz::string gltf_path_;
        ozz::string output_path_;
        float sampling_rate_ = 30.0f;

        ozz::unique_ptr<tinygltf::Model> model_;

        ozz::unique_ptr<ozz::sample::MeshAsset> mesh_asset_;
        ozz::unique_ptr<ozz::sample::MaterialSet> material_set_;
        ozz::unique_ptr<ozz::animation::Skeleton> skeleton_;
        ozz::vector<ozz::unique_ptr<ozz::animation::Animation>> animations_;

        // [修改] 映射表现在只针对蒙皮骨架
        std::map<int, int> gltf_joint_node_to_ozz_joint_map_;
    };

}  // namespace GltfTools

int gltf2Ozz(int _argc, char* _argv[]);

#endif  // OZZ_SAMPLES_FRAMEWORK_GLTF_TOOLS_H_
