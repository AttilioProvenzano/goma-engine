#include "engine.hpp"

#include "platform/win32_platform.hpp"
#include "scene/loaders/assimp_loader.hpp"

namespace goma {

Engine::Engine() : platform_(std::make_unique<Win32Platform>()) {
    platform_->InitWindow();
    renderer_ = std::make_unique<Renderer>(this);
}

result<void> Engine::LoadScene(const char* file_path) {
    AssimpLoader loader;
    OUTCOME_TRY(scene, loader.ReadSceneFromFile(file_path));
    scene_ = std::move(scene);

    return outcome::success();
}

}  // namespace goma
