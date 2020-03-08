#include "renderer/renderer.hpp"

#include "engine/engine.hpp"
#include "rhi/context.hpp"
#include "scene/attachments/mesh.hpp"

namespace goma {

Renderer::Renderer(Engine& engine)
    : engine_(engine), device_(std::make_unique<Device>()) {
    auto res = device_->InitWindow(engine_.platform());
    if (res.has_error()) {
        throw std::runtime_error(res.error().message());
    }
}

namespace {

void CreateMeshBuffers(Device& device, UploadContext& ctx, Scene& scene) {
    scene.ForEach<Mesh>([&](auto id, auto, Mesh& mesh) {});
}

void UploadTextures(Device& device, UploadContext& ctx, Scene& scene) {
    scene.ForEach<Texture>([&](auto id, auto, Texture& texture) {});
}

}  // namespace

result<void> Renderer::Render() {
    if (!engine_.scene()) {
        // TODO: might do more in this case, e.g. GUI
        return Error::NoSceneLoaded;
    }
    auto& scene = *engine_.scene();

    UploadContext ctx(*device_);

    // Ensure that all meshes have their own buffers
    CreateMeshBuffers(*device_, ctx, scene);

    // Upload any missing textures
    UploadTextures(*device_, ctx, scene);

    return outcome::success();
}

}  // namespace goma
