//----------------------------------------------------------------------------//
//                                                                            //
// ozz-animation is hosted at http://github.com/guillaumeblanc/ozz-animation  //
// and distributed under the MIT License (MIT).                               //
//                                                                            //
// Copyright (c) Guillaume Blanc                                              //
//                                                                            //
// Permission is hereby granted, free of charge, to any person obtaining a    //
// copy of this software and associated documentation files (the "Software"), //
// to deal in the Software without restriction, including without limitation  //
// the rights to use, copy, modify, merge, publish, distribute, sublicense,   //
// and/or sell copies of the Software, and to permit persons to whom the      //
// Software is furnished to do so, subject to the following conditions:       //
//                                                                            //
// The above copyright notice and this permission notice shall be included in //
// all copies or substantial portions of the Software.                        //
//                                                                            //
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR //
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   //
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    //
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER //
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    //
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        //
// DEALINGS IN THE SOFTWARE.                                                  //
//                                                                            //
//----------------------------------------------------------------------------//

#ifndef OZZ_SAMPLES_FRAMEWORK_INTERNAL_RENDERER_IMPL_H_
#define OZZ_SAMPLES_FRAMEWORK_INTERNAL_RENDERER_IMPL_H_

#ifndef OZZ_INCLUDE_PRIVATE_HEADER
#error "This header is private, it cannot be included from public headers."
#endif  // OZZ_INCLUDE_PRIVATE_HEADER

#include <cstddef>

//
// #############################################################################
// ## 1. 替换 glfw.h 为 glad.h 和 SDL.h
// ## glad.h 必须在 SDL_opengl.h 和其他任何可能包含 gl.h 的头文件之前
// #############################################################################
//
#include "glad/glad.h"
#include <SDL2/SDL.h>


#include "framework/renderer.h"
#include "ozz/base/containers/vector.h"
#include "ozz/base/log.h"
#include "ozz/base/memory/unique_ptr.h"

// Provides helper macro to test for glGetError on a gl call.
#ifndef NDEBUG
#define GL(_f)                                                     \
  do {                                                             \
    gl##_f;                                                        \
    GLenum gl_err = glGetError();                                  \
    if (gl_err != 0) {                                             \
      ozz::log::Err() << "GL error 0x" << std::hex << gl_err       \
                      << " returned from 'gl" << #_f << std::endl; \
    }                                                              \
    assert(gl_err == GL_NO_ERROR);                                 \
  } while (void(0), 0)

#else  // NDEBUG
#define GL(_f) gl##_f
#endif  // NDEBUG

// Convenient macro definition for specifying buffer offsets.
#define GL_PTR_OFFSET(i) reinterpret_cast<void*>(static_cast<intptr_t>(i))

namespace ozz {
namespace animation {
class Skeleton;
}
namespace math {
struct Float4x4;
}
namespace sample {
namespace internal {
class Camera;
class Shader;
class PointsShader;
class SkeletonShader;
class AmbientShader;
class AmbientTexturedShader;
class AmbientShaderInstanced;
class GlImmediateRenderer;

// Implements Renderer interface.
class RendererImpl : public Renderer {
 public:
  RendererImpl(Camera* _camera);
  virtual ~RendererImpl();

  // See Renderer for all the details about the API.
  virtual bool Initialize();

  virtual bool DrawAxes(const ozz::math::Float4x4& _transform);

  virtual bool DrawGrid(int _cell_count, float _cell_size);

  virtual bool DrawSkeleton(const animation::Skeleton& _skeleton,
                            const ozz::math::Float4x4& _transform,
                            bool _draw_joints);

  virtual bool DrawPosture(const animation::Skeleton& _skeleton,
                           ozz::span<const ozz::math::Float4x4> _matrices,
                           const ozz::math::Float4x4& _transform,
                           bool _draw_joints);

  virtual bool DrawPoints(const ozz::span<const float>& _positions,
                          const ozz::span<const float>& _sizes,
                          const ozz::span<const Color>& _colors,
                          const ozz::math::Float4x4& _transform,
                          bool _screen_space);

  virtual bool DrawBoxIm(const ozz::math::Box& _box,
                         const ozz::math::Float4x4& _transform,
                         const Color& _color);

  virtual bool DrawBoxIm(const ozz::math::Box& _box,
                         const ozz::math::Float4x4& _transform,
                         const Color _colors[2]);

  virtual bool DrawBoxShaded(const ozz::math::Box& _box,
                             ozz::span<const ozz::math::Float4x4> _transforms,
                             const Color& _color);

  virtual bool DrawSphereIm(float _radius,
                            const ozz::math::Float4x4& _transform,
                            const Color& _color);

  virtual bool DrawSphereShaded(
      float _radius, ozz::span<const ozz::math::Float4x4> _transforms,
      const Color& _color);

  virtual bool DrawSkinnedMesh(const Mesh& _mesh,
                               const span<math::Float4x4> _skinning_matrices,
                               const ozz::math::Float4x4& _transform,
                               const Options& _options = Options());

  virtual bool DrawMesh(const Mesh& _mesh,
                        const ozz::math::Float4x4& _transform,
                        const Options& _options = Options());

  virtual bool DrawLines(ozz::span<const math::Float3> _vertices,
                         const Color& _color,
                         const ozz::math::Float4x4& _transform);

  virtual bool DrawLineStrip(ozz::span<const math::Float3> _vertices,
                             const Color& _color,
                             const ozz::math::Float4x4& _transform);

  virtual bool DrawVectors(ozz::span<const float> _positions,
                           size_t _positions_stride,
                           ozz::span<const float> _directions,
                           size_t _directions_stride, int _num_vectors,
                           float _vector_length, const Color& _color,
                           const ozz::math::Float4x4& _transform);

  virtual bool DrawBinormals(
      ozz::span<const float> _positions, size_t _positions_stride,
      ozz::span<const float> _normals, size_t _normals_stride,
      ozz::span<const float> _tangents, size_t _tangents_stride,
      ozz::span<const float> _handenesses, size_t _handenesses_stride,
      int _num_vectors, float _vector_length, const Color& _color,
      const ozz::math::Float4x4& _transform);

  // Get GL immediate renderer implementation;
  GlImmediateRenderer* immediate_renderer() const { return immediate_.get(); }

  // Get application camera that provides rendering matrices.
  Camera* camera() const { return camera_; }

 private:
  // Defines the internal structure used to define a model.
  struct Model {
    Model();
    ~Model();

    GLuint vbo;
    GLenum mode;
    GLsizei count;
    ozz::unique_ptr<SkeletonShader> shader;
  };

  //
  // #############################################################################
  // ## 2. 移除 InitOpenGLExtensions
  // ## glad 会在 application.cc 中处理所有扩展加载
  // #############################################################################
  // bool InitOpenGLExtensions();

  // Initializes posture rendering.
  // Return true if initialization succeeded.
  bool InitPostureRendering();

  // Initializes the checkered texture.
  // Return true if initialization succeeded.
  bool InitCheckeredTexture();

  // Draw posture internal non-instanced rendering fall back implementation.
  void DrawPosture_Impl(const ozz::math::Float4x4& _transform,
                        const float* _uniforms, int _instance_count,
                        bool _draw_joints);

  // Draw posture internal instanced rendering implementation.
  void DrawPosture_InstancedImpl(const ozz::math::Float4x4& _transform,
                                 const float* _uniforms, int _instance_count,
                                 bool _draw_joints);

  // Array of matrices used to store model space matrices during DrawSkeleton
  // execution.
  ozz::vector<ozz::math::Float4x4> prealloc_models_;

  // Application camera that provides rendering matrices.
  Camera* camera_;

  // Bone and joint model objects.
  Model models_[2];

#ifndef EMSCRIPTEN
  // Vertex array
  GLuint vertex_array_o_ = 0;
#endif  // EMSCRIPTEN

  // Dynamic vbo used for arrays.
  GLuint dynamic_array_bo_ = 0;

  // Dynamic vbo used for indices.
  GLuint dynamic_index_bo_ = 0;

  // Volatile memory buffer that can be used within function scope.
  // Minimum alignment is 16 bytes.
  class ScratchBuffer {
   public:
    ScratchBuffer();
    ~ScratchBuffer();

    // Resizes the buffer to the new size and return the memory address.
    void* Resize(size_t _size);

   private:
    void* buffer_;
    size_t size_;
  };
  ScratchBuffer scratch_buffer_;

  // Immediate renderer implementation.
  ozz::unique_ptr<GlImmediateRenderer> immediate_;

  // Ambient rendering shader.
  ozz::unique_ptr<AmbientShader> ambient_shader;
  ozz::unique_ptr<AmbientTexturedShader> ambient_textured_shader;
  ozz::unique_ptr<AmbientShaderInstanced> ambient_shader_instanced;
  ozz::unique_ptr<PointsShader> points_shader;

  // Checkered texture
  GLuint checkered_texture_ = 0;
};
}  // namespace internal
}  // namespace sample
}  // namespace ozz

//
// #############################################################################
// ## 3. 移除所有 PFNGL... 函数指针声明
// ## glad 会提供所有这些函数的定义
// #############################################################################
//

#endif  // OZZ_SAMPLES_FRAMEWORK_INTERNAL_RENDERER_IMPL_H_
