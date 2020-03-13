#pragma once

#include "input/input_system.hpp"
#include "renderer/renderer.hpp"
#include "scripting/scripting_system.hpp"
#include "platform/platform.hpp"
#include "scene/scene.hpp"
#include "scene/attachments/camera.hpp"
#include "scene/attachments/light.hpp"

namespace goma {

class Engine {
  public:
    Engine();

    result<void> MainLoop(MainLoopFn inner_fn);
    result<void> LoadScene(const char* file_path);

    Platform& platform() { return *platform_.get(); }
    InputSystem& input_system() { return *input_system_.get(); }
    ScriptingSystem& scripting_system() { return *scripting_system_.get(); }
    Renderer& renderer() { return *renderer_.get(); }
    Scene* scene() { return scene_.get(); }
    gen_id main_camera_id() { return main_camera_id_; }

    uint32_t frame_count() { return frame_count_; }

  private:
    std::unique_ptr<Platform> platform_{};
    std::unique_ptr<InputSystem> input_system_{};
    std::unique_ptr<ScriptingSystem> scripting_system_{};
    std::unique_ptr<Renderer> renderer_{};

    std::unique_ptr<Scene> scene_{};
    gen_id main_camera_id_{};

    uint32_t frame_count_{0};

    uint32_t fps_cap{60};
    std::chrono::duration<float> delta_time_{0.0f};
    std::chrono::time_point<std::chrono::high_resolution_clock>
        frame_timestamp_{std::chrono::high_resolution_clock::now()};

    gen_id CreateDefaultCamera();
    gen_id CreateDefaultLight();
};

}  // namespace goma
