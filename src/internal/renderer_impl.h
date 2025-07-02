// 完整替换 src/internal/renderer_impl.h

#ifndef OZZ_SAMPLES_FRAMEWORK_INTERNAL_RENDERER_IMPL_H_
#define OZZ_SAMPLES_FRAMEWORK_INTERNAL_RENDERER_IMPL_H_

#ifndef OZZ_INCLUDE_PRIVATE_HEADER
#error "This header is private, it cannot be included from public headers."
#endif

#include <cstddef>
#include "glad/glad.h"
#include <SDL2/SDL.h>
#include "renderer.h"
#include "ozz/base/containers/vector.h"
#include "ozz/base/log.h"
#include "ozz/base/memory/unique_ptr.h"

#define GL_PTR_OFFSET(i) reinterpret_cast<void*>(static_cast<intptr_t>(i))

namespace ozz {
    namespace animation { class Skeleton; }
    namespace math { struct Float4x4; }
    namespace sample {
        namespace internal {
            class Camera; class Shader; class PointsShader; class SkeletonShader;
            class AmbientShader; class AmbientTexturedShader; class AmbientShaderInstanced;
            class GlImmediateRenderer; class VtfSkinnedShader;

            class RendererImpl : public Renderer {
            public:
                RendererImpl(Camera* _camera);
                virtual ~RendererImpl();
                virtual bool Initialize() override;
                virtual bool DrawAxes(const ozz::math::Float4x4& _transform) override;
                virtual bool DrawGrid(int _cell_count, float _cell_size) override;
                virtual bool DrawSkeleton(const animation::Skeleton& _skeleton,
                                          const ozz::math::Float4x4& _transform,
                                          bool _draw_joints) override;
                virtual bool DrawPosture(const animation::Skeleton& _skeleton,
                                         ozz::span<const ozz::math::Float4x4> _matrices,
                                         const ozz::math::Float4x4& _transform,
                                         bool _draw_joints) override;
                virtual bool DrawPoints(const ozz::span<const float>& _positions,
                                        const ozz::span<const float>& _sizes,
                                        const ozz::span<const Color>& _colors,
                                        const ozz::math::Float4x4& _transform,
                                        bool _screen_space) override;
                virtual bool DrawBoxIm(const ozz::math::Box& _box,
                                       const ozz::math::Float4x4& _transform,
                                       const Color& _color) override;
                virtual bool DrawBoxIm(const ozz::math::Box& _box,
                                       const ozz::math::Float4x4& _transform,
                                       const Color _colors[2]) override;
                virtual bool DrawBoxShaded(const ozz::math::Box& _box,
                                           ozz::span<const ozz::math::Float4x4> _transforms,
                                           const Color& _color) override;
                virtual bool DrawSphereIm(float _radius,
                                          const ozz::math::Float4x4& _transform,
                                          const Color& _color) override;
                virtual bool DrawSphereShaded(
                        float _radius, ozz::span<const ozz::math::Float4x4> _transforms,
                        const Color& _color) override;
                virtual bool DrawSkinnedMesh(const Mesh& _mesh,
                                             const span<math::Float4x4> _skinning_matrices,
                                             const ozz::math::Float4x4& _transform,
                                             const Options& _options = Options()) override;
                virtual bool DrawMesh(const Mesh& _mesh,
                                      const ozz::math::Float4x4& _transform,
                                      const Options& _options = Options()) override;
                virtual bool DrawLines(ozz::span<const math::Float3> _vertices,
                                       const Color& _color,
                                       const ozz::math::Float4x4& _transform) override;
                virtual bool DrawLineStrip(ozz::span<const math::Float3> _vertices,
                                           const Color& _color,
                                           const ozz::math::Float4x4& _transform) override;
                virtual bool DrawVectors(ozz::span<const float> _positions,
                                         size_t _positions_stride,
                                         ozz::span<const float> _directions,
                                         size_t _directions_stride, int _num_vectors,
                                         float _vector_length, const Color& _color,
                                         const ozz::math::Float4x4& _transform) override;
                virtual bool DrawBinormals(
                        ozz::span<const float> _positions, size_t _positions_stride,
                        ozz::span<const float> _normals, size_t _normals_stride,
                        ozz::span<const float> _tangents, size_t _tangents_stride,
                        ozz::span<const float> _handenesses, size_t _handenesses_stride,
                        int _num_vectors, float _vector_length, const Color& _color,
                        const ozz::math::Float4x4& _transform) override;

                // ====================== NEW FUNCTION SIGNATURE ======================
                virtual bool DrawSkinnedMeshVtf(
                        const Mesh& _mesh,
                        const span<const float> _packed_skinning_matrices,
                        const ozz::vector<ozz::math::Float4x4>& _instance_world_matrices,
                        const Options& _options = Options()) override;

                GlImmediateRenderer* immediate_renderer() const { return immediate_.get(); }
                Camera* camera() const { return camera_; }

            private:
                struct Model { Model(); ~Model(); GLuint vbo; GLenum mode; GLsizei count;
                    ozz::unique_ptr<SkeletonShader> shader; };
                bool InitPostureRendering();
                bool InitCheckeredTexture();
                void DrawPosture_Impl(const ozz::math::Float4x4& _transform,
                                      const float* _uniforms, int _instance_count,
                                      bool _draw_joints);
                void DrawPosture_InstancedImpl(const ozz::math::Float4x4& _transform,
                                               const float* _uniforms, int _instance_count,
                                               bool _draw_joints);
                ozz::vector<ozz::math::Float4x4> prealloc_models_;
                Camera* camera_;
                Model models_[2];
                GLuint vertex_array_o_; GLuint dynamic_array_bo_; GLuint dynamic_index_bo_;
                class ScratchBuffer {
                public: ScratchBuffer(); ~ScratchBuffer(); void* Resize(size_t _size);
                private: void* buffer_; size_t size_;
                };
                ScratchBuffer scratch_buffer_;
                ozz::unique_ptr<GlImmediateRenderer> immediate_;
                ozz::unique_ptr<AmbientShader> ambient_shader;
                ozz::unique_ptr<AmbientTexturedShader> ambient_textured_shader;
                ozz::unique_ptr<AmbientShaderInstanced> ambient_shader_instanced;
                ozz::unique_ptr<PointsShader> points_shader;
                GLuint checkered_texture_;
                ozz::unique_ptr<internal::VtfSkinnedShader> vtf_skinned_shader_;
                GLuint skinning_data_texture_;
            };
        }  // namespace internal
    }  // namespace sample
}  // namespace ozz
#endif  // OZZ_SAMPLES_FRAMEWORK_INTERNAL_RENDERER_IMPL_H_