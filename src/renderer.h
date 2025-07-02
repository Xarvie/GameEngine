// 完整替换 src/renderer.h

#ifndef OZZ_SAMPLES_FRAMEWORK_RENDERER_H_
#define OZZ_SAMPLES_FRAMEWORK_RENDERER_H_

#include "ozz/base/platform.h"
#include "ozz/base/span.h"
#include "ozz/base/containers/vector.h" // Needed for vector
#define VTF_SIZE_512 512
namespace ozz {
    namespace animation {
        class Skeleton;
    }
    namespace math {
        struct Float4x4;
        struct Float3;
        struct Box;
    }  // namespace math
    namespace sample {

        struct Mesh;
        struct Color { float r, g, b, a; };
        static const Color kRed = {1, 0, 0, 1};
        static const Color kGreen = {0, 1, 0, 1};
        static const Color kBlue = {0, 0, 1, 1};
        static const Color kWhite = {1, 1, 1, 1};
        static const Color kYellow = {1, 1, 0, 1};
        static const Color kMagenta = {1, 0, 1, 1};
        static const Color kCyan = {0, 1, 1, 1};
        static const Color kGrey = {.5f, .5f, .5f, 1};
        static const Color kBlack = {.5f, .5f, .5f, 1};

        class Renderer {
        public:
            virtual ~Renderer() {}
            virtual bool Initialize() = 0;
            virtual bool DrawAxes(const ozz::math::Float4x4& _transform) = 0;
            virtual bool DrawGrid(int _cell_count, float _cell_size) = 0;
            virtual bool DrawSkeleton(const animation::Skeleton& _skeleton,
                                      const ozz::math::Float4x4& _transform,
                                      bool _draw_joints = true) = 0;
            virtual bool DrawPosture(const animation::Skeleton& _skeleton,
                                     ozz::span<const ozz::math::Float4x4> _matrices,
                                     const ozz::math::Float4x4& _transform,
                                     bool _draw_joints = true) = 0;
            virtual bool DrawPoints(const ozz::span<const float>& _positions,
                                    const ozz::span<const float>& _sizes,
                                    const ozz::span<const Color>& _colors,
                                    const ozz::math::Float4x4& _transform,
                                    bool _screen_space = false) = 0;
            virtual bool DrawBoxIm(const ozz::math::Box& _box,
                                   const ozz::math::Float4x4& _transform,
                                   const Color& _color) = 0;
            virtual bool DrawBoxIm(const ozz::math::Box& _box,
                                   const ozz::math::Float4x4& _transform,
                                   const Color _colors[2]) = 0;
            virtual bool DrawBoxShaded(const ozz::math::Box& _box,
                                       ozz::span<const ozz::math::Float4x4> _transforms,
                                       const Color& _color) = 0;
            virtual bool DrawSphereIm(float _radius,
                                      const ozz::math::Float4x4& _transform,
                                      const Color& _color) = 0;
            virtual bool DrawSphereShaded(
                    float _radius, ozz::span<const ozz::math::Float4x4> _transforms,
                    const Color& _color) = 0;

            struct Options {
                bool triangles; bool texture; bool vertices; bool normals; bool tangents;
                bool binormals; bool colors; bool wireframe; bool skip_skinning;
                Options() : triangles(true), texture(false), vertices(false), normals(false),
                            tangents(false), binormals(false), colors(false),
                            wireframe(false), skip_skinning(false) {}
            };

            virtual bool DrawSkinnedMesh(const Mesh& _mesh,
                                         const span<math::Float4x4> _skinning_matrices,
                                         const ozz::math::Float4x4& _transform,
                                         const Options& _options = Options()) = 0;

            virtual bool DrawMesh(const Mesh& _mesh,
                                  const ozz::math::Float4x4& _transform,
                                  const Options& _options = Options()) = 0;

            virtual bool DrawLines(ozz::span<const math::Float3> _vertices,
                                   const Color& _color,
                                   const ozz::math::Float4x4& _transform) = 0;
            virtual bool DrawLineStrip(ozz::span<const math::Float3> _vertices,
                                       const Color& _color,
                                       const ozz::math::Float4x4& _transform) = 0;
            virtual bool DrawVectors(ozz::span<const float> _positions,
                                     size_t _positions_stride,
                                     ozz::span<const float> _directions,
                                     size_t _directions_stride, int _num_vectors,
                                     float _vector_length, const Color& _color,
                                     const ozz::math::Float4x4& _transform) = 0;
            virtual bool DrawBinormals(
                    ozz::span<const float> _positions, size_t _positions_stride,
                    ozz::span<const float> _normals, size_t _normals_stride,
                    ozz::span<const float> _tangents, size_t _tangents_stride,
                    ozz::span<const float> _handenesses, size_t _handenesses_stride,
                    int _num_vectors, float _vector_length, const Color& _color,
                    const ozz::math::Float4x4& _transform) = 0;

            // ====================== NEW FUNCTION SIGNATURE ======================
            // This is the updated declaration that fixes the "abstract class" error.
            virtual bool DrawSkinnedMeshVtf(
                    const Mesh& _mesh,
                    const span<const float> _packed_skinning_matrices,
                    const ozz::vector<ozz::math::Float4x4>& _instance_world_matrices,
                    const Options& _options = Options()) = 0;
        };
    }  // namespace sample
}  // namespace ozz
#endif  // OZZ_SAMPLES_FRAMEWORK_RENDERER_H_