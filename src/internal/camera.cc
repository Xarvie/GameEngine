// 完整替换 src/internal/camera.cc

#define OZZ_INCLUDE_PRIVATE_HEADER

#include "internal/camera.h"
#include <SDL2/SDL.h>
#include <cmath>
#include "imgui.h"
#include "ozz/base/log.h"
#include "ozz/base/maths/box.h"
#include "ozz/base/maths/math_constant.h"
#include "ozz/base/maths/math_ex.h"
#include "ozz/base/platform.h"
#include "internal/renderer_impl.h"

using ozz::math::Float2;
using ozz::math::Float3;
using ozz::math::Float4x4;

namespace ozz {
    namespace sample {
        namespace internal {

            const float kDefaultDistance = 8.f;
            const Float3 kDefaultCenter = Float3(0.f, .5f, 0.f);
            const Float2 kDefaultAngle = Float2(-ozz::math::kPi * 1.f / 12.f, ozz::math::kPi * 1.f / 5.f);
            const float kAngleFactor = .005f;
            const float kDistanceFactor = .1f;
            const float kScrollFactor = .03f;
            const float kPanFactor = .005f;
            const float kKeyboardFactor = 100.f;
            const float kNear = .2f;
            const float kFar = 200.f;
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
                        ozz::math::Store3PtrU(-ozz::math::Normalize3(_transform.cols[2]), &camera_dir.x);
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

            Camera::Controls Camera::UpdateControls(const Application::InputState& _input, float _delta_time) {
                Controls controls{};

                float dx = 0.f;
                float dy = 0.f;
                float zoom_delta = 0.f;
                float pan_dx = 0.f;
                float pan_dy = 0.f;

                // --- 1. 鼠标事件处理 ---
                int x = _input.mouse_x;
                int y = _input.mouse_y;

                if (_input.mouse_buttons & SDL_BUTTON_RMASK) {
                    dx += x - mouse_last_x_;
                    dy += y - mouse_last_y_;
                    controls.rotating = true;
                }
                if (_input.mouse_buttons & SDL_BUTTON_MMASK) {
                    pan_dx += (x - mouse_last_x_);
                    pan_dy += (y - mouse_last_y_);
                    controls.panning = true;
                }
                if (_input.mouse_wheel != 0) {
                    zoom_delta += _input.mouse_wheel;
                    controls.zooming = true;
                }

                mouse_last_x_ = x;
                mouse_last_y_ = y;

                // --- 2. 键盘事件处理 ---
                const float keyboard_pan_speed = kPanFactor * 500.f * _delta_time;
                const float keyboard_zoom_speed = kDistanceFactor * 40.f * _delta_time;
                const float keyboard_rotate_speed = kAngleFactor * 100.f;

                if (_input.keyboard[SDL_SCANCODE_W]) { zoom_delta += keyboard_zoom_speed; controls.zooming = true; }
                if (_input.keyboard[SDL_SCANCODE_S]) { zoom_delta -= keyboard_zoom_speed; controls.zooming = true; }
                if (_input.keyboard[SDL_SCANCODE_A]) { pan_dx += keyboard_pan_speed; controls.panning = true; }
                if (_input.keyboard[SDL_SCANCODE_D]) { pan_dx -= keyboard_pan_speed; controls.panning = true; }
                if (_input.keyboard[SDL_SCANCODE_UP]) { dy -= keyboard_rotate_speed; controls.rotating = true; }
                if (_input.keyboard[SDL_SCANCODE_DOWN]) { dy += keyboard_rotate_speed; controls.rotating = true; }
                if (_input.keyboard[SDL_SCANCODE_LEFT]) { dx -= keyboard_rotate_speed; controls.rotating = true; }
                if (_input.keyboard[SDL_SCANCODE_RIGHT]) { dx += keyboard_rotate_speed; controls.rotating = true; }

                // --- 3. 触摸手势处理 ---
                if (_input.num_touch_fingers == 1) {
                    const auto& finger = _input.fingers[0];
                    dx += finger.x - finger.last_x;
                    dy += finger.y - finger.last_y;
                    controls.rotating = true;
                } else if (_input.num_touch_fingers == 2) {
                    const auto& f0 = _input.fingers[0];
                    const auto& f1 = _input.fingers[1];

                    float last_dist = std::sqrt(std::pow(f0.last_x - f1.last_x, 2.f) + std::pow(f0.last_y - f1.last_y, 2.f));
                    float current_dist = std::sqrt(std::pow(f0.x - f1.x, 2.f) + std::pow(f0.y - f1.y, 2.f));
                    if (last_dist > 1.f && current_dist > 1.f) {
                        zoom_delta += (last_dist - current_dist) * kScrollFactor * 0.5f;
                        controls.zooming = true;
                    }

                    float last_center_x = (f0.last_x + f1.last_x) * 0.5f;
                    float last_center_y = (f0.last_y + f1.last_y) * 0.5f;
                    float current_center_x = (f0.x + f1.x) * 0.5f;
                    float current_center_y = (f0.y + f1.y) * 0.5f;
                    pan_dx += current_center_x - last_center_x;
                    pan_dy += current_center_y - last_center_y;
                    controls.panning = true;
                }

                // --- 4. 统一应用所有增量 ---
                if (std::abs(dx) > 0.f || std::abs(dy) > 0.f) {
                    angles_.y -= dx * kAngleFactor;
                    angles_.x -= dy * kAngleFactor;
                }
                if (std::abs(zoom_delta) > 0.f) {
                    distance_ *= (1.f - zoom_delta * kScrollFactor);
                }
                if (std::abs(pan_dx) > 0.f || std::abs(pan_dy) > 0.f) {
                    math::Float4x4 transpose = Transpose(view_);
                    math::Float3 right_vec, up_vec;
                    math::Store3PtrU(transpose.cols[0], &right_vec.x);
                    math::Store3PtrU(transpose.cols[1], &up_vec.x);
                    center_ = center_ + right_vec * (-pan_dx * kPanFactor) + up_vec * (pan_dy * kPanFactor);
                }

                // --- 5. 更新状态和矩阵 ---
                distance_ = ozz::math::Max(0.1f, distance_);
                angles_.x = ozz::math::Clamp(-ozz::math::kPi / 2.01f, angles_.x, ozz::math::kPi / 2.01f);

                const Float4x4 center_trans = Float4x4::Translation(math::simd_float4::Load(center_.x, center_.y, center_.z, 1.f));
                const Float4x4 y_rot = Float4x4::FromAxisAngle(math::simd_float4::y_axis(), math::simd_float4::Load1(angles_.y));
                const Float4x4 x_rot = Float4x4::FromAxisAngle(math::simd_float4::x_axis(), math::simd_float4::Load1(angles_.x));
                const Float4x4 dist_trans = Float4x4::Translation(math::simd_float4::Load(0.f, 0.f, distance_, 1.f));
                view_ = Invert(center_trans * y_rot * x_rot * dist_trans);

                controls.panning |= controls.zooming || controls.rotating;
                return controls;
            }

            void Camera::Reset(const math::Float3& _center, const math::Float2& _angles, float _distance) {
                center_ = _center;
                angles_ = _angles;
                distance_ = _distance;
            }
            void Camera::OnGui(ImGui* _im_gui) {
                const char* controls_label =
                        "- MOUSE RMB: Rotate\n"
                        "- MOUSE MMB: Pan\n"
                        "- MOUSE Wheel: Zoom\n"
                        "- KEYB WASD: Pan/Zoom\n"
                        "- KEYB Arrows: Rotate\n";
                _im_gui->DoLabel(controls_label, ImGui::kLeft, false);
                _im_gui->DoCheckBox("Automatic framing", &auto_framing_);
            }
            void Camera::Bind3D() { view_proj_ = projection_ * view_; }
            void Camera::Bind2D() { view_proj_ = projection_2d_; }
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
                projection_.cols[2] = math::simd_float4::Load(0.f, 0.f, -(kFar + kNear) / (kFar - kNear), -1.f);
                projection_.cols[3] = math::simd_float4::Load(0.f, 0.f, -(2.f * kFar * kNear) / (kFar - kNear), 0.f);
                projection_2d_.cols[0] = math::simd_float4::Load( 2.f / _width,  0.f,            0.f, 0.f);
                projection_2d_.cols[1] = math::simd_float4::Load( 0.f,           2.f / _height,  0.f, 0.f); // Y轴不再翻转
                projection_2d_.cols[2] = math::simd_float4::Load( 0.f,           0.f,           -1.f, 0.f);
                projection_2d_.cols[3] = math::simd_float4::Load(-1.f,          -1.f,           0.f, 1.f); // 将(0,0)平移到(-1,-1)
            }
        } // namespace internal
    } // namespace sample
} // namespace ozz
