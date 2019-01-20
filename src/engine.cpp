#include "engine.hpp"

#include "platform/win32_platform.hpp"

namespace goma {

Engine::Engine() : platform_(std::make_unique<Win32Platform>()) {
    platform_->InitWindow();
    renderer_ = std::make_unique<Renderer>(this);
}

}  // namespace goma
