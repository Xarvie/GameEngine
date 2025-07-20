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
// FROM, OUT of OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        //
// DEALINGS IN THE SOFTWARE.                                                  //
//                                                                            //
//----------------------------------------------------------------------------//

#define OZZ_INCLUDE_PRIVATE_HEADER  // Allows to include private headers.

#include "framework/application.h"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <map>

#ifdef __APPLE__
#include <unistd.h>
#endif  // __APPLE__


#include "glad/glad.h"
#include <SDL2/SDL.h>

#include "framework/image.h"
#include "framework/internal/camera.h"
#include "framework/internal/imgui_impl.h"
#include "framework/internal/renderer_impl.h"
#include "framework/internal/shooter.h"
#include "framework/profile.h"
#include "framework/renderer.h"
#include "ozz/base/io/stream.h"
#include "ozz/base/log.h"
#include "ozz/base/maths/box.h"
#include "ozz/base/memory/allocator.h"
#include "ozz/options/options.h"

OZZ_OPTIONS_DECLARE_INT(
    max_idle_loops,
    "The maximum number of idle loops the sample application can perform."
    " Application automatically exit when this number of loops is reached."
    " A negative value disables this feature.",
    -1, false);

OZZ_OPTIONS_DECLARE_BOOL(render, "Enables sample redering.", true, false);

namespace {
// Screen resolution presets.
const ozz::sample::Resolution resolution_presets[] = {
    {640, 360},   {640, 480},  {800, 450},  {800, 600},   {1024, 576},
    {1024, 768},  {1280, 720}, {1280, 800}, {1280, 960},  {1280, 1024},
    {1400, 1050}, {1440, 900}, {1600, 900}, {1600, 1200}, {1680, 1050},
    {1920, 1080}, {1920, 1200}};
const int kNumPresets = OZZ_ARRAY_SIZE(resolution_presets);

// Check resolution argument is within 0 - kNumPresets
bool ResolutionCheck(const ozz::options::Option& _option,
                     int /*_argc*/) {
  const ozz::options::IntOption& option =
      static_cast<const ozz::options::IntOption&>(_option);
  return option >= 0 && option < kNumPresets;
}
}  // namespace


OZZ_OPTIONS_DECLARE_INT_FN(resolution, "Resolution index (0 to 17).", 5, false,
                           &ResolutionCheck);

namespace ozz {
namespace sample {
Application* Application::application_ = nullptr;

Application::Application()
    : window_(nullptr),
      gl_context_(nullptr),
      exit_(false),
      freeze_(false),
      fix_update_rate(false),
      fixed_update_rate(60.f),
      time_factor_(1.f),
      time_(0.f),
      last_idle_time_(0.),
      show_help_(false),
      vertical_sync_(true),
      swap_interval_(1),
      show_grid_(true),
      show_axes_(true),
      capture_video_(false),
      capture_screenshot_(false),
      fps_(New<Record>(128)),
      update_time_(New<Record>(128)),
      render_time_(New<Record>(128)),
      resolution_(resolution_presets[0]) {
#ifndef NDEBUG
  // Assert presets are correctly sorted.
  for (int i = 1; i < kNumPresets; ++i) {
    const Resolution& preset_m1 = resolution_presets[i - 1];
    const Resolution& preset = resolution_presets[i];
    assert(preset.width > preset_m1.width || preset.height > preset_m1.height);
  }
#endif  //  NDEBUG
}

Application::~Application() {}

int Application::Run(int _argc, const char** _argv, const char* _version,
                     const char* _title) {
  // Only one application at a time can be ran.
  if (application_) {
    return EXIT_FAILURE;
  }
  application_ = this;

  // Starting application
  log::Out() << "Starting sample \"" << _title << "\" version \"" << _version
             << "\"" << std::endl;
  log::Out() << "Ozz libraries were built with \""
             << math::SimdImplementationName() << "\" SIMD math implementation."
             << std::endl;

  // Parse command line arguments.
  const char* usage =
      "Ozz animation sample. See README.md file for more details.";
  ozz::options::ParseResult result =
      ozz::options::ParseCommandLine(_argc, _argv, _version, usage);
  if (result != ozz::options::kSuccess) {
    exit_ = true;
    return result == ozz::options::kExitSuccess ? EXIT_SUCCESS : EXIT_FAILURE;
  }

  // Fetch initial resolution.
  resolution_ = resolution_presets[OPTIONS_resolution];

#ifdef __APPLE__
  chdir(ozz::options::ParsedExecutablePath().c_str());
#endif  // __APPLE__

  // Initialize help.
  ParseReadme();

  bool success = true;
  if (OPTIONS_render) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
      log::Err() << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
      application_ = nullptr;
      return EXIT_FAILURE;
    }

    const int gl_version_major = 4;
    const int gl_version_minor = 1;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, gl_version_major);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, gl_version_minor);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    window_ = SDL_CreateWindow(
        _title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        resolution_.width, resolution_.height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

    if (!window_) {
      log::Err() << "Failed to create SDL window: " << SDL_GetError() << std::endl;
      success = false;
    } else {
      gl_context_ = SDL_GL_CreateContext(window_);
      if (!gl_context_) {
        log::Err() << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
        success = false;
      } else {
        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress))) {
          log::Err() << "Failed to initialize GLAD" << std::endl;
          success = false;
        } else {
          log::Out() << "Successfully opened OpenGL window version \""
                     << glGetString(GL_VERSION) << "\"." << std::endl;

          camera_ = make_unique<internal::Camera>();
          math::Float3 camera_center;
          math::Float2 camera_angles;
          float distance;
          if (GetCameraInitialSetup(&camera_center, &camera_angles, &distance)) {
            camera_->Reset(camera_center, camera_angles, distance);
          }

          renderer_ = make_unique<internal::RendererImpl>(camera_.get());
          success = renderer_->Initialize();

          if (success) {
            shooter_ = make_unique<internal::Shooter>();
            im_gui_ = make_unique<internal::ImGuiImpl>();

            int w, h;
            SDL_GetWindowSize(window_, &w, &h);
            Resize(w,h);

            SDL_GL_SetSwapInterval(vertical_sync_ ? swap_interval_ : 0);

            success = Loop();
            shooter_.reset();
            im_gui_.reset();
          }
          renderer_.reset();
          camera_.reset();
        }
      }
    }

    if (gl_context_) {
      SDL_GL_DeleteContext(gl_context_);
    }
    if (window_) {
      SDL_DestroyWindow(window_);
    }
    SDL_Quit();

  } else {
    success = Loop();
  }

  if (!success) {
    log::Err() << "An error occurred during sample execution." << std::endl;
  }
  application_ = nullptr;
  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

Application::LoopStatus Application::OneLoop(const InputState& _input_state, int _loops) {
  Profiler profile(fps_.get());

  if (exit_) {
    return kBreak;
  }

  if (OPTIONS_max_idle_loops > 0 && _loops > OPTIONS_max_idle_loops) {
    return kBreak;
  }

  if (!Idle(_loops == 0)) {
    return kBreakFailure;
  }

  if (OPTIONS_render) {
    if (!Display(_input_state)) {
      return kBreakFailure;
    }
  }

  return kContinue;
}

bool Application::Loop() {
  bool success = true;

  success = OnInitialize();
  if (!success) {
    OnDestroy();
    return false;
  }

  last_idle_time_ = SDL_GetTicks() / 1000.0;

  InputState input_state{};
  input_state.keyboard = SDL_GetKeyboardState(nullptr);
  std::map<SDL_FingerID, Application::InputState::Finger> last_touch_state;

  for (int loops = 0; !exit_ && success; ++loops) {

    // Reset one-time-events
    input_state.touch_down = false;
    input_state.touch_up = false;
    input_state.mouse_wheel = 0;

    // Reset finger delta movements
    for(int i = 0; i < input_state.num_touch_fingers; ++i) {
      input_state.fingers[i].dx = 0.f;
      input_state.fingers[i].dy = 0.f;
    }

    SDL_Event event;
    while(SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        exit_ = true;
      }
      if (event.type == SDL_WINDOWEVENT) {
        if(event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window_)) {
          exit_ = true;
        }
        if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
          Resize(event.window.data1, event.window.data2);
        }
      }
      if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.sym == SDLK_ESCAPE) exit_ = true;
        if (event.key.keysym.sym == SDLK_F1) show_help_ = !show_help_;
        if (event.key.keysym.sym == SDLK_s) capture_screenshot_ = true;
        if (event.key.keysym.sym == SDLK_v) capture_video_ = !capture_video_;
      }
      if (event.type == SDL_MOUSEWHEEL) {
        input_state.mouse_wheel = event.wheel.y;
      }

      // Handle Touch Events
      if (event.type == SDL_FINGERDOWN) {
        input_state.touch_down = true;
        if (input_state.num_touch_fingers < 2) {
          auto& finger = input_state.fingers[input_state.num_touch_fingers++];
          finger.id = event.tfinger.fingerId;
          finger.x = event.tfinger.x * resolution_.width;
          finger.y = event.tfinger.y * resolution_.height;
          last_touch_state[finger.id] = finger;
        }
      } else if (event.type == SDL_FINGERUP) {
        input_state.touch_up = true;
        for (int i = 0; i < input_state.num_touch_fingers; ++i) {
          if (input_state.fingers[i].id == event.tfinger.fingerId) {
            // Remove finger by swapping with the last one
            last_touch_state.erase(input_state.fingers[i].id);
            input_state.fingers[i] = input_state.fingers[input_state.num_touch_fingers - 1];
            input_state.num_touch_fingers--;
            break;
          }
        }
      } else if (event.type == SDL_FINGERMOTION) {
        for (int i = 0; i < input_state.num_touch_fingers; ++i) {
          if (input_state.fingers[i].id == event.tfinger.fingerId) {
            auto& finger = input_state.fingers[i];
            auto& last_finger = last_touch_state[finger.id];
            finger.x = event.tfinger.x * resolution_.width;
            finger.y = event.tfinger.y * resolution_.height;
            finger.dx = finger.x - last_finger.x;
            finger.dy = finger.y - last_finger.y;
            last_finger.x = finger.x;
            last_finger.y = finger.y;
            break;
          }
        }
      }
    }

    input_state.mouse_buttons = SDL_GetMouseState(&input_state.mouse_x, &input_state.mouse_y);

    const LoopStatus status = OneLoop(input_state, loops);
    success = status != kBreakFailure;
    if (status == kBreak) {
      break;
    }

    capture_screenshot_ = false;
  }

  OnDestroy();
  return success;
}

bool Application::Display(const InputState& _input_state) {
  assert(OPTIONS_render);
  bool success = true;

  {
    Profiler profile(render_time_.get());

    glClearDepthf(1.f);
    glClearColor(.4f, .42f, .38f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);

    camera_->Bind3D();

    if (success) {
      success = OnDisplay(renderer_.get());
    }
  }

  if (show_grid_) {
    renderer_->DrawGrid(20, 1.f);
  }
  if (show_axes_) {
    renderer_->DrawAxes(ozz::math::Float4x4::identity());
  }

  camera_->Bind2D();

  if (success) {
    success = Gui(_input_state);
  }

  if (capture_screenshot_ || capture_video_) {
    if(shooter_) shooter_->Capture(GL_BACK);
  }

  SDL_GL_SwapWindow(window_);

  return success;
}

bool Application::Idle(bool _first_frame) {
  if (show_help_) {
    last_idle_time_ = SDL_GetTicks() / 1000.0;
    return true;
  }

  float delta;
  double time = SDL_GetTicks() / 1000.0;
  if (_first_frame) {
    delta = 1.f / 60.f;
  } else {
    delta = static_cast<float>(time - last_idle_time_);
  }
  last_idle_time_ = time;

  float update_delta;
  if (freeze_) {
    update_delta = 0.f;
  } else {
    if (fix_update_rate) {
      update_delta = time_factor_ / fixed_update_rate;
    } else {
      update_delta = delta * time_factor_;
    }
  }

  time_ += update_delta;

  bool update_result;
  {
    Profiler profile(update_time_.get());
    update_result = OnUpdate(update_delta, time_);
  }

  if (shooter_) {
    shooter_->Update();
  }

  // Camera is now updated in the main loop based on input state.
  // This part is now handled inside the OneLoop function logic.
  // if (camera_) { ... }

  return update_result;
}


bool Application::Gui(const InputState& _input_state) {
  bool success = true;
  const float kFormWidth = 200.f;
  const float kHelpMargin = 16.f;
  const float kGuiMargin = 2.f;

  ozz::math::RectInt window_rect(0, 0, resolution_.width, resolution_.height);

  internal::ImGuiImpl::Inputs input;
  input.mouse_x = _input_state.mouse_x;
  input.mouse_y = window_rect.height - _input_state.mouse_y;
  input.lmb_pressed = (_input_state.mouse_buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;

  im_gui_->BeginFrame(input, window_rect, renderer_.get());

  ImGui* im_gui = im_gui_.get();

  if (!show_help_) {
    success = OnFloatingGui(im_gui);
  }

  {
    math::RectFloat rect(kGuiMargin, kGuiMargin,
                         window_rect.width - kGuiMargin * 2.f,
                         window_rect.height - kGuiMargin * 2.f);
    ImGui::Form form(im_gui, "Show help", rect, &show_help_, !show_help_);
    if (show_help_) {
      im_gui->DoLabel(help_.c_str(), ImGui::kLeft, false);
    }
  }

  if (!show_help_ && success &&
      window_rect.width > (kGuiMargin + kFormWidth) * 2.f) {
    static bool open = true;
    math::RectFloat rect(kGuiMargin, kGuiMargin, kFormWidth,
                         window_rect.height - kGuiMargin * 2.f - kHelpMargin);
    ImGui::Form form(im_gui, "Framework", rect, &open, true);
    if (open) {
      success = FrameworkGui();
    }
  }

  if (!show_help_ && success && window_rect.width > kGuiMargin + kFormWidth) {
    static bool open = true;
    math::RectFloat rect(window_rect.width - kFormWidth - kGuiMargin,
                         kGuiMargin, kFormWidth,
                         window_rect.height - kGuiMargin * 2 - kHelpMargin);
    ImGui::Form form(im_gui, "Sample", rect, &open, true);
    if (open) {
      success = OnGui(im_gui);
    }
  }

  im_gui_->EndFrame();

  return success;
}

bool Application::FrameworkGui() {
  char label[64];
  ImGui* im_gui = im_gui_.get();
  {
    static bool open = true;
    ImGui::OpenClose stat_oc(im_gui, "Statistics", &open);
    if (open) {
      {
        Record::Statistics statistics = fps_->GetStatistics();
        std::snprintf(label, sizeof(label), "FPS: %.0f",
                      statistics.mean == 0.f ? 0.f : 1000.f / statistics.mean);
        static bool fps_open = false;
        ImGui::OpenClose stats(im_gui, label, &fps_open);
        if (fps_open) {
          std::snprintf(label, sizeof(label), "Frame: %.2f ms",
                        statistics.mean);
          im_gui->DoGraph(label, 0.f, statistics.max, statistics.latest,
                          fps_->cursor(), fps_->record_begin(),
                          fps_->record_end());
        }
      }
      {
        Record::Statistics statistics = update_time_->GetStatistics();
        std::snprintf(label, sizeof(label), "Update: %.2f ms", statistics.mean);
        static bool update_open = true;
        ImGui::OpenClose stats(im_gui, label, &update_open);
        if (update_open) {
          im_gui->DoGraph(nullptr, 0.f, statistics.max, statistics.latest,
                          update_time_->cursor(), update_time_->record_begin(),
                          update_time_->record_end());
        }
      }
      {
        Record::Statistics statistics = render_time_->GetStatistics();
        std::snprintf(label, sizeof(label), "Render: %.2f ms", statistics.mean);
        static bool render_open = false;
        ImGui::OpenClose stats(im_gui, label, &render_open);
        if (render_open) {
          im_gui->DoGraph(nullptr, 0.f, statistics.max, statistics.latest,
                          render_time_->cursor(), render_time_->record_begin(),
                          render_time_->record_end());
        }
      }
    }
  }

  {
    static bool open = false;
    ImGui::OpenClose stats(im_gui, "Time control", &open);
    if (open) {
      im_gui->DoButton("Freeze", true, &freeze_);
      im_gui->DoCheckBox("Fix update rate", &fix_update_rate, true);
      if (!fix_update_rate) {
        std::snprintf(label, sizeof(label), "Time factor: %.2f", time_factor_);
        im_gui->DoSlider(label, -5.f, 5.f, &time_factor_);
        if (im_gui->DoButton("Reset time factor", time_factor_ != 1.f)) {
          time_factor_ = 1.f;
        }
      } else {
        std::snprintf(label, sizeof(label), "Update rate: %.0f fps",
                      fixed_update_rate);
        im_gui->DoSlider(label, 1.f, 200.f, &fixed_update_rate, .5f, true);
        if (im_gui->DoButton("Reset update rate", fixed_update_rate != 60.f)) {
          fixed_update_rate = 60.f;
        }
      }
    }
  }

  {
    static bool open = false;
    ImGui::OpenClose options(im_gui, "Options", &open);
    if (open) {
      int fsaa_samples;
      SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &fsaa_samples);
      static bool fsaa_available = fsaa_samples > 0;
      static bool fsaa_enabled = fsaa_available;
      if (im_gui->DoCheckBox("Anti-aliasing", &fsaa_enabled, fsaa_available)) {
        if (fsaa_enabled) {
//          glEnable(GL_MULTISAMPLE);
        } else {
//          glDisable(GL_MULTISAMPLE);
        }
      }

      bool changed = im_gui->DoCheckBox("Vertical sync", &vertical_sync_);
      std::snprintf(label, sizeof(label), "Swap interval: %d", swap_interval_);
      changed |=
          im_gui->DoSlider(label, 0, 4, &swap_interval_, 1.f, vertical_sync_);
      if (changed) {
        SDL_GL_SetSwapInterval(vertical_sync_ ? swap_interval_ : 0);
      }

      im_gui->DoCheckBox("Show grid", &show_grid_, true);
      im_gui->DoCheckBox("Show axes", &show_axes_, true);
    }

    int preset_lookup = 0;
    for (; preset_lookup < kNumPresets - 1; ++preset_lookup) {
      const Resolution& preset = resolution_presets[preset_lookup];
      if (preset.width > resolution_.width) {
        break;
      } else if (preset.width == resolution_.width) {
        if (preset.height >= resolution_.height) {
          break;
        }
      }
    }

    std::snprintf(label, sizeof(label), "Resolution: %dx%d", resolution_.width,
                  resolution_.height);
    if (im_gui->DoSlider(label, 0, kNumPresets - 1, &preset_lookup)) {
      resolution_ = resolution_presets[preset_lookup];
      SDL_SetWindowSize(window_, resolution_.width, resolution_.height);
    }
  }

  {
    static bool open = false;
    ImGui::OpenClose controls(im_gui, "Capture", &open);
    if (open) {
      im_gui->DoButton("Capture video", true, &capture_video_);
      capture_screenshot_ |= im_gui->DoButton(
          "Capture screenshot", !capture_video_, &capture_screenshot_);
    }
  }

  {
    static bool open = false;
    ImGui::OpenClose controls(im_gui, "Camera controls", &open);
    if (open) {
      camera_->OnGui(im_gui);
    }
  }
  return true;
}

bool Application::OnInitialize() { return true; }

void Application::OnDestroy() {}

bool Application::OnUpdate(float, float) { return true; }

bool Application::OnGui(ImGui*) { return true; }

bool Application::OnFloatingGui(ImGui*) { return true; }

bool Application::OnDisplay(Renderer*) { return true; }

bool Application::GetCameraInitialSetup(math::Float3*, math::Float2*,
                                        float*) const {
  return false;
}

bool Application::GetCameraOverride(math::Float4x4*) const { return false; }

void Application::GetSceneBounds(math::Box*) const {}

math::Float2 Application::WorldToScreen(const math::Float3& _world) const {
  const math::SimdFloat4 ndc =
      (camera_->projection() * camera_->view()) *
      math::simd_float4::Load(_world.x, _world.y, _world.z, 1.f);

  const math::SimdFloat4 resolution = math::simd_float4::FromInt(
      math::simd_int4::Load(resolution_.width, resolution_.height, 0, 0));
  const ozz::math::SimdFloat4 screen =
      resolution * ((ndc / math::SplatW(ndc)) + math::simd_float4::one()) /
      math::simd_float4::Load1(2.f);
  math::Float2 ret;
  math::Store2PtrU(screen, &ret.x);
  return ret;
}

void Application::Resize(int _width, int _height) {
  application_->resolution_.width = _width;
  application_->resolution_.height = _height;

  glViewport(0, 0, _width, _height);

  if(application_->camera_) application_->camera_->Resize(_width, _height);
  if(application_->shooter_) application_->shooter_->Resize(_width, _height);
}

void Application::ParseReadme() {
  const char* error_message = "Unable to find README.md help file.";

  ozz::io::File file("README.md", "rb");
  if (!file.opened()) {
    help_ = error_message;
    return;
  }

  const size_t read_length = file.Size();
  ozz::memory::Allocator* allocator = ozz::memory::default_allocator();
  char* content = reinterpret_cast<char*>(allocator->Allocate(read_length, 4));

  if (file.Read(content, read_length) == read_length) {
    help_ = ozz::string(content, content + read_length);
  } else {
    help_ = error_message;
  }

  allocator->Deallocate(content);
}
}  // namespace sample
}  // namespace ozz
