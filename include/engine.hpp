#pragma once

#include <memory>
#include <vector>

#include "renderer/renderer.hpp"
#include "platform/platform.hpp"
#include "scene/scene.hpp"

namespace goma {

class Engine {
  public:
    Engine();

    result<void> LoadScene(const char* file_path);

    Platform* platform() { return platform_.get(); }
    Renderer* renderer() { return renderer_.get(); }
    Scene* scene() { return scene_.get(); }

  private:
    std::unique_ptr<Renderer> renderer_;
    std::unique_ptr<Platform> platform_;
    std::unique_ptr<Scene> scene_;
};

}  // namespace goma
