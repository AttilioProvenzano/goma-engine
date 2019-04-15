#include "engine.hpp"

#include "platform/win32_platform.hpp"
#include "scene/loaders/assimp_loader.hpp"
#include "scripting/scripts/fly_camera.hpp"

namespace goma {

Engine::Engine()
    : platform_(std::make_unique<Win32Platform>()),
      input_system_(std::make_unique<InputSystem>(*platform_.get())) {
    platform_->InitWindow();
    scripting_system_ = std::make_unique<ScriptingSystem>(*this);
    renderer_ = std::make_unique<Renderer>(this);
}

result<void> Engine::MainLoop(MainLoopFn inner_loop) {
    if (platform_) {
        platform_->MainLoop([&]() {
            input_system_->AcquireFrameInput();
            scripting_system_->Update(0.016f);  // TODO delta_time
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

    FlyCamera fly_camera(main_camera_, 100.0f);
    scripting_system_->RegisterScript(std::move(fly_camera));

    OUTCOME_TRY(renderer_->CreateSkybox());

    return outcome::success();
}

result<AttachmentIndex<Camera>> Engine::CreateDefaultCamera() {
    Camera camera = {};
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

}  // namespace goma
