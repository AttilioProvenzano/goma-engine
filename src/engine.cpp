#include "engine.hpp"

#include "platform/win32_platform.hpp"
#include "scene/loaders/assimp_loader.hpp"

namespace goma {

Engine::Engine()
    : platform_(std::make_unique<Win32Platform>()),
      input_system_(std::make_unique<InputSystem>(*platform_.get())) {
    platform_->InitWindow();
    renderer_ = std::make_unique<Renderer>(this);
}

result<void> Engine::MainLoop(MainLoopFn inner_loop) {
    if (platform_) {
        platform_->MainLoop([&]() {
            input_system_->AcquireFrameInput();
            renderer_->Render();

            bool res = false;
            if (inner_loop) {
                // Inner loop allows for conditional termination
                res = inner_loop();
            }

            frame_count_++;
            return res;
        });
    }

    return outcome::success();
};

result<void> Engine::LoadScene(const char* file_path) {
    AssimpLoader loader;
    OUTCOME_TRY(scene, loader.ReadSceneFromFile(file_path));
    scene_ = std::move(scene);

    return outcome::success();
}

}  // namespace goma
