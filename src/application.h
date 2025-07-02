// 完整替换 src/application.h

#ifndef OZZ_SAMPLES_FRAMEWORK_APPLICATION_H_
#define OZZ_SAMPLES_FRAMEWORK_APPLICATION_H_

#include <cstddef>
#include <cstdint>
#include <SDL2/SDL.h>
#include "ozz/base/containers/string.h"
#include "ozz/base/memory/unique_ptr.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace ozz {
    namespace math {
        struct Box;
        struct Float2;
        struct Float3;
        struct Float4x4;
    }  // namespace math
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

// Screen resolution settings.
        struct Resolution {
            int width;
            int height;
        };

        class Application {
        public:
            Application();
            virtual ~Application();

            int Run(int _argc, char* _argv[], const char* _version, const char* _title);

            // Defines the state of user inputs.
            struct InputState {
                const uint8_t* keyboard;
                uint32_t mouse_buttons;
                int mouse_x;
                int mouse_y;
                int mouse_wheel;

                // ================== 修正触摸支持 ==================
                struct TouchPoint {
                    SDL_FingerID id; // 用于唯一标识一个触摸点
                    bool down;
                    float x, y;
                    float last_x, last_y;
                };
                int num_touch_fingers;
                TouchPoint fingers[2]; // 最多支持两个手指用于手势识别
                // ===================================================
            };

            // 新增的主循环迭代函数
            void MainLoopIteration();

        protected:
            math::Float2 WorldToScreen(const math::Float3& _world) const;
            internal::Camera* camera() { return camera_.get(); }
            const internal::Camera* camera() const { return camera_.get(); }
            const Resolution& resolution() const { return resolution_; }

        private:
            // 这些函数现在将在 application.cc 中被完整定义
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
            bool Idle(const InputState& _input, bool _first_frame);
            bool Display(const InputState& _input);
            bool Gui(const InputState& _input);
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

            // 为Emscripten循环准备的成员变量
            bool initialized_ = false;
            InputState input_state_{};
            int loops_ = 0;


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
    }  // namespace sample
}  // namespace ozz
#endif  // OZZ_SAMPLES_FRAMEWORK_APPLICATION_H_