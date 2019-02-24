#include "renderer/renderer.hpp"

#include "engine.hpp"
#include "renderer/vez/vez_backend.hpp"
#include "scene/attachments/mesh.hpp"

#define LOG(prefix, format, ...) printf(prefix format "\n", __VA_ARGS__)
#define LOGE(format, ...) LOG("** ERROR: ", format, __VA_ARGS__)
#define LOGW(format, ...) LOG("* Warning: ", format, __VA_ARGS__)
#define LOGI(format, ...) LOG("", format, __VA_ARGS__)

namespace goma {

Renderer::Renderer(Engine* engine)
    : engine_(engine), backend_(std::make_unique<VezBackend>(engine)) {
    if (auto result = backend_->InitContext()) {
        LOGI("Context initialized.");
    } else {
        LOGE("%s", result.error().message().c_str());
    }
}

result<void> Renderer::Render() {
    Scene* scene = engine_->scene();
    if (!scene) {
        return Error::NoSceneLoaded;
    }

    backend_->SetupFrames(3);

    // TODO culling
    // TODO ordering

    // Ensure that all meshes have their own buffers
    scene->ForEach<Mesh>([&](auto id, auto _, Mesh& mesh) {
		// Create vertex buffer
        if (!mesh.vertices.empty()) {
            auto vb_result = backend_->CreateVertexBuffer(
                id, "vertex", mesh.vertices.size() * sizeof(mesh.vertices[0]),
                true, mesh.vertices.data());

            if (vb_result) {
                auto& vb = vb_result.value();
                mesh.buffers.vertex = vb;
            }
        }
    });

    return outcome::success();
}

}  // namespace goma
