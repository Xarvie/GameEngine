#ifndef OZZ_SAMPLES_FRAMEWORK_APPLICATION_H_
#define OZZ_SAMPLES_FRAMEWORK_APPLICATION_H_

#include <cstddef>
#include <cstdint>

#include "ozz/base/containers/string.h"
#include "ozz/base/memory/unique_ptr.h"

struct SDL_Window;
typedef void* SDL_GLContext;
typedef long long SDL_TouchID;
typedef long long SDL_FingerID;


namespace ozz {
namespace math {
struct Box;
struct Float2;
struct Float3;
struct Float4x4;
}
namespace sample {

class ImGui;
class Renderer;
class Record;

namespace internal {
class ImGuiImpl;
class RendererImpl;
class Camera;
class Shooter;
}

struct Resolution {
  int width;
  int height;
};

class Application {
 public:
  Application();
  virtual ~Application();

  int Run(int _argc, const char** _argv, const char* _version,
          const char* _title);

  struct InputState {
    const uint8_t* keyboard;
    uint32_t mouse_buttons;
    int mouse_x;
    int mouse_y;
    int mouse_wheel;

    int num_touch_fingers;
    struct Finger {
      SDL_FingerID id;
      float x;
      float y;
      float dx;
      float dy;
    } fingers[2];
    bool touch_down;
    bool touch_up;
  };

 protected:
  math::Float2 WorldToScreen(const math::Float3& _world) const;

 private:
  virtual bool OnInitialize();
  virtual void OnDestroy();
  virtual bool OnUpdate(float _dt, float _time);
  virtual bool OnGui(ImGui* _im_gui);
  virtual bool OnFloatingGui(ImGui* _im_gui);
  virtual bool OnDisplay(Renderer* _renderer);
  virtual bool GetCameraInitialSetup(math::Float3* _center,
                                     math::Float2* _angles,
                                     float* _distance) const;
  virtual bool GetCameraOverride(math::Float4x4* _transform) const;
  virtual void GetSceneBounds(math::Box* _bound) const;

  bool Loop();

  enum LoopStatus {
    kContinue,
    kBreak,
    kBreakFailure,
  };
  LoopStatus OneLoop(const InputState& _input_state, int _loops);
  bool Idle(bool _first_frame);
  bool Display(const InputState& _input_state);
  bool Gui(const InputState& _input_state);
  bool FrameworkGui();
  void Resize(int _width, int _height);
  void ParseReadme();

  Application(const Application& _application);
  void operator=(const Application& _application);

  static Application* application_;

  SDL_Window* window_;
  SDL_GLContext gl_context_;

  bool exit_;
  bool freeze_;
  bool fix_update_rate;
  float fixed_update_rate;
  float time_factor_;
  float time_;
  double last_idle_time_;

  unique_ptr<internal::Camera> camera_;
  unique_ptr<internal::Shooter> shooter_;

  bool show_help_;
  bool vertical_sync_;
  int swap_interval_;
  bool show_grid_;
  bool show_axes_;
  bool capture_video_;
  bool capture_screenshot_;

  unique_ptr<internal::RendererImpl> renderer_;
  unique_ptr<internal::ImGuiImpl> im_gui_;
  unique_ptr<Record> fps_;
  unique_ptr<Record> update_time_;
  unique_ptr<Record> render_time_;
  Resolution resolution_;
  ozz::string help_;
};
}
}
#endif
