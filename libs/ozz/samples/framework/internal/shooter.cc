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

#include "shooter.h"

#include <cassert>
#include <cstdio>
#include <vector> // 为 WebGL 路径添加 vector 头文件

#include "renderer_impl.h"

namespace ozz {
namespace sample {
namespace internal {

Shooter::Shooter()
    : gl_shot_format_(GL_RGBA),  // Default fail safe format and types.
      image_format_(image::Format::kRGBA),
      shot_number_(0) {
  // 在现代的 GLES3/GL4.1/WebGL2 环境中，我们依赖 GLAD 来加载正确的函数。
  // glMapBufferRange (GLES3/GL4) 或 GetBufferSubData (WebGL2) 是否可用，
  // 将在调用时由函数指针是否为 nullptr 来决定。
  // 因此，不再需要基于旧 glMapBuffer 的检查。
  supported_ = true;

  // Initializes shots
  GLuint pbos[kNumShots];
  glGenBuffers(kNumShots, pbos);
  for (int i = 0; i < kNumShots; ++i) {
    Shot& shot = shots_[i];
    shot.pbo = pbos[i];
  }

  // OpenGL ES2 compatibility extension allows to query for implementation best
  // format and type.
  // 这段代码在你所有目标平台上都是有效的，因为它们都支持此查询（尽管名称可能已去OES后缀）
  // GLAD 会处理好这些细节。
  {
    GLint format;
    glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &format);
    GLint type;
    glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &type);

    // Only support GL_UNSIGNED_BYTE.
    if (type == GL_UNSIGNED_BYTE) {
      switch (format) {
        case GL_RGBA:
          gl_shot_format_ = format;
          image_format_ = image::Format::kRGBA;
          break;
        case GL_RGB:
          gl_shot_format_ = format;
          image_format_ = image::Format::kRGB;
          break;
      }
    }
  }
}

Shooter::~Shooter() {
  // Process all remaining shots.
  ProcessAll();

  // Clean shot pbos.
  for (int i = 0; i < kNumShots; ++i) {
    Shot& shot = shots_[i];
    glDeleteBuffers(1, &shot.pbo);

    assert(shot.cooldown == 0);  // Must have been processed.
  }
}

void Shooter::Resize(int _width, int _height) {
  // Early out if not supported.
  if (!supported_) {
    return;
  }

  // Process all remaining shots.
  ProcessAll();

  // Resizes all pbos.
  // Emscripten/WebGL 路径下，BufferData 通常在需要时（如ReadPixels前）动态处理，
  // 或者使用固定大小的缓冲区池，因此这里的逻辑保持不变是合理的。
  for (int i = 0; i < kNumShots; ++i) {
    Shot& shot = shots_[i];
    shot.width = _width;
    shot.height = _height;
    // PBO 的大小在 Capture 中根据实际读取尺寸确定，这里可以不着急分配
  }
}

bool Shooter::Update() { return Process(); }

bool Shooter::Process() {
  // Early out if not supported.
  if (!supported_) {
    return true;
  }

  // Early out if process stack is empty.
  for (int i = 0; i < kNumShots; ++i) {
    Shot& shot = shots_[i];

    // Early out for already processed, or empty shots.
    if (shot.cooldown == 0) {
      continue;
    }

    // Skip shots that didn't reached the cooldown.
    if (--shot.cooldown != 0) {
      continue;
    }

    // A shot needs to be processed
    const int bpp = image_format_ == image::Format::kRGBA ? 4 : 3;
    const GLsizeiptr buffer_size = shot.width * shot.height * bpp;

    if (buffer_size == 0) {
      continue;
    }

#if defined(__EMSCRIPTEN__) // WebGL 2.0 路径
    // WebGL出于安全原因，不支持直接内存映射(glMapBufferRange)。
    // 必须使用 glGetBufferSubData 将数据从PBO复制到CPU内存。
    glBindBuffer(GL_PIXEL_PACK_BUFFER, shot.pbo);
    std::vector<uint8_t> pixels(buffer_size);
    glGetBufferSubData(GL_PIXEL_PACK_BUFFER, 0, buffer_size, pixels.data());

    char filename[16];
    std::snprintf(filename, sizeof(filename), "%06d.tga", shot_number_++);

    // 注意：在浏览器中直接写文件是受限的。
    // 这需要 Emscripten 的文件系统支持(如MEMFS)或自定义的下载逻辑。
    ozz::sample::image::WriteTGA(
        filename, shot.width, shot.height, image_format_,
        pixels.data(), false);

    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

#else   // OpenGL ES 3.0 / OpenGL 4.1 路径
    glBindBuffer(GL_PIXEL_PACK_BUFFER, shot.pbo);
    // 使用现代的、跨平台的 glMapBufferRange
    const void* pixels = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, buffer_size, GL_MAP_READ_BIT);
    if (pixels) {
      char filename[16];
      std::snprintf(filename, sizeof(filename), "%06d.tga", shot_number_++);

      ozz::sample::image::WriteTGA(
          filename, shot.width, shot.height, image_format_,
          reinterpret_cast<const uint8_t*>(pixels), false);
      
      // 完成后必须解除映射
      glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
#endif  // EMSCRIPTEN
  }
  return true;
}

bool Shooter::ProcessAll() {
  // Reset cooldown to 1 for all "unprocessed" shots, so they will be
  // "processed".
  for (int i = 0; i < kNumShots; ++i) {
    Shot& shot = shots_[i];
    shot.cooldown = shot.cooldown > 0 ? 1 : 0;
  }

  return Process();
}

bool Shooter::Capture(int _buffer) {
  assert(_buffer == GL_FRONT || _buffer == GL_BACK);

  // Early out if not supported.
  if (!supported_) {
    return true;
  }

  // Finds the shot to use for this capture.
  Shot* shot;
  for (shot = shots_; shot < shots_ + kNumShots && shot->cooldown != 0;
       ++shot) {
  }
  
  if (shot == shots_ + kNumShots) {
    // No available slot, skip capture
    return true;
  }

  // Initializes cooldown.
  shot->cooldown = kInitialCountDown;

  // 为所有平台启用像素捕获逻辑
  // Copy pixels to shot's pbo.
  glReadBuffer(_buffer);
  glBindBuffer(GL_PIXEL_PACK_BUFFER, shot->pbo);

  // 重新分配PBO大小以匹配当前捕获尺寸
  const int bpp = image_format_ == image::Format::kRGBA ? 4 : 3;
  const GLsizeiptr buffer_size = shot->width * shot->height * bpp;
  glBufferData(GL_PIXEL_PACK_BUFFER, buffer_size, nullptr, GL_STREAM_READ);

  glPixelStorei(GL_PACK_ALIGNMENT, 4);
  // glReadPixels 的最后一个参数是PBO内的偏移量，对于新PBO应为0
  GL(ReadPixels(0, 0, shot->width, shot->height, gl_shot_format_,
                GL_UNSIGNED_BYTE, 0));
  glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

  return true;
}
}  // namespace internal
}  // namespace sample
}  // namespace ozz