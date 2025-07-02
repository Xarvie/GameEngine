// 完整替换 src/application.cc

#define OZZ_INCLUDE_PRIVATE_HEADER

#include "application.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#ifdef __APPLE__
#include <unistd.h>
#endif
#include <glad/glad.h>
#include "image.h"
#include "internal/camera.h"
#include "internal/imgui_impl.h"
#include "internal/renderer_impl.h"
#include "internal/shooter.h"
#include "profile.h"
#include "renderer.h"
#include "ozz/base/io/stream.h"
#include "ozz/base/log.h"
#include "ozz/base/maths/box.h"
#include "ozz/base/memory/allocator.h"
#include "ozz/options/options.h"

OZZ_OPTIONS_DECLARE_INT(max_idle_loops, "The maximum number of idle loops the sample application can perform.", -1, false);
OZZ_OPTIONS_DECLARE_BOOL(render, "Enables sample redering.", true, false);

namespace {
//    const ozz::sample::Resolution resolution_presets[] = {
//            {640, 360},   {640, 480},  {800, 450},  {800, 600},   {1024, 576},
//            {1024, 768},  {1280, 720}, {1280, 800}, {1280, 960},  {1280, 1024},
//            {1400, 1050}, {1440, 900}, {1600, 900}, {1600, 1200}, {1680, 1050},
//            {1920, 1080}, {1920, 1200}};
//    const int kNumPresets = OZZ_ARRAY_SIZE(resolution_presets);

    // 为 emscripten_set_main_loop 创建一个全局应用指针
    ozz::sample::Application* g_app = nullptr;

    // C风格的回调函数
    void MainLoopForEmscripten() {
        if (g_app) {
            g_app->MainLoopIteration();
        }
    }
}  // namespace

//static bool ResolutionCheck(const ozz::options::Option& _option, int) {
//    const ozz::options::IntOption& option = static_cast<const ozz::options::IntOption&>(_option);
//    return option >= 0 && option < kNumPresets;
//}
//OZZ_OPTIONS_DECLARE_INT_FN(resolution, "Resolution index (0 to 17).", 5, false, &ResolutionCheck);

namespace ozz {
    namespace sample {
        Application* Application::application_ = nullptr;
        Application::Application()
                : window_(nullptr), gl_context_(nullptr), exit_(false), freeze_(false),
                  fix_update_rate(false), fixed_update_rate(60.f), time_factor_(1.f),
                  time_(0.f), last_idle_time_(0.), show_help_(false), vertical_sync_(true),
                  swap_interval_(1), show_grid_(true), show_axes_(true),
                  capture_video_(false), capture_screenshot_(false), fps_(New<Record>(128)),
                  update_time_(New<Record>(128)), render_time_(New<Record>(128)),
                  resolution_() {}
        Application::~Application() {}

        int Application::Run(int _argc, char* _argv[], const char* _version,
                             const char* _title) {
            if (application_) { return EXIT_FAILURE; }
            application_ = this;
            g_app = this; // 初始化全局指针

            log::Out() << "LOG: Application::Run - Starting sample \"" << _title << "\" version \"" << _version << "\"" << std::endl;
            log::Out() << "LOG: Application::Run - Ozz libraries were built with \""
                       << math::SimdImplementationName() << "\" SIMD math implementation."
                       << std::endl;

            const char* usage = "Ozz animation sample. See README.md file for more details.";
            ozz::options::ParseResult result = ozz::options::ParseCommandLine(_argc, _argv, _version, usage);
            if (result != ozz::options::kSuccess) {
                exit_ = true;
                return result == ozz::options::kExitSuccess ? EXIT_SUCCESS : EXIT_FAILURE;
            }


#ifdef __APPLE__
            chdir(ozz::options::ParsedExecutablePath().c_str());
#endif
//            ParseReadme();

            bool success = true;
            if (OPTIONS_render) {
                log::Out() << "LOG: Application::Run - Initializing SDL..." << std::endl;
                if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
                    log::Err() << "LOG: Application::Run - Failed to initialize SDL: " << SDL_GetError() << std::endl;
                    application_ = nullptr;
                    return EXIT_FAILURE;
                }
                log::Out() << "LOG: Application::Run - SDL Initialized." << std::endl;

#if __OS_WINDOWS || __OS_MACOS || __OS_LINUX
                const int gl_version_major = 4;
                const int gl_version_minor = 1;
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
                log::Out() << "LOG: Application::Run - Requesting OpenGL " << gl_version_major << "." << gl_version_minor
                           << " Core Profile context." << std::endl;
#else
                const int gl_version_major = 3;
                const int gl_version_minor = 0;
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
                log::Out() << "LOG: Application::Run - Requesting OpenGL ES " << gl_version_major << "." << gl_version_minor
                           << " context for WebGL." << std::endl;
#endif
                SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
                SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

                log::Out() << "LOG: Application::Run - Creating SDL window..." << std::endl;

#ifdef __EMSCRIPTEN__
                // 在创建窗口前，直接通过嵌入JS代码获取浏览器尺寸
    resolution_.width = EM_ASM_INT({
        return window.innerWidth;
    });
    resolution_.height = EM_ASM_INT({
        return window.innerHeight;
    });
    log::Out() << "LOG: Application::Run - Fetched browser dimensions: "
               << resolution_.width << "x" << resolution_.height << std::endl;
#else
                // 对于非Web平台，我们仍然使用预设的分辨率
                resolution_ = {800,600};
#endif
                window_ = SDL_CreateWindow(_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                           resolution_.width, resolution_.height,
                                           SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
                if (!window_) {
                    log::Err() << "LOG: Application::Run - Failed to create SDL window: " << SDL_GetError() << std::endl;
                    success = false;
                } else {
                    log::Out() << "LOG: Application::Run - SDL window created. Creating GL context..." << std::endl;
                    gl_context_ = SDL_GL_CreateContext(window_);
                    if (!gl_context_) {
                        log::Err() << "LOG: Application::Run - Failed to create OpenGL context: " << SDL_GetError() << std::endl;
                        success = false;
                    } else {
                        log::Out() << "LOG: Application::Run - GL context created." << std::endl;
#if !__OS_WEB
                        log::Out() << "LOG: Application::Run - Initializing GLAD..." << std::endl;
                        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress))) {
                            log::Err() << "LOG: Application::Run - Failed to initialize GLAD" << std::endl;
                            success = false;
                        }else{
                            log::Out() << "LOG: Application::Run - GLAD initialized. OpenGL version: \"" << glGetString(GL_VERSION) << "\"." << std::endl;
                        }

#endif
                        if(success)
                        {
#if __OS_WEB
                            log::Out() << "LOG: Application::Run - Webgl initialized. OpenGL version: \"" << glGetString(GL_VERSION) << "\"." << std::endl;
#endif
                            camera_ = make_unique<internal::Camera>();
                            camera_->set_resolution(resolution_);
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
                                Resize(w, h);
                                SDL_GL_SetSwapInterval(vertical_sync_ ? swap_interval_ : 0);
                                log::Out() << "LOG: Application::Run - Entering main loop...." << std::endl;
                                success = Loop();
                            }else{
                                log::Err() << "renderer_->Initialize() error ..." << std::endl;
                            }

                            shooter_.reset(nullptr);
                            im_gui_.reset(nullptr);
                            renderer_.reset(nullptr);
                            camera_.reset(nullptr);
                        }
                    }
                }
                if (gl_context_) {
                    log::Out() << "LOG: Application::Run - Deleting GL context." << std::endl;
                    SDL_GL_DeleteContext(gl_context_);
                }
                if (window_) {
                    log::Out() << "LOG: Application::Run - Destroying window." << std::endl;
                    SDL_DestroyWindow(window_);
                }
                log::Out() << "LOG: Application::Run - Quitting SDL." << std::endl;
                SDL_Quit();
            } else {
                success = Loop();
            }
            if (!success) log::Err() << "An error occurred during sample execution." << std::endl;
            application_ = nullptr;
            g_app = nullptr;
            return success ? EXIT_SUCCESS : EXIT_FAILURE;
        }

        void Application::MainLoopIteration() {
            if (exit_) {
#ifdef __EMSCRIPTEN__
                emscripten_cancel_main_loop();
#endif
                return;
            }

            loops_++;
            Profiler profile(fps_.get());
            input_state_.mouse_wheel = 0;

            for (int i = 0; i < 2; ++i) {
                if(input_state_.fingers[i].down) {
                    input_state_.fingers[i].last_x = input_state_.fingers[i].x;
                    input_state_.fingers[i].last_y = input_state_.fingers[i].y;
                }
            }

            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) exit_ = true;
                if (event.type == SDL_WINDOWEVENT) {
                    if (event.window.event == SDL_WINDOWEVENT_CLOSE &&
                        event.window.windowID == SDL_GetWindowID(window_)) exit_ = true;
                    if (event.window.event == SDL_WINDOWEVENT_RESIZED)
                        Resize(event.window.data1, event.window.data2);
                }
                if (event.type == SDL_KEYDOWN) {
                    if (event.key.keysym.sym == SDLK_ESCAPE) exit_ = true;
                    if (event.key.keysym.sym == SDLK_F1) show_help_ = !show_help_;
                    if (event.key.keysym.sym == SDLK_s) capture_screenshot_ = true;
                    if (event.key.keysym.sym == SDLK_v) capture_video_ = !capture_video_;
                }
                if (event.type == SDL_MOUSEWHEEL) {
                    input_state_.mouse_wheel = event.wheel.y;
                }

                switch (event.type) {
                    case SDL_FINGERDOWN: {
                        bool found = false;
                        for(int i = 0; i < 2; ++i) {
                            if (input_state_.fingers[i].down && input_state_.fingers[i].id == event.tfinger.fingerId) {
                                found = true;
                                break;
                            }
                        }
                        if(!found) {
                            for(int i = 0; i < 2; ++i) {
                                if (!input_state_.fingers[i].down) {
                                    auto& finger = input_state_.fingers[i];
                                    finger.id = event.tfinger.fingerId;
                                    finger.down = true;
                                    finger.x = event.tfinger.x * resolution_.width;
                                    finger.y = event.tfinger.y * resolution_.height;
                                    finger.last_x = finger.x;
                                    finger.last_y = finger.y;
                                    input_state_.num_touch_fingers++;
                                    break;
                                }
                            }
                        }
                        break;
                    }
                    case SDL_FINGERUP: {
                        for(int i = 0; i < 2; ++i) {
                            if (input_state_.fingers[i].down && input_state_.fingers[i].id == event.tfinger.fingerId) {
                                input_state_.fingers[i].down = false;
                                input_state_.num_touch_fingers--;
                                break;
                            }
                        }
                        break;
                    }
                    case SDL_FINGERMOTION: {
                        for(int i = 0; i < 2; ++i) {
                            if (input_state_.fingers[i].down && input_state_.fingers[i].id == event.tfinger.fingerId) {
                                auto& finger = input_state_.fingers[i];
                                finger.x = event.tfinger.x * resolution_.width;
                                finger.y = event.tfinger.y * resolution_.height;
                                break;
                            }
                        }
                        break;
                    }
                }
            }

            input_state_.mouse_buttons = SDL_GetMouseState(&input_state_.mouse_x, &input_state_.mouse_y);

            if (OPTIONS_max_idle_loops > 0 && loops_ > OPTIONS_max_idle_loops) {
                exit_ = true;
                return;
            }
            if (!Idle(input_state_, loops_ == 1)) {
                exit_ = true;
                return;
            }
            if (OPTIONS_render) {
                if (!Display(input_state_)) {
                    exit_ = true;
                    return;
                }
            }
            capture_screenshot_ = false;
        }

        bool Application::Loop() {
            log::Out() << "LOG: Application::Loop - Calling OnInitialize()." << std::endl;
            if (!OnInitialize()) {
                log::Err() << "LOG: Application::Loop - OnInitialize() failed." << std::endl;
                OnDestroy();
                return false;
            }
            log::Out() << "LOG: Application::Loop - OnInitialize() successful." << std::endl;

            initialized_ = true;
            last_idle_time_ = SDL_GetTicks() / 1000.0;
            input_state_.keyboard = SDL_GetKeyboardState(nullptr);
            input_state_.num_touch_fingers = 0;
            for (int i = 0; i < 2; ++i) {
                input_state_.fingers[i] = {0, false, 0.f, 0.f, 0.f, 0.f};
            }
            loops_ = 0;

#ifdef __EMSCRIPTEN__
            emscripten_set_main_loop(MainLoopForEmscripten, 0, 1);
#else
            while (!exit_) {
                MainLoopIteration();
            }
#endif

            log::Out() << "LOG: Application::Loop - Exiting loop. Calling OnDestroy()." << std::endl;
            OnDestroy();
            return true;
        }

        bool Application::Display(const InputState& _input) {
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
                if (success) success = OnDisplay(renderer_.get());
            }

            if (show_grid_) renderer_->DrawGrid(20, 1.f);
            if (show_axes_) renderer_->DrawAxes(ozz::math::Float4x4::identity());

            camera_->Bind2D();
            if (success) success = Gui(_input);
            if (capture_screenshot_ || capture_video_) shooter_->Capture(GL_BACK);

            SDL_GL_SwapWindow(window_);
            return success;
        }

        bool Application::Idle(const InputState& _input, bool _first_frame) {
            if (show_help_) {
                last_idle_time_ = SDL_GetTicks() / 1000.0;
                return true;
            }

            float delta;
            double time = SDL_GetTicks() / 1000.0;
            if (_first_frame || time == 0.) {
                delta = 1.f / 60.f;
            } else {
                delta = static_cast<float>(time - last_idle_time_);
            }
            last_idle_time_ = time;

            if (camera_) {
                math::Box scene_bounds;
                GetSceneBounds(&scene_bounds);
                math::Float4x4 camera_transform;
                if (GetCameraOverride(&camera_transform)) {
                    camera_->Update(_input, camera_transform, scene_bounds, delta, _first_frame);
                } else {
                    camera_->Update(_input, scene_bounds, delta, _first_frame);
                }
            }

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

            if (shooter_) shooter_->Update();
            return update_result;
        }

        bool Application::Gui(const InputState& _input) {
            bool success = true;
            const float kFormWidth = 200.f;
            const float kHelpMargin = 16.f;
            const float kGuiMargin = 2.f;

            ozz::math::RectInt window_rect(0, 0, resolution_.width, resolution_.height);
            internal::ImGuiImpl::Inputs gui_input;
            gui_input.mouse_x = _input.mouse_x;
            gui_input.mouse_y = window_rect.height - _input.mouse_y;
            gui_input.lmb_pressed = (_input.mouse_buttons & SDL_BUTTON_LMASK) != 0;

            im_gui_->BeginFrame(gui_input, window_rect, renderer_.get());
            ImGui* im_gui = im_gui_.get();

            if (!show_help_) success = OnFloatingGui(im_gui);

            {
                math::RectFloat rect(kGuiMargin, kGuiMargin,
                                     window_rect.width - kGuiMargin * 2.f,
                                     window_rect.height - kGuiMargin * 2.f);
                ImGui::Form form(im_gui, "Show help", rect, &show_help_, !show_help_);
                if (show_help_) im_gui->DoLabel(help_.c_str(), ImGui::kLeft, false);
            }

            if (!show_help_ && success && window_rect.width > (kGuiMargin + kFormWidth) * 2.f) {
                static bool open = true;
                math::RectFloat rect(kGuiMargin, kGuiMargin, kFormWidth,
                                     window_rect.height - kGuiMargin * 2.f - kHelpMargin);
                ImGui::Form form(im_gui, "Framework", rect, &open, true);
                if (open) success = FrameworkGui();
            }

            if (!show_help_ && success && window_rect.width > kGuiMargin + kFormWidth) {
                static bool open = true;
                math::RectFloat rect(window_rect.width - kFormWidth - kGuiMargin,
                                     kGuiMargin, kFormWidth,
                                     window_rect.height - kGuiMargin * 2 - kHelpMargin);
                ImGui::Form form(im_gui, "Sample", rect, &open, true);
                if (open) success = OnGui(im_gui);
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
                        std::snprintf(label, sizeof(label), "FPS: %.0f", statistics.mean == 0.f ? 0.f : 1000.f / statistics.mean);
                        static bool fps_open = false;
                        ImGui::OpenClose stats(im_gui, label, &fps_open);
                        if (fps_open) {
                            std::snprintf(label, sizeof(label), "Frame: %.2f ms", statistics.mean);
                            im_gui->DoGraph(label, 0.f, statistics.max, statistics.latest, fps_->cursor(), fps_->record_begin(), fps_->record_end());
                        }
                    }
                    {
                        Record::Statistics statistics = update_time_->GetStatistics();
                        std::snprintf(label, sizeof(label), "Update: %.2f ms", statistics.mean);
                        static bool update_open = true;
                        ImGui::OpenClose stats(im_gui, label, &update_open);
                        if (update_open) {
                            im_gui->DoGraph(nullptr, 0.f, statistics.max, statistics.latest, update_time_->cursor(), update_time_->record_begin(), update_time_->record_end());
                        }
                    }
                    {
                        Record::Statistics statistics = render_time_->GetStatistics();
                        std::snprintf(label, sizeof(label), "Render: %.2f ms", statistics.mean);
                        static bool render_open = false;
                        ImGui::OpenClose stats(im_gui, label, &render_open);
                        if (render_open) {
                            im_gui->DoGraph(nullptr, 0.f, statistics.max, statistics.latest, render_time_->cursor(), render_time_->record_begin(), render_time_->record_end());
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
                        if (im_gui->DoButton("Reset time factor", time_factor_ != 1.f)) time_factor_ = 1.f;
                    } else {
                        std::snprintf(label, sizeof(label), "Update rate: %.0f fps", fixed_update_rate);
                        im_gui->DoSlider(label, 1.f, 200.f, &fixed_update_rate, .5f, true);
                        if (im_gui->DoButton("Reset update rate", fixed_update_rate != 60.f)) fixed_update_rate = 60.f;
                    }
                }
            }
            {
                static bool open = false;
                ImGui::OpenClose options(im_gui, "Options", &open);
                if (open) {
                    bool changed = im_gui->DoCheckBox("Vertical sync", &vertical_sync_);
                    std::snprintf(label, sizeof(label), "Swap interval: %d", swap_interval_);
                    changed |= im_gui->DoSlider(label, 0, 4, &swap_interval_, 1.f, vertical_sync_);
                    if (changed) SDL_GL_SetSwapInterval(vertical_sync_ ? swap_interval_ : 0);
                    im_gui->DoCheckBox("Show grid", &show_grid_, true);
                    im_gui->DoCheckBox("Show axes", &show_axes_, true);
                }

            }
            {
                static bool open = false;
                ImGui::OpenClose controls(im_gui, "Capture", &open);
                if (open) {
                    im_gui->DoButton("Capture video", true, &capture_video_);
                    capture_screenshot_ |= im_gui->DoButton("Capture screenshot", !capture_video_, &capture_screenshot_);
                }
            }
            {
                static bool open = false;
                ImGui::OpenClose controls(im_gui, "Camera controls", &open);
                if (open) camera_->OnGui(im_gui);
            }
            return true;
        }

        bool Application::OnInitialize() {
            log::Out() << "LOG: Application::OnInitialize - Base class OnInitialize called." << std::endl;
            return true;
        }
        void Application::OnDestroy() {
            log::Out() << "LOG: Application::OnDestroy - Base class OnDestroy called." << std::endl;
        }
        bool Application::OnUpdate(float, float) { return true; }
        bool Application::OnGui(ImGui*) { return true; }
        bool Application::OnFloatingGui(ImGui*) { return true; }
        bool Application::OnDisplay(Renderer*) { return true; }
        bool Application::GetCameraInitialSetup(math::Float3*, math::Float2*, float*) const { return false; }
        bool Application::GetCameraOverride(math::Float4x4*) const { return false; }
        void Application::GetSceneBounds(math::Box*) const {}

        math::Float2 Application::WorldToScreen(const math::Float3& _world) const {
            const math::SimdFloat4 ndc = (camera_->projection() * camera_->view()) *
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
            log::Out() << "LOG: Application::Resize - Window resized to " << _width << "x" << _height << std::endl;
            resolution_.width = _width;
            resolution_.height = _height;
            glViewport(0, 0, _width, _height);
            if (camera_) {
                camera_->set_resolution(resolution_);
                camera_->Resize(_width, _height);
            }
            if (shooter_) shooter_->Resize(_width, _height);
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
    } // namespace sample
} // namespace ozz