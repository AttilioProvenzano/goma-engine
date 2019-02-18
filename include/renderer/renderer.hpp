#pragma once

#include "renderer/backend.hpp"

#include <memory>

namespace goma {

class Engine;

class Renderer {
  public:
    Renderer(Engine* engine);

    result<void> Render();

  private:
    Engine* engine_ = nullptr;
    std::unique_ptr<Backend> backend_;
};

}  // namespace goma
