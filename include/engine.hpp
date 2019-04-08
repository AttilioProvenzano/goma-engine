#pragma once

#include "input/input_system.hpp"
#include "renderer/renderer.hpp"
#include "scripting/scripting_system.hpp"
#include "platform/platform.hpp"
#include "scene/scene.hpp"
#include "scene/attachments/camera.hpp"

namespace goma {

class Engine {
  public:
    Engine();

    result<void> MainLoop(MainLoopFn inner_fn);
    result<void> LoadScene(const char* file_path);

    Platform* platform() { return platform_.get(); }
    InputSystem* input_system() { return input_system_.get(); }
    ScriptingSystem* scripting_system() { return scripting_system_.get(); }
    Renderer* renderer() { return renderer_.get(); }
    Scene* scene() { return scene_.get(); }
    AttachmentIndex<Camera> main_camera() { return main_camera_; }

    uint32_t frame_count() { return frame_count_; }

  private:
    uint32_t frame_count_ = 0;

    std::unique_ptr<Platform> platform_;

    std::unique_ptr<InputSystem> input_system_;
    std::unique_ptr<ScriptingSystem> scripting_system_;
    std::unique_ptr<Renderer> renderer_;

    std::unique_ptr<Scene> scene_;
    AttachmentIndex<Camera> main_camera_;

    result<AttachmentIndex<Camera>> CreateDefaultCamera();
};

}  // namespace goma
