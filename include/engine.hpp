#pragma once

#include "input/input_system.hpp"
#include "renderer/renderer.hpp"
#include "platform/platform.hpp"
#include "scene/scene.hpp"

namespace goma {

class Engine {
  public:
    Engine();

    result<void> MainLoop(MainLoopFn inner_fn);
    result<void> LoadScene(const char* file_path);

    Platform* platform() { return platform_.get(); }
    InputSystem* input_system() { return input_system_.get(); }
    Renderer* renderer() { return renderer_.get(); }
    Scene* scene() { return scene_.get(); }

    uint32_t frame_count() { return frame_count_; }

  private:
    uint32_t frame_count_ = 0;

    std::unique_ptr<Platform> platform_;

    std::unique_ptr<InputSystem> input_system_;
    std::unique_ptr<Renderer> renderer_;

    std::unique_ptr<Scene> scene_;
};

}  // namespace goma
