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

    const Platform* platform() { return platform_.get(); }

  private:
    std::unique_ptr<Renderer> renderer_;
    std::unique_ptr<Platform> platform_;
};

}  // namespace goma
