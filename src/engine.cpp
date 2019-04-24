#include "engine.hpp"

#include "platform/win32_platform.hpp"
#include "scene/loaders/assimp_loader.hpp"
#include "scripting/scripts/fly_camera.hpp"

namespace goma {

Engine::Engine()
    : platform_(std::make_unique<Win32Platform>()),
      input_system_(std::make_unique<InputSystem>(*platform_.get())),
      scripting_system_{std::make_unique<ScriptingSystem>(*this)} {
    platform_->InitWindow();
    renderer_ = std::make_unique<Renderer>(*this);
}

result<void> Engine::MainLoop(MainLoopFn inner_loop) {
    if (platform_) {
        platform_->MainLoop([&]() {
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

            input_system_->AcquireFrameInput();
            scripting_system_->Update(delta_time_.count());
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

    OUTCOME_TRY(main_camera, CreateDefaultCamera());
    main_camera_ = main_camera;

    FlyCamera fly_camera(main_camera_, 10.0f);
    scripting_system_->RegisterScript(std::move(fly_camera));

    OUTCOME_TRY(renderer_->CreateSkybox());

    return outcome::success();
}

result<AttachmentIndex<Camera>> Engine::CreateDefaultCamera() {
    Camera camera{};
    camera.aspect_ratio = float(platform_->GetWidth()) / platform_->GetHeight();

    auto new_camera_res = scene_->CreateAttachment<Camera>(std::move(camera));
    if (!new_camera_res) {
        return Error::NoMainCamera;
    }

    auto new_camera_id = new_camera_res.value();
    // Create a node for the new camera
    auto camera_node = scene_->CreateNode(scene_->GetRootNode()).value();
    scene_->Attach<Camera>(new_camera_id, camera_node);

    return new_camera_id;
}

result<AttachmentIndex<Light>> Engine::CreateDefaultLight() {
    Light light{"default_light"};

    OUTCOME_TRY(light_id, scene_->CreateAttachment<Light>(std::move(light)));

    // Create a node for the new light
    OUTCOME_TRY(light_node, scene_->CreateNode(scene_->GetRootNode()));
    scene_->Attach<Light>(light_id, light_node);

    return light_id;
}

}  // namespace goma
