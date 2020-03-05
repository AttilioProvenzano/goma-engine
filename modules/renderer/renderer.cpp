#include "renderer/renderer.hpp"

#include "engine/engine.hpp"

namespace goma {

Renderer::Renderer(Engine& engine) : engine_(engine) {}

result<void> Renderer::Render() { return outcome::success(); }

}  // namespace goma
