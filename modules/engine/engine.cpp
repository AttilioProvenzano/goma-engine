#include "engine/engine.hpp"

#include "common/error_codes.hpp"
#include "platform/win32_platform.hpp"
#include "scene/loaders/assimp_loader.hpp"
#include "scripting/scripts/fly_camera.hpp"

namespace goma {

Engine::Engine()
    : platform_(std::make_unique<Win32Platform>()),
      input_system_(std::make_unique<InputSystem>(*platform_.get())),
      scripting_system_{std::make_unique<ScriptingSystem>(*this)} {
    auto res = platform_->InitWindow(1280, 800);
    if (res.has_error()) {
        throw std::runtime_error(res.error().message());
    }

    renderer_ = std::make_unique<Renderer>(*this);
}

result<void> Engine::MainLoop(MainLoopFn inner_loop) {
    if (platform_) {
        OUTCOME_TRY(platform_->MainLoop([&]() -> result<bool> {
            if (fps_cap > 0) {
                // Limit FPS
                std::chrono::duration<float> elapsed =
                    std::chrono::high_resolution_clock::now() -
                    frame_timestamp_;

                auto min_frame_time = 1.0f / fps_cap;
                if (elapsed.count() < min_frame_time) {
                    auto wait_us = 1e6 * (min_frame_time - elapsed.count());
                    platform_->Sleep(static_cast<uint32_t>(wait_us));
                }
            }

            auto now = std::chrono::high_resolution_clock::now();
            delta_time_ = now - frame_timestamp_;
            frame_timestamp_ = now;

            OUTCOME_TRY(input_system_->AcquireFrameInput());
            scripting_system_->Update(delta_time_.count());
            OUTCOME_TRY(renderer_->Render());

            bool res = false;
            if (inner_loop) {
                // Inner loop allows for conditional termination
                OUTCOME_TRY(res, inner_loop());
            }

            frame_count_++;
            return res;
        }));
    }

    return outcome::success();
};

result<void> Engine::LoadScene(const char* file_path) {
    AssimpLoader loader;
    OUTCOME_TRY(scene, loader.ReadSceneFromFile(file_path));
    scene_ = std::move(scene);

    main_camera_id_ = CreateDefaultCamera();

    if (scene_->lights().empty()) {
        CreateDefaultLight();
    }

    FlyCamera fly_camera(main_camera_id_, 5.0f);
    scripting_system_->RegisterScript(std::move(fly_camera));

    // OUTCOME_TRY(renderer_->CreateSkybox());

    return outcome::success();
}

gen_id Engine::CreateDefaultCamera() {
    auto& node = scene_->root_node().add_child("Default camera");

    Camera camera{};
    camera.aspect_ratio = float(platform_->GetWidth()) / platform_->GetHeight();
    camera.attach_to(node);

    return scene_->cameras().push_back(std::move(camera));
}

gen_id Engine::CreateDefaultLight() {
    auto& node = scene_->root_node().add_child("Default light");

    Light light{"Default light"};

    // Light facing down
    light.direction = {0.0f, -1.0f, 0.0f};
    light.up = {1.0f, 0.0f, 0.0f};

    // Tilt it a bit
    auto rotation = glm::quat({glm::radians(5.0f), 0.0f, glm::radians(5.0f)});

    light.attach_to(node);
    return scene_->lights().push_back(std::move(light));
}

}  // namespace goma
