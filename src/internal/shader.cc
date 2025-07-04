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

#define OZZ_INCLUDE_PRIVATE_HEADER  // Allows to include private headers.

#include "shader.h"

#include <cassert>
#include <cstdio>

#include "ozz/base/log.h"
#include "ozz/base/maths/simd_math.h"

namespace ozz {
namespace sample {
namespace internal {

#ifdef __EMSCRIPTEN__
// WebGL requires to specify floating point precision
static const char* kPlatformSpecificVSHeader =
    "#version 300 es\n precision mediump float;\n";
static const char* kPlatformSpecificFSHeader =
    "#version 300 es\n precision mediump float;\n";
#else   // __EMSCRIPTEN__
static const char* kPlatformSpecificVSHeader = "#version 330\n";
static const char* kPlatformSpecificFSHeader = "#version 330\n";
#endif  // __EMSCRIPTEN__

void glUniformMat4(ozz::math::Float4x4 _mat4, GLint _uniform) {
  float values[16];
  math::StorePtrU(_mat4.cols[0], values + 0);
  math::StorePtrU(_mat4.cols[1], values + 4);
  math::StorePtrU(_mat4.cols[2], values + 8);
  math::StorePtrU(_mat4.cols[3], values + 12);
  glUniformMatrix4fv(_uniform, 1, false, values);
}

Shader::Shader() : program_(0), vertex_(0), fragment_(0) {}

Shader::~Shader() {
  if (vertex_) {
    glDetachShader(program_, vertex_);
    glDeleteShader(vertex_);
  }
  if (fragment_) {
    glDetachShader(program_, fragment_);
    glDeleteShader(fragment_);
  }
  if (program_) {
    glDeleteProgram(program_);
  }
}

namespace {
GLuint CompileShader(GLenum _type, int _count, const char** _src) {
  GLuint shader = glCreateShader(_type);
  glShaderSource(shader, _count, _src, nullptr);
  glCompileShader(shader);

  int infolog_length = 0;
  glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infolog_length);
  if (infolog_length > 1) {
    char* info_log = reinterpret_cast<char*>(
        memory::default_allocator()->Allocate(infolog_length, alignof(char)));
    int chars_written = 0;
    glGetShaderInfoLog(shader, infolog_length, &chars_written, info_log);
    log::Err() << info_log << std::endl;
    memory::default_allocator()->Deallocate(info_log);
  }

  int status;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if (status) {
    return shader;
  }

  glDeleteShader(shader);
  return 0;
}
}  // namespace

bool Shader::BuildFromSource(int _vertex_count, const char** _vertex,
                             int _fragment_count, const char** _fragment) {
  // Tries to compile shaders.
  GLuint vertex_shader = 0;
  if (_vertex) {
      for(int i = 0 ;  i< _vertex_count; i++){
          log::Out() << _vertex[i] << std::endl;
      }


    vertex_shader = CompileShader(GL_VERTEX_SHADER, _vertex_count, _vertex);
    if (!vertex_shader) {
      return false;
    }
  }
  GLuint fragment_shader = 0;
  if (_fragment) {
      for(int i = 0 ;  i< _fragment_count; i++){
          log::Out() << _vertex[i] << std::endl;
      }
    fragment_shader =
        CompileShader(GL_FRAGMENT_SHADER, _fragment_count, _fragment);
    if (!fragment_shader) {
      if (vertex_shader) {
        glDeleteShader(vertex_shader);
      }
      return false;
    }
  }

  // Shaders are compiled, builds program.
  program_ = glCreateProgram();
  vertex_ = vertex_shader;
  fragment_ = fragment_shader;
  glAttachShader(program_, vertex_shader);
  glAttachShader(program_, fragment_shader);
  glLinkProgram(program_);

  int infolog_length = 0;
  glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &infolog_length);
  if (infolog_length > 1) {
    char* info_log = reinterpret_cast<char*>(
        memory::default_allocator()->Allocate(infolog_length, alignof(char)));
    int chars_written = 0;
    glGetProgramInfoLog(program_, infolog_length, &chars_written, info_log);
    log::Err() << info_log << std::endl;
    memory::default_allocator()->Deallocate(info_log);
  }

  return true;
}

bool Shader::BindUniform(const char* _semantic) {
  if (!program_) {
    return false;
  }
  GLint location = glGetUniformLocation(program_, _semantic);
  if (glGetError() != GL_NO_ERROR || location == -1) {  // _semantic not found.
    return false;
  }
  uniforms_.push_back(location);
  return true;
}

bool Shader::FindAttrib(const char* _semantic) {
  if (!program_) {
    return false;
  }
  GLint location = glGetAttribLocation(program_, _semantic);
  if (glGetError() != GL_NO_ERROR || location == -1) {  // _semantic not found.
    return false;
  }
  attribs_.push_back(location);
  return true;
}

void Shader::UnbindAttribs() {
  for (size_t i = 0; i < attribs_.size(); ++i) {
    glDisableVertexAttribArray(attribs_[i]);
  }
}

void Shader::Unbind() {
  UnbindAttribs();
  glUseProgram(0);
}

ozz::unique_ptr<ImmediatePCShader> ImmediatePCShader::Build() {
  bool success = true;

  const char* kSimplePCVS =
      "uniform mat4 u_mvp;\n"
      "in vec3 a_position;\n"
      "in vec4 a_color;\n"
      "out vec4 v_vertex_color;\n"
      "void main() {\n"
      "  vec4 vertex = vec4(a_position.xyz, 1.);\n"
      "  gl_Position = u_mvp * vertex;\n"
      "  v_vertex_color = a_color;\n"
      "}\n";
  const char* kSimplePCPS =
      "in vec4 v_vertex_color;\n"
      "out vec4 o_color;\n"
      "void main() {\n"
      "  o_color = v_vertex_color;\n"
      "}\n";

  const char* vs[] = {kPlatformSpecificVSHeader, kSimplePCVS};
  const char* fs[] = {kPlatformSpecificFSHeader, kSimplePCPS};

  ozz::unique_ptr<ImmediatePCShader> shader = make_unique<ImmediatePCShader>();
  success &=
      shader->BuildFromSource(OZZ_ARRAY_SIZE(vs), vs, OZZ_ARRAY_SIZE(fs), fs);

  // Binds default attributes
  success &= shader->FindAttrib("a_position");
  success &= shader->FindAttrib("a_color");

  // Binds default uniforms
  success &= shader->BindUniform("u_mvp");

  if (!success) {
    shader.reset();
  }

  return shader;
}

void ImmediatePCShader::Bind(const math::Float4x4& _model,
                             const math::Float4x4& _view_proj,
                             GLsizei _pos_stride, GLsizei _pos_offset,
                             GLsizei _color_stride, GLsizei _color_offset) {
  glUseProgram(program());

  const GLint position_attrib = attrib(0);
  glEnableVertexAttribArray(position_attrib);
  glVertexAttribPointer(position_attrib, 3, GL_FLOAT, GL_FALSE, _pos_stride, GL_PTR_OFFSET(_pos_offset));

  const GLint color_attrib = attrib(1);
  glEnableVertexAttribArray(color_attrib);
  glVertexAttribPointer(color_attrib, 4, GL_FLOAT, GL_FALSE, _color_stride, GL_PTR_OFFSET(_color_offset));

  // Binds mvp uniform
  glUniformMat4(_view_proj * _model, uniform(0));
}

ozz::unique_ptr<ImmediatePTCShader> ImmediatePTCShader::Build() {
  bool success = true;

  const char* kSimplePCVS =
      "uniform mat4 u_mvp;\n"
      "in vec3 a_position;\n"
      "in vec2 a_tex_coord;\n"
      "in vec4 a_color;\n"
      "out vec4 v_vertex_color;\n"
      "out vec2 v_texture_coord;\n"
      "void main() {\n"
      "  vec4 vertex = vec4(a_position.xyz, 1.);\n"
      "  gl_Position = u_mvp * vertex;\n"
      "  v_vertex_color = a_color;\n"
      "  v_texture_coord = a_tex_coord;\n"
      "}\n";
  const char* kSimplePCPS =
      "uniform sampler2D u_texture;\n"
      "in vec4 v_vertex_color;\n"
      "in vec2 v_texture_coord;\n"
      "out vec4 o_color;\n"
      "void main() {\n"
      "  vec4 tex_color = texture(u_texture, v_texture_coord);\n"
      "  o_color = v_vertex_color * tex_color;\n"
      "  if(o_color.a < .01) discard;\n"  // Implements alpha testing.
      "}\n";

  const char* vs[] = {kPlatformSpecificVSHeader, kSimplePCVS};
  const char* fs[] = {kPlatformSpecificFSHeader, kSimplePCPS};

  ozz::unique_ptr<ImmediatePTCShader> shader =
      make_unique<ImmediatePTCShader>();
  success &=
      shader->BuildFromSource(OZZ_ARRAY_SIZE(vs), vs, OZZ_ARRAY_SIZE(fs), fs);

  // Binds default attributes
  success &= shader->FindAttrib("a_position");
  success &= shader->FindAttrib("a_tex_coord");
  success &= shader->FindAttrib("a_color");

  // Binds default uniforms
  success &= shader->BindUniform("u_mvp");
  success &= shader->BindUniform("u_texture");

  if (!success) {
    shader.reset();
  }

  return shader;
}

void ImmediatePTCShader::Bind(const math::Float4x4& _model,
                              const math::Float4x4& _view_proj,
                              GLsizei _pos_stride, GLsizei _pos_offset,
                              GLsizei _tex_stride, GLsizei _tex_offset,
                              GLsizei _color_stride, GLsizei _color_offset) {
  glUseProgram(program());

  const GLint position_attrib = attrib(0);
  glEnableVertexAttribArray(position_attrib);
 glVertexAttribPointer(position_attrib, 3, GL_FLOAT, GL_FALSE, _pos_stride,
                         GL_PTR_OFFSET(_pos_offset));

  const GLint tex_attrib = attrib(1);
  glEnableVertexAttribArray(tex_attrib);
 glVertexAttribPointer(tex_attrib, 2, GL_FLOAT, GL_FALSE, _tex_stride,
                         GL_PTR_OFFSET(_tex_offset));

  const GLint color_attrib = attrib(2);
  glEnableVertexAttribArray(color_attrib);
 glVertexAttribPointer(color_attrib, 4, GL_FLOAT, GL_FALSE, _color_stride,
                         GL_PTR_OFFSET(_color_offset));

  // Binds mvp uniform
  glUniformMat4(_view_proj * _model, uniform(0));

  // Binds texture
  const GLint texture = uniform(1);
  glUniform1i(texture, 0);
}

ozz::unique_ptr<PointsShader> PointsShader::Build() {
  bool success = true;

  const char* kSimplePointsVS =
      "uniform mat4 u_mvp;\n"
      "in vec3 a_position;\n"
      "in vec4 a_color;\n"
      "in float a_size;\n"
      "in float a_screen_space;\n"
      "out vec4 v_vertex_color;\n"
      "void main() {\n"
      "  vec4 vertex = vec4(a_position.xyz, 1.);\n"
      "  gl_Position = u_mvp * vertex;\n"
      "  gl_PointSize = a_screen_space == 0. ? a_size / gl_Position.w : "
      "a_size;\n"
      "  v_vertex_color = a_color;\n"
      "}\n";
  const char* kSimplePointsPS =
      "in vec4 v_vertex_color;\n"
      "out vec4 o_color;\n"
      "void main() {\n"
      "  o_color = v_vertex_color;\n"
      "}\n";

  const char* vs[] = {kPlatformSpecificVSHeader, kSimplePointsVS};
  const char* fs[] = {kPlatformSpecificFSHeader, kSimplePointsPS};

  ozz::unique_ptr<PointsShader> shader = make_unique<PointsShader>();
  success &=
      shader->BuildFromSource(OZZ_ARRAY_SIZE(vs), vs, OZZ_ARRAY_SIZE(fs), fs);

  // Binds default attributes
  success &= shader->FindAttrib("a_position");
  success &= shader->FindAttrib("a_color");
  success &= shader->FindAttrib("a_size");
  success &= shader->FindAttrib("a_screen_space");

  // Binds default uniforms
  success &= shader->BindUniform("u_mvp");

  if (!success) {
    shader.reset();
  }

  return shader;
}

PointsShader::GenericAttrib PointsShader::Bind(
    const math::Float4x4& _model, const math::Float4x4& _view_proj,
    GLsizei _pos_stride, GLsizei _pos_offset, GLsizei _color_stride,
    GLsizei _color_offset, GLsizei _size_stride, GLsizei _size_offset,
    bool _screen_space) {
  glUseProgram(program());

  const GLint position_attrib = attrib(0);
  glEnableVertexAttribArray(position_attrib);
 glVertexAttribPointer(position_attrib, 3, GL_FLOAT, GL_FALSE, _pos_stride,
                         GL_PTR_OFFSET(_pos_offset));

  const GLint color_attrib = attrib(1);
  if (_color_stride) {
    glEnableVertexAttribArray(color_attrib);
   glVertexAttribPointer(color_attrib, 4, GL_FLOAT, GL_FALSE, _color_stride,
                           GL_PTR_OFFSET(_color_offset));
  }
  const GLint size_attrib = attrib(2);
  if (_size_stride) {
    glEnableVertexAttribArray(size_attrib);
   glVertexAttribPointer(size_attrib, 1, GL_FLOAT, GL_TRUE, _size_stride,
                           GL_PTR_OFFSET(_size_offset));
  }
  const GLint screen_space_attrib = attrib(3);
  glVertexAttrib1f(screen_space_attrib, 1.f * _screen_space);

  // Binds mvp uniform
  glUniformMat4(_view_proj * _model, uniform(0));

  return {_color_stride ? -1 : color_attrib, _size_stride ? -1 : size_attrib};
}

namespace {
const char* kPassUv =
    "in vec2 a_uv;\n"
    "out vec2 v_vertex_uv;\n"
    "void PassUv() {\n"
    "  v_vertex_uv = a_uv;\n"
    "}\n";
const char* kPassNoUv =
    "void PassUv() {\n"
    "}\n";
const char* kShaderUberVS =
    "uniform mat4 u_viewproj;\n"
    "in vec3 a_position;\n"
    "in vec3 a_normal;\n"
    "in vec4 a_color;\n"
    "out vec3 v_world_normal;\n"
    "out vec4 v_vertex_color;\n"
    "void main() {\n"
    "  mat4 world_matrix = GetWorldMatrix();\n"
    "  vec4 vertex = vec4(a_position.xyz, 1.);\n"
    "  gl_Position = u_viewproj * world_matrix * vertex;\n"
    "  mat3 cross_matrix = mat3(\n"
    "    cross(world_matrix[1].xyz, world_matrix[2].xyz),\n"
    "    cross(world_matrix[2].xyz, world_matrix[0].xyz),\n"
    "    cross(world_matrix[0].xyz, world_matrix[1].xyz));\n"
    "  float invdet = 1.0 / dot(cross_matrix[2], world_matrix[2].xyz);\n"
    "  mat3 normal_matrix = cross_matrix * invdet;\n"
    "  v_world_normal = normal_matrix * a_normal;\n"
    "  v_vertex_color = a_color;\n"
    "  PassUv();\n"
    "}\n";
const char* kShaderAmbientFct =
    "vec4 GetAmbient(vec3 _world_normal) {\n"
    "  vec3 normal = normalize(_world_normal);\n"
    "  vec3 alpha = (normal + 1.) * .5;\n"
    "  vec2 bt = mix(vec2(.3, .7), vec2(.4, .8), alpha.xz);\n"
    "  vec3 ambient = mix(vec3(bt.x, .3, bt.x), vec3(bt.y, .8, bt.y), "
    "alpha.y);\n"
    "  return vec4(ambient, 1.);\n"
    "}\n";
const char* kShaderAmbientFS =
    "in vec3 v_world_normal;\n"
    "in vec4 v_vertex_color;\n"
    "out vec4 o_color;\n"
    "void main() {\n"
    "  vec4 ambient = GetAmbient(v_world_normal);\n"
    "  o_color = ambient *\n"
    "                 v_vertex_color;\n"
    "}\n";
const char* kShaderAmbientTexturedFS =
    "uniform sampler2D u_texture;\n"
    "in vec3 v_world_normal;\n"
    "in vec4 v_vertex_color;\n"
    "in vec2 v_vertex_uv;\n"
    "out vec4 o_color;\n"
    "void main() {\n"
    "  vec4 ambient = GetAmbient(v_world_normal);\n"
    "  o_color = ambient *\n"
    "                 v_vertex_color *\n"
    "                 texture(u_texture, v_vertex_uv);\n"
    "}\n";
}  // namespace

void SkeletonShader::Bind(const math::Float4x4& _model,
                          const math::Float4x4& _view_proj, GLsizei _pos_stride,
                          GLsizei _pos_offset, GLsizei _normal_stride,
                          GLsizei _normal_offset, GLsizei _color_stride,
                          GLsizei _color_offset) {
  glUseProgram(program());

  const GLint position_attrib = attrib(0);
  glEnableVertexAttribArray(position_attrib);
 glVertexAttribPointer(position_attrib, 3, GL_FLOAT, GL_FALSE, _pos_stride,
                         GL_PTR_OFFSET(_pos_offset));

  const GLint normal_attrib = attrib(1);
  glEnableVertexAttribArray(normal_attrib);
 glVertexAttribPointer(normal_attrib, 3, GL_FLOAT, GL_FALSE, _normal_stride,
                         GL_PTR_OFFSET(_normal_offset));

  const GLint color_attrib = attrib(2);
  glEnableVertexAttribArray(color_attrib);
 glVertexAttribPointer(color_attrib, 4, GL_FLOAT, GL_FALSE, _color_stride,
                         GL_PTR_OFFSET(_color_offset));

  // Binds vp uniform
  glUniformMat4(_model, uniform(0));

  // Binds vp uniform
  glUniformMat4(_view_proj, uniform(1));
}

ozz::unique_ptr<JointShader> JointShader::Build() {
  bool success = true;

  const char* vs_joint_to_world_matrix =
      "uniform mat4 u_model;\n"
      "mat4 GetWorldMatrix() {\n"
      "  // Rebuilds joint matrix.\n"
      "  mat4 joint_matrix;\n"
      "  joint_matrix[0] = vec4(normalize(joint[0].xyz), 0.);\n"
      "  joint_matrix[1] = vec4(normalize(joint[1].xyz), 0.);\n"
      "  joint_matrix[2] = vec4(normalize(joint[2].xyz), 0.);\n"
      "  joint_matrix[3] = vec4(joint[3].xyz, 1.);\n"

      "  // Rebuilds bone properties.\n"
      "  vec3 bone_dir = vec3(joint[0].w, joint[1].w, joint[2].w);\n"
      "  float bone_len = length(bone_dir);\n"

      "  // Setup rendering world matrix.\n"
      "  mat4 world_matrix;\n"
      "  world_matrix[0] = joint_matrix[0] * bone_len;\n"
      "  world_matrix[1] = joint_matrix[1] * bone_len;\n"
      "  world_matrix[2] = joint_matrix[2] * bone_len;\n"
      "  world_matrix[3] = joint_matrix[3];\n"
      "  return u_model * world_matrix;\n"
      "}\n";

  const char* vs[] = {kPlatformSpecificVSHeader, kPassNoUv,
                       "in mat4 joint;\n",
                      vs_joint_to_world_matrix, kShaderUberVS};
  const char* fs[] = {kPlatformSpecificFSHeader, kShaderAmbientFct,
                      kShaderAmbientFS};

  ozz::unique_ptr<JointShader> shader = make_unique<JointShader>();
  success &=
      shader->BuildFromSource(OZZ_ARRAY_SIZE(vs), vs, OZZ_ARRAY_SIZE(fs), fs);

  // Binds default attributes
  success &= shader->FindAttrib("a_position");
  success &= shader->FindAttrib("a_normal");
  success &= shader->FindAttrib("a_color");
  success &= shader->FindAttrib("joint");
  // Binds default uniforms
  success &= shader->BindUniform("u_model");
  success &= shader->BindUniform("u_viewproj");


  if (!success) {
    shader.reset();
  }

  return shader;
}

ozz::unique_ptr<BoneShader>
BoneShader::Build() {  // Builds a world matrix from joint uniforms,
                       // sticking bone model between
  bool success = true;

  // parent and child joints.
  const char* vs_joint_to_world_matrix =
      "uniform mat4 u_model;\n"
      "mat4 GetWorldMatrix() {\n"
      "  // Rebuilds bone properties.\n"
      "  // Bone length is set to zero to disable leaf rendering.\n"
      "  float is_bone = joint[3].w;\n"
      "  vec3 bone_dir = vec3(joint[0].w, joint[1].w, joint[2].w) * is_bone;\n"
      "  float bone_len = length(bone_dir);\n"

      "  // Setup rendering world matrix.\n"
      "  float dot1 = dot(joint[2].xyz, bone_dir);\n"
      "  float dot2 = dot(joint[0].xyz, bone_dir);\n"
      "  vec3 binormal = abs(dot1) < abs(dot2) ? joint[2].xyz : joint[0].xyz;\n"

      "  mat4 world_matrix;\n"
      "  world_matrix[0] = vec4(bone_dir, 0.);\n"
      "  world_matrix[1] = \n"
      "    vec4(bone_len * normalize(cross(binormal, bone_dir)), 0.);\n"
      "  world_matrix[2] =\n"
      "    vec4(bone_len * normalize(cross(bone_dir, world_matrix[1].xyz)), "
      "0.);\n"
      "  world_matrix[3] = vec4(joint[3].xyz, 1.);\n"
      "  return u_model * world_matrix;\n"
      "}\n";
  const char* vs[] = {kPlatformSpecificVSHeader, kPassNoUv,
                        "in mat4 joint;\n"
                         ,
                      vs_joint_to_world_matrix, kShaderUberVS};
  const char* fs[] = {kPlatformSpecificFSHeader, kShaderAmbientFct,
                      kShaderAmbientFS};

  ozz::unique_ptr<BoneShader> shader = make_unique<BoneShader>();
  success &=
      shader->BuildFromSource(OZZ_ARRAY_SIZE(vs), vs, OZZ_ARRAY_SIZE(fs), fs);

  // Binds default attributes
  success &= shader->FindAttrib("a_position");
  success &= shader->FindAttrib("a_normal");
  success &= shader->FindAttrib("a_color");

  // Binds default uniforms
  success &= shader->BindUniform("u_model");
  success &= shader->BindUniform("u_viewproj");
  success &= shader->FindAttrib("joint");

  if (!success) {
    shader.reset();
  }

  return shader;
}

ozz::unique_ptr<AmbientShader> AmbientShader::Build() {
  const char* vs[] = {kPlatformSpecificVSHeader, kPassNoUv,
                      "uniform mat4 u_model;\n"
                      "mat4 GetWorldMatrix() {return u_model;}\n",
                      kShaderUberVS};
  const char* fs[] = {kPlatformSpecificFSHeader, kShaderAmbientFct,
                      kShaderAmbientFS};

  ozz::unique_ptr<AmbientShader> shader = make_unique<AmbientShader>();
  bool success =
      shader->InternalBuild(OZZ_ARRAY_SIZE(vs), vs, OZZ_ARRAY_SIZE(fs), fs);

  if (!success) {
    shader.reset();
  }

  return shader;
}

bool AmbientShader::InternalBuild(int _vertex_count, const char** _vertex,
                                  int _fragment_count, const char** _fragment) {
  bool success = true;

  success &=
      BuildFromSource(_vertex_count, _vertex, _fragment_count, _fragment);

  // Binds default attributes
  success &= FindAttrib("a_position");
  success &= FindAttrib("a_normal");
  success &= FindAttrib("a_color");

  // Binds default uniforms
  success &= BindUniform("u_model");
  success &= BindUniform("u_viewproj");

  return success;
}

void AmbientShader::Bind(const math::Float4x4& _model,
                         const math::Float4x4& _view_proj, GLsizei _pos_stride,
                         GLsizei _pos_offset, GLsizei _normal_stride,
                         GLsizei _normal_offset, GLsizei _color_stride,
                         GLsizei _color_offset, bool _color_float) {
  glUseProgram(program());

  const GLint position_attrib = attrib(0);
  glEnableVertexAttribArray(position_attrib);
  glVertexAttribPointer(position_attrib, 3, GL_FLOAT, GL_FALSE, _pos_stride,
                         GL_PTR_OFFSET(_pos_offset));

  const GLint normal_attrib = attrib(1);
  glEnableVertexAttribArray(normal_attrib);
    glVertexAttribPointer(normal_attrib, 3, GL_FLOAT, GL_FALSE, _normal_stride,
                         GL_PTR_OFFSET(_normal_offset));

  const GLint color_attrib = attrib(2);
  glEnableVertexAttribArray(color_attrib);
 glVertexAttribPointer(
      color_attrib, 4, _color_float ? GL_FLOAT : GL_UNSIGNED_BYTE,
      !_color_float, _color_stride, GL_PTR_OFFSET(_color_offset));

  // Binds mw uniform
  glUniformMat4(_model, uniform(0));

  // Binds mvp uniform
  glUniformMat4(_view_proj, uniform(1));
}

ozz::unique_ptr<AmbientShaderInstanced> AmbientShaderInstanced::Build() {
  bool success = true;

  const char* vs[] = {kPlatformSpecificVSHeader, kPassNoUv,
                      "in mat4 a_m;\n mat4 GetWorldMatrix() {return a_m;}\n",
                      kShaderUberVS};
  const char* fs[] = {kPlatformSpecificFSHeader, kShaderAmbientFct,
                      kShaderAmbientFS};

  ozz::unique_ptr<AmbientShaderInstanced> shader =
      make_unique<AmbientShaderInstanced>();
  success &=
      shader->BuildFromSource(OZZ_ARRAY_SIZE(vs), vs, OZZ_ARRAY_SIZE(fs), fs);

  // Binds default attributes
  success &= shader->FindAttrib("a_position");
  success &= shader->FindAttrib("a_normal");
  success &= shader->FindAttrib("a_color");
  success &= shader->FindAttrib("a_m");

  // Binds default uniforms
  success &= shader->BindUniform("u_viewproj");

  if (!success) {
    shader.reset();
  }

  return shader;
}

void AmbientShaderInstanced::Bind(GLsizei _models_offset,
                                  const math::Float4x4& _view_proj,
                                  GLsizei _pos_stride, GLsizei _pos_offset,
                                  GLsizei _normal_stride,
                                  GLsizei _normal_offset, GLsizei _color_stride,
                                  GLsizei _color_offset, bool _color_float) {
  glUseProgram(program());

  const GLint position_attrib = attrib(0);
  glEnableVertexAttribArray(position_attrib);
 glVertexAttribPointer(position_attrib, 3, GL_FLOAT, GL_FALSE, _pos_stride,
                         GL_PTR_OFFSET(_pos_offset));

  const GLint normal_attrib = attrib(1);
  glEnableVertexAttribArray(normal_attrib);
 glVertexAttribPointer(normal_attrib, 3, GL_FLOAT, GL_FALSE, _normal_stride,
                         GL_PTR_OFFSET(_normal_offset));

  const GLint color_attrib = attrib(2);
  glEnableVertexAttribArray(color_attrib);
 glVertexAttribPointer(
      color_attrib, 4, _color_float ? GL_FLOAT : GL_UNSIGNED_BYTE,
      !_color_float, _color_stride, GL_PTR_OFFSET(_color_offset));
  if (_color_stride == 0) {
    glVertexAttribDivisor(color_attrib, 0xffffffff);
  }

  // Binds mw uniform
  const GLint models_attrib = attrib(3);
  glEnableVertexAttribArray(models_attrib + 0);
  glEnableVertexAttribArray(models_attrib + 1);
  glEnableVertexAttribArray(models_attrib + 2);
  glEnableVertexAttribArray(models_attrib + 3);
  glVertexAttribDivisor(models_attrib + 0, 1);
  glVertexAttribDivisor(models_attrib + 1, 1);
  glVertexAttribDivisor(models_attrib + 2, 1);
  glVertexAttribDivisor(models_attrib + 3, 1);
 glVertexAttribPointer(models_attrib + 0, 4, GL_FLOAT, GL_FALSE,
                         sizeof(math::Float4x4),
                         GL_PTR_OFFSET(0 + _models_offset));
 glVertexAttribPointer(models_attrib + 1, 4, GL_FLOAT, GL_FALSE,
                         sizeof(math::Float4x4),
                         GL_PTR_OFFSET(16 + _models_offset));
 glVertexAttribPointer(models_attrib + 2, 4, GL_FLOAT, GL_FALSE,
                         sizeof(math::Float4x4),
                         GL_PTR_OFFSET(32 + _models_offset));
 glVertexAttribPointer(models_attrib + 3, 4, GL_FLOAT, GL_FALSE,
                         sizeof(math::Float4x4),
                         GL_PTR_OFFSET(48 + _models_offset));

  // Binds mvp uniform
  glUniformMat4(_view_proj, uniform(0));
}

void AmbientShaderInstanced::Unbind() {
  const GLint color_attrib = attrib(2);
  glVertexAttribDivisor(color_attrib, 0);

  const GLint models_attrib = attrib(3);
  glDisableVertexAttribArray(models_attrib + 0);
  glDisableVertexAttribArray(models_attrib + 1);
  glDisableVertexAttribArray(models_attrib + 2);
  glDisableVertexAttribArray(models_attrib + 3);
  glVertexAttribDivisor(models_attrib + 0, 0);
  glVertexAttribDivisor(models_attrib + 1, 0);
  glVertexAttribDivisor(models_attrib + 2, 0);
  glVertexAttribDivisor(models_attrib + 3, 0);
  Shader::Unbind();
}

ozz::unique_ptr<AmbientTexturedShader> AmbientTexturedShader::Build() {
  const char* vs[] = {
      kPlatformSpecificVSHeader, kPassUv,
      "uniform mat4 u_model;\n mat4 GetWorldMatrix() {return u_model;}\n",
      kShaderUberVS};
  const char* fs[] = {kPlatformSpecificFSHeader, kShaderAmbientFct,
                      kShaderAmbientTexturedFS};

  ozz::unique_ptr<AmbientTexturedShader> shader =
      make_unique<AmbientTexturedShader>();
  bool success =
      shader->InternalBuild(OZZ_ARRAY_SIZE(vs), vs, OZZ_ARRAY_SIZE(fs), fs);

  success &= shader->FindAttrib("a_uv");

  if (!success) {
    shader.reset();
  }

  return shader;
}

void AmbientTexturedShader::Bind(const math::Float4x4& _model,
                                 const math::Float4x4& _view_proj,
                                 GLsizei _pos_stride, GLsizei _pos_offset,
                                 GLsizei _normal_stride, GLsizei _normal_offset,
                                 GLsizei _color_stride, GLsizei _color_offset,
                                 bool _color_float, GLsizei _uv_stride,
                                 GLsizei _uv_offset) {
  AmbientShader::Bind(_model, _view_proj, _pos_stride, _pos_offset,
                      _normal_stride, _normal_offset, _color_stride,
                      _color_offset, _color_float);

  const GLint uv_attrib = attrib(3);
  glEnableVertexAttribArray(uv_attrib);
 glVertexAttribPointer(uv_attrib, 2, GL_FLOAT, GL_FALSE, _uv_stride,
                         GL_PTR_OFFSET(_uv_offset));
}

        // 在 src/internal/shader.cc 文件中，找到 VtfSkinnedShader::Build 函数并替换

        // 在 src/internal/shader.cc 文件中，找到 VtfSkinnedShader::Build 函数并替换

        ozz::unique_ptr<VtfSkinnedShader> VtfSkinnedShader::Build() {
            // -------------------------------------------------------------------------
            // 实例化 + 2D VTF 顶点着色器 (Vertex Shader)
            // -------------------------------------------------------------------------
            const char* kSkinnedVS = R"(
// Per-vertex attributes from Mesh VBO
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec4 a_color;
layout(location = 3) in uvec4 a_joint_indices;
layout(location = 4) in vec4 a_joint_weights;

// Per-instance attributes from Instance VBO
// a_world_matrix 定义了每个实例在世界中的位置、旋转和缩放
layout(location = 5) in mat4 a_world_matrix;

// Uniforms
uniform mat4 u_viewproj;
// 2D 纹理，存储所有实例的所有骨骼的蒙皮矩阵
uniform sampler2D u_skinning_texture;
// 2D 纹理的宽度，用于将一维逻辑索引转换为二维物理坐标
uniform int u_skinning_texture_width;
// 每个实例（角色）有多少根骨骼（用于计算在纹理中的偏移）
uniform int u_num_joints;

// Outputs to Fragment Shader
out vec3 v_world_normal;
out vec4 v_vertex_color;

// 从2D数据纹理中获取一个完整的 mat4 矩阵
mat4 fetch_matrix_2d(int logical_index) {
    // 一个 mat4 需要 4 个 RGBA (vec4) 像素来存储
    int base_pixel_index = logical_index * 4;

    // 将一维逻辑索引转换为二维物理坐标
    int u = base_pixel_index % u_skinning_texture_width;
    int v = base_pixel_index / u_skinning_texture_width;

    // 从2D纹理中拾取组成矩阵的4个像素
    vec4 c0 = texelFetch(u_skinning_texture, ivec2(u + 0, v), 0);
    vec4 c1 = texelFetch(u_skinning_texture, ivec2(u + 1, v), 0);
    vec4 c2 = texelFetch(u_skinning_texture, ivec2(u + 2, v), 0);
    vec4 c3 = texelFetch(u_skinning_texture, ivec2(u + 3, v), 0);
    return mat4(c0, c1, c2, c3);
}

void main() {
    mat4 skinning_transform = mat4(0.0);

    // 计算当前实例在巨大纹理数据中的骨骼矩阵起始索引
    int instance_joint_offset = gl_InstanceID * u_num_joints;

    // 根据骨骼索引和权重，从纹理中获取矩阵并进行线性混合
    if (a_joint_weights.x > 0.0) {
        skinning_transform += fetch_matrix_2d(instance_joint_offset + int(a_joint_indices.x)) * a_joint_weights.x;
    }
    if (a_joint_weights.y > 0.0) {
        skinning_transform += fetch_matrix_2d(instance_joint_offset + int(a_joint_indices.y)) * a_joint_weights.y;
    }
    if (a_joint_weights.z > 0.0) {
        skinning_transform += fetch_matrix_2d(instance_joint_offset + int(a_joint_indices.z)) * a_joint_weights.z;
    }
    if (a_joint_weights.w > 0.0) {
        skinning_transform += fetch_matrix_2d(instance_joint_offset + int(a_joint_indices.w)) * a_joint_weights.w;
    }

    // 应用蒙皮变换，然后应用实例独有的世界变换
    vec4 world_pos = a_world_matrix * skinning_transform * vec4(a_position, 1.0);

    // 正确地变换法线
    mat3 normal_matrix = transpose(inverse(mat3(a_world_matrix) * mat3(skinning_transform)));
    v_world_normal = normal_matrix * a_normal;

    gl_Position = u_viewproj * world_pos;
    v_vertex_color = a_color;
}
)";

            // 片元着色器保持不变
            const char* kAmbientFS = R"(
in vec3 v_world_normal;
in vec4 v_vertex_color;
out vec4 o_color;
vec4 GetAmbient(vec3 _world_normal) {
  vec3 n = normalize(_world_normal);
  vec3 a = (n + 1.) * .5;
  vec2 bt = mix(vec2(.3, .7), vec2(.4, .8), a.xz);
  vec3 ambient = mix(vec3(bt.x, .3, bt.x), vec3(bt.y, .8, bt.y), a.y);
  return vec4(ambient, 1.);
}
void main() {
    o_color = GetAmbient(v_world_normal) * v_vertex_color;
}
)";

            auto shader = make_unique<VtfSkinnedShader>();
            const char* vs_sources[] = { kPlatformSpecificVSHeader, kSkinnedVS };
            const char* fs_sources[] = { kPlatformSpecificFSHeader, kAmbientFS };
            bool success = shader->BuildFromSource(OZZ_ARRAY_SIZE(vs_sources), vs_sources, OZZ_ARRAY_SIZE(fs_sources), fs_sources);

            if (success) {
                // 绑定所有新的 uniforms
                success &= shader->BindUniform("u_viewproj");
                success &= shader->BindUniform("u_skinning_texture");
                success &= shader->BindUniform("u_skinning_texture_width");
                success &= shader->BindUniform("u_num_joints");
            }

            if (!success) {
                shader.reset();
            }
            return shader;
        }

        void VtfSkinnedShader::Bind(const math::Float4x4& _view_proj, int _texture_unit, int _texture_width, int _num_joints) {
            glUseProgram(program());

            // 设置 u_viewproj
            float view_proj_matrix[16];
            ozz::math::StorePtrU(_view_proj.cols[0], view_proj_matrix + 0);
            ozz::math::StorePtrU(_view_proj.cols[1], view_proj_matrix + 4);
            ozz::math::StorePtrU(_view_proj.cols[2], view_proj_matrix + 8);
            ozz::math::StorePtrU(_view_proj.cols[3], view_proj_matrix + 12);
            glUniformMatrix4fv(uniform(0), 1, false, view_proj_matrix);

            // 告诉shader去哪个纹理单元读取数据
            glUniform1i(uniform(1), _texture_unit);
            // 传递2D纹理的宽度
            glUniform1i(uniform(2), _texture_width);
            // 传递骨骼数量
            glUniform1i(uniform(3), _num_joints);
        }

}  // namespace internal
}  // namespace sample
}  // namespace ozz
