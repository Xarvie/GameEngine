#ifndef OZZ_SAMPLES_FRAMEWORK_INTERNAL_CAMERA_H_
#define OZZ_SAMPLES_FRAMEWORK_INTERNAL_CAMERA_H_

#ifndef OZZ_INCLUDE_PRIVATE_HEADER
#error "This header is private, it cannot be included from public headers."
#endif  // OZZ_INCLUDE_PRIVATE_HEADER

#include "ozz/base/maths/simd_math.h"
#include "ozz/base/maths/vec_float.h"
#include "framework/application.h"

namespace ozz {
namespace math {
struct Box;
}
namespace sample {
class ImGui;
namespace internal {

class Camera {
 public:
  Camera();
  ~Camera();

  void Update(const Application::InputState& _input, const math::Box& _box,
              float _delta_time, bool _first_frame);
  void Update(const Application::InputState& _input,
              const math::Float4x4& _transform, const math::Box& _box,
              float _delta_time, bool _first_frame);
  void Reset(const math::Float3& _center, const math::Float2& _angles,
             float _distance);
  void OnGui(ImGui* _im_gui);
  void Bind3D();
  void Bind2D();
  void Resize(int _width, int _height);

  const math::Float4x4& projection() const { return projection_; }
  const math::Float4x4& view() const { return view_; }
  const math::Float4x4& view_proj() const { return view_proj_; }
  void set_auto_framing(bool _auto) { auto_framing_ = _auto; }
  bool auto_framing() const { return auto_framing_; }

 private:
  struct Controls {
    bool zooming;
    bool rotating;
    bool panning;
  };
  Controls UpdateControls(const Application::InputState& _input, float _delta_time);

  math::Float4x4 projection_;
  math::Float4x4 projection_2d_;
  math::Float4x4 view_;
  math::Float4x4 view_proj_;
  math::Float2 angles_;
  math::Float3 center_;
  float distance_;
  int mouse_last_x_;
  int mouse_last_y_;
  bool auto_framing_;
};
}
}
}
#endif
