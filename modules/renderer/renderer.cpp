#include "renderer/renderer.hpp"

#include "engine/engine.hpp"
#include "rhi/context.hpp"
#include "rhi/utils.hpp"
#include "scene/attachments/mesh.hpp"
#include "scene/utils.hpp"

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
    scene.ForEach<Mesh>([&](auto id, auto, Mesh& mesh) {
        if (!mesh.vertex_buffer && !mesh.vertices.data.empty()) {
            // Upload vertex buffer
            auto& vtx_data = mesh.vertices.data;

            BufferDesc vtx_buf_desc = {};
            vtx_buf_desc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            vtx_buf_desc.num_elements = mesh.vertices.size;
            vtx_buf_desc.stride =
                static_cast<uint32_t>(utils::GetStride(mesh.vertices.layout));
            vtx_buf_desc.size = vtx_data.size() * sizeof(vtx_data[0]);
            vtx_buf_desc.storage = VMA_MEMORY_USAGE_GPU_ONLY;

            auto vtx_buf_res = device.CreateBuffer(vtx_buf_desc);
            if (!vtx_buf_res) {
                spdlog::error("Vertex buffer creation failed for mesh \"{}\"",
                              mesh.name);
                return;
            }

            mesh.vertex_buffer = vtx_buf_res.value();

            if (!ctx.UploadBuffer(*vtx_buf_res.value(),
                                  {vtx_data.size(), vtx_data.data()})) {
                spdlog::error("Vertex buffer upload failed for mesh \"{}\"",
                              mesh.name);
                return;
            }
        }

        if (!mesh.index_buffer && !mesh.indices.empty()) {
            // Upload index buffer
            auto& idx_data = mesh.indices;

            BufferDesc idx_buf_desc = {};
            idx_buf_desc.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            idx_buf_desc.num_elements = idx_data.size();
            idx_buf_desc.stride = sizeof(idx_data[0]);
            idx_buf_desc.size = idx_data.size() * sizeof(idx_data[0]);
            idx_buf_desc.storage = VMA_MEMORY_USAGE_GPU_ONLY;

            auto idx_buf_res = device.CreateBuffer(idx_buf_desc);
            if (!idx_buf_res) {
                spdlog::error("Index buffer creation failed for mesh \"{}\"",
                              mesh.name);
                return;
            }

            mesh.index_buffer = idx_buf_res.value();

            if (!ctx.UploadBuffer(*idx_buf_res.value(),
                                  {idx_data.size(), idx_data.data()})) {
                spdlog::error("Index buffer upload failed for mesh \"{}\"",
                              mesh.name);
                return;
            }
        }
    });
}

void UploadTextures(Device& device, UploadContext& ctx, Scene& scene) {
    scene.ForEach<Texture>([&](auto id, auto, Texture& texture) {
        auto desc = ImageDesc::TextureDesc;
        desc.size = {texture.width, texture.height, 1};
        desc.mip_levels =
            utils::ComputeMipLevels(texture.width, texture.height);

        auto image_res = device.CreateImage(desc);
        if (!image_res) {
            spdlog::error("Image creation failed for texture \"{}\"",
                          texture.path);
            return;
        }
        auto& image = image_res.value();

        auto upload_res = ctx.UploadImage(*image, {texture.data.data()});
        if (!upload_res) {
            spdlog::error("Image upload failed for texture \"{}\"",
                          texture.path);
            return;
        }

        if (desc.mip_levels > 1) {
            ctx.GenerateMipmaps(*image);
        }
    });
}

}  // namespace

result<void> Renderer::Render() {
    if (!engine_.scene()) {
        // TODO: might do more in this case, e.g. GUI
        return Error::NoSceneLoaded;
    }
    auto& scene = *engine_.scene();

    UploadContext upload_ctx(*device_);
    OUTCOME_TRY(upload_ctx.Begin());

    // Ensure that all meshes have their own buffers
    CreateMeshBuffers(*device_, upload_ctx, scene);

    // Upload any missing textures
    UploadTextures(*device_, upload_ctx, scene);

    upload_ctx.End();
    OUTCOME_TRY(receipt, device_->Submit(upload_ctx));

    // TODO: store the upload ctx and wait for previous receipt
    OUTCOME_TRY(device_->WaitOnWork(std::move(receipt)));

    return outcome::success();
}

}  // namespace goma
