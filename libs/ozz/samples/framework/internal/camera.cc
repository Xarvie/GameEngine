#define OZZ_INCLUDE_PRIVATE_HEADER

#include "framework/internal/camera.h"

#include <SDL2/SDL.h>

#include "framework/imgui.h"
#include "ozz/base/log.h"
#include "ozz/base/maths/box.h"
#include "ozz/base/maths/math_constant.h"
#include "ozz/base/maths/math_ex.h"
#include "ozz/base/platform.h"
#include "framework/internal/renderer_impl.h"

using ozz::math::Float2;
using ozz::math::Float3;
using ozz::math::Float4x4;

namespace ozz {
namespace sample {
namespace internal {

const float kDefaultDistance = 8.f;
const Float3 kDefaultCenter = Float3(0.f, .5f, 0.f);
const Float2 kDefaultAngle =
    Float2(-ozz::math::kPi * 1.f / 12.f, ozz::math::kPi * 1.f / 5.f);
const float kAngleFactor = .01f;
const float kDistanceFactor = .1f;
const float kScrollFactor = .03f;
const float kPanFactor = .05f;
const float kKeyboardFactor = 100.f;
const float kNear = .01f;
const float kFar = 1000.f;
const float kFovY = ozz::math::kPi / 3.f;
const float kFrameAllZoomOut = 1.3f;

Camera::Camera()
    : projection_(Float4x4::identity()),
      projection_2d_(Float4x4::identity()),
      view_(Float4x4::identity()),
      view_proj_(Float4x4::identity()),
      angles_(kDefaultAngle),
      center_(kDefaultCenter),
      distance_(kDefaultDistance),
      mouse_last_x_(0),
      mouse_last_y_(0),
      auto_framing_(true) {}

Camera::~Camera() {}

void Camera::Update(const Application::InputState& _input, const math::Box& _box,
                    float _delta_time, bool _first_frame) {
  if (_box.is_valid()) {
    if (auto_framing_ || _first_frame) {
      center_ = (_box.max + _box.min) * .5f;
      if (_first_frame) {
        const float radius = Length(_box.max - _box.min) * .5f;
        distance_ = radius * kFrameAllZoomOut / tanf(kFovY * .5f);
      }
    }
  }
  const Controls controls = UpdateControls(_input, _delta_time);
  auto_framing_ &= !controls.panning;
}

void Camera::Update(const Application::InputState& _input,
                    const math::Float4x4& _transform, const math::Box& _box,
                    float _delta_time, bool _first_frame) {
  if (_box.is_valid()) {
    if (auto_framing_ || _first_frame) {
      ozz::math::Float3 camera_dir;
      ozz::math::Store3PtrU(-ozz::math::Normalize3(_transform.cols[2]),
                            &camera_dir.x);
      ozz::math::Float3 camera_pos;
      ozz::math::Store3PtrU(_transform.cols[3], &camera_pos.x);

      const ozz::math::Float3 box_center_ = (_box.max + _box.min) * .5f;
      distance_ = Length(box_center_ - camera_pos);
      center_ = camera_pos + camera_dir * distance_;
      angles_.x = asinf(camera_dir.y);
      angles_.y = atan2(-camera_dir.x, -camera_dir.z);
    }
  }
  const Controls controls = UpdateControls(_input, _delta_time);
  auto_framing_ &= !controls.panning;

  if (auto_framing_) {
    view_ = Invert(_transform);
  }
}

Camera::Controls Camera::UpdateControls(const Application::InputState& _input,
                                        float _delta_time) {
  Controls controls{};

  if (_input.keyboard[SDL_SCANCODE_LSHIFT]) {
    const int dw = _input.mouse_wheel;
    if (dw != 0) {
      controls.zooming = true;
      distance_ *= 1.f + -dw * kScrollFactor;
    }
  }

  int x = _input.mouse_x;
  int y = _input.mouse_y;
  const int mdx = x - mouse_last_x_;
  const int mdy = y - mouse_last_y_;
  mouse_last_x_ = x;
  mouse_last_y_ = y;

  const int timed_factor =
      ozz::math::Max(1, static_cast<int>(kKeyboardFactor * _delta_time));
  const int kdx =
      timed_factor * (_input.keyboard[SDL_SCANCODE_LEFT] -
                      _input.keyboard[SDL_SCANCODE_RIGHT]);
  const int kdy =
      timed_factor *
      (_input.keyboard[SDL_SCANCODE_DOWN] - _input.keyboard[SDL_SCANCODE_UP]);
  const bool keyboard_interact = kdx || kdy;

  const int dx = mdx + kdx;
  const int dy = mdy + kdy;

  if (keyboard_interact || (_input.mouse_buttons & SDL_BUTTON_RMASK)) {
    if (_input.keyboard[SDL_SCANCODE_LSHIFT]) {
      controls.zooming = true;
      distance_ += dy * kDistanceFactor;
    } else if (_input.keyboard[SDL_SCANCODE_LALT]) {
      controls.panning = true;

      const float dx_pan = -dx * kPanFactor;
      const float dy_pan = -dy * kPanFactor;

      math::Float4x4 transpose = Transpose(view_);
      math::Float3 right_transpose, up_transpose;
      math::Store3PtrU(transpose.cols[0], &right_transpose.x);
      math::Store3PtrU(transpose.cols[1], &up_transpose.x);
      center_ = center_ + right_transpose * dx_pan + up_transpose * dy_pan;
    } else {
      controls.rotating = true;
      angles_.x = fmodf(angles_.x - dy * kAngleFactor, ozz::math::k2Pi);
      angles_.y = fmodf(angles_.y - dx * kAngleFactor, ozz::math::k2Pi);
    }
  }

  const Float4x4 center = Float4x4::Translation(
      math::simd_float4::Load(center_.x, center_.y, center_.z, 1.f));
  const Float4x4 y_rotation = Float4x4::FromAxisAngle(
      math::simd_float4::y_axis(), math::simd_float4::Load1(angles_.y));
  const Float4x4 x_rotation = Float4x4::FromAxisAngle(
      math::simd_float4::x_axis(), math::simd_float4::Load1(angles_.x));
  const Float4x4 distance =
      Float4x4::Translation(math::simd_float4::Load(0.f, 0.f, distance_, 1.f));

  view_ = Invert(center * y_rotation * x_rotation * distance);

  // Any user interaction is considered as "panning" for auto-framing logic.
  controls.panning |= controls.zooming || controls.rotating;

  return controls;
}

void Camera::Reset(const math::Float3& _center, const math::Float2& _angles,
                   float _distance) {
  center_ = _center;
  angles_ = _angles;
  distance_ = _distance;
}

void Camera::OnGui(ImGui* _im_gui) {
  const char* controls_label =
      "-RMB: Rotate\n"
      "-Shift + Wheel: Zoom\n"
      "-Shift + RMB: Zoom\n"
      "-Alt + RMB: Pan\n";
  _im_gui->DoLabel(controls_label, ImGui::kLeft, false);

  _im_gui->DoCheckBox("Automatic", &auto_framing_);
}

void Camera::Bind3D() {
  view_proj_ = projection_ * view_;
}

void Camera::Bind2D() {
  view_proj_ = projection_2d_;
}

void Camera::Resize(int _width, int _height) {
  if (_width <= 0 || _height <= 0) {
    projection_ = ozz::math::Float4x4::identity();
    projection_2d_ = ozz::math::Float4x4::identity();
    return;
  }

  const float ratio = 1.f * _width / _height;
  const float h = tan(kFovY * .5f) * kNear;
  const float w = h * ratio;

  projection_.cols[0] = math::simd_float4::Load(kNear / w, 0.f, 0.f, 0.f);
  projection_.cols[1] = math::simd_float4::Load(0.f, kNear / h, 0.f, 0.f);
  projection_.cols[2] =
      math::simd_float4::Load(0.f, 0.f, -(kFar + kNear) / (kFar - kNear), -1.f);
  projection_.cols[3] = math::simd_float4::Load(
      0.f, 0.f, -(2.f * kFar * kNear) / (kFar - kNear), 0.f);

  projection_2d_.cols[0] = math::simd_float4::Load(2.f / _width, 0.f, 0.f, 0.f);
  projection_2d_.cols[1] =
      math::simd_float4::Load(0.f, -2.f / _height, 0.f,
                              0.f);
  projection_2d_.cols[2] = math::simd_float4::Load(0.f, 0.f, -1.f, 0.f);
  projection_2d_.cols[3] = math::simd_float4::Load(-1.f, 1.f, 0.f, 1.f);
}
}
}
}
