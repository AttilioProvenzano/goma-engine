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
        if (!mesh.vertices.empty() &&
            (!mesh.buffers.vertex || !mesh.buffers.vertex->valid)) {
            auto vb_result = backend_->CreateVertexBuffer(
                id, "vertex", mesh.vertices.size() * sizeof(mesh.vertices[0]),
                true, mesh.vertices.data());

            if (vb_result) {
                auto& vb = vb_result.value();
                mesh.buffers.vertex = vb;
            }
        }

        // Create normal buffer
        if (!mesh.normals.empty() &&
            (!mesh.buffers.normal || !mesh.buffers.normal->valid)) {
            auto vb_result = backend_->CreateVertexBuffer(
                id, "normal", mesh.normals.size() * sizeof(mesh.normals[0]),
                true, mesh.normals.data());

            if (vb_result) {
                auto& vb = vb_result.value();
                mesh.buffers.normal = vb;
            }
        }

        // Create tangent buffer
        if (!mesh.tangents.empty() &&
            (!mesh.buffers.tangent || !mesh.buffers.tangent->valid)) {
            auto vb_result = backend_->CreateVertexBuffer(
                id, "tangent", mesh.tangents.size() * sizeof(mesh.tangents[0]),
                true, mesh.tangents.data());

            if (vb_result) {
                auto& vb = vb_result.value();
                mesh.buffers.tangent = vb;
            }
        }

        // Create bitangent buffer
        if (!mesh.bitangents.empty() &&
            (!mesh.buffers.bitangent || !mesh.buffers.bitangent->valid)) {
            auto vb_result = backend_->CreateVertexBuffer(
                id, "bitangent",
                mesh.bitangents.size() * sizeof(mesh.bitangents[0]), true,
                mesh.bitangents.data());

            if (vb_result) {
                auto& vb = vb_result.value();
                mesh.buffers.bitangent = vb;
            }
        }

        // Create index buffer
        if (!mesh.indices.empty() &&
            (!mesh.buffers.index || !mesh.buffers.index->valid)) {
            auto ib_result = backend_->CreateIndexBuffer(
                id, "index", mesh.indices.size() * sizeof(mesh.indices[0]),
                true, mesh.indices.data());

            if (ib_result) {
                auto& ib = ib_result.value();
                mesh.buffers.index = ib;
            }
        }

        // Create color buffer
        if (!mesh.colors.empty() &&
            (!mesh.buffers.color || !mesh.buffers.color->valid)) {
            auto vb_result = backend_->CreateVertexBuffer(
                id, "color", mesh.colors.size() * sizeof(mesh.colors[0]), true,
                mesh.colors.data());

            if (vb_result) {
                auto& vb = vb_result.value();
                mesh.buffers.color = vb;
            }
        }

        mesh.buffers.uv.resize(mesh.uv_sets.size());
        for (size_t i = 0; i < mesh.uv_sets.size(); i++) {
            auto& uv_set = mesh.uv_sets[i];

            if (!mesh.buffers.uv[i] || !mesh.buffers.uv[i]->valid) {
                // Create UV buffer
                auto vb_result = backend_->CreateVertexBuffer(
                    id, "uv" + i, uv_set.size() * sizeof(uv_set[0]), true,
                    uv_set.data());

                if (vb_result) {
                    auto& vb = vb_result.value();
                    mesh.buffers.uv[i] = vb;
                }
            }
        }

        mesh.buffers.uvw.resize(mesh.uvw_sets.size());
        for (size_t i = 0; i < mesh.uvw_sets.size(); i++) {
            auto& uvw_set = mesh.uvw_sets[i];

            if (!mesh.buffers.uvw[i] || !mesh.buffers.uvw[i]->valid) {
                // Create UV buffer
                auto vb_result = backend_->CreateVertexBuffer(
                    id, "uvw" + i, uvw_set.size() * sizeof(uvw_set[0]), true,
                    uvw_set.data());

                if (vb_result) {
                    auto& vb = vb_result.value();
                    mesh.buffers.uvw[i] = vb;
                }
            }
        }
    });

    return outcome::success();
}

}  // namespace goma
