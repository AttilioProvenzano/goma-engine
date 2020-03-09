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
        if (texture.image) {
            return;
        }

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

        texture.image = image;
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

    GraphicsContext graphics_ctx(*device_);
    OUTCOME_TRY(graphics_ctx.Begin());

    OUTCOME_TRY(swapchain_image, device_->AcquireSwapchainImage());
    OUTCOME_TRY(
        graphics_ctx.BindFramebuffer(FramebufferDesc{{{swapchain_image}}}));

    RenderMeshes(graphics_ctx, scene);

    graphics_ctx.End();
    OUTCOME_TRY(graphics_receipt, device_->Submit(graphics_ctx));
    OUTCOME_TRY(device_->Present());

    // TODO: store the graphics ctx and wait for previous receipt
    OUTCOME_TRY(device_->WaitOnWork(std::move(graphics_receipt)));

    return outcome::success();
}

void Renderer::RenderMeshes(GraphicsContext& ctx, Scene& scene) {
    auto& device = *device_;
    auto& platform = engine_.platform();

    static struct {
        std::string vtx_code;
        std::string frag_code;
    } ro;  // rendering objects

    static const auto vtx_path = GOMA_ASSETS_DIR "shaders/base.vert";
    static const auto frag_path = GOMA_ASSETS_DIR "shaders/base.frag";

    if (ro.vtx_code.empty()) {
        auto vtx_code = platform.ReadFileToString(vtx_path);
        if (!vtx_code) {
            spdlog::error("Could not read shader code from \"{}\"", vtx_path);
            return;
        }
        ro.vtx_code = std::move(vtx_code.value());
    }
    if (ro.frag_code.empty()) {
        auto frag_code = platform.ReadFileToString(frag_path);
        if (!frag_code) {
            spdlog::error("Could not read shader code from \"{}\"", frag_path);
            return;
        }
        ro.frag_code = std::move(frag_code.value());
    }

    scene.ForEach<Mesh>([&](auto id, auto, Mesh& mesh) {
        std::stringstream preamble;
        std::transform(
            mesh.vertices.layout.begin(), mesh.vertices.layout.end(),
            std::ostream_iterator<const char*>(preamble, "\n"),
            [](const auto& a) {
                using PreambleMap =
                    std::unordered_map<VertexAttribute, const char*>;
                static const PreambleMap preamble_map = {
                    {VertexAttribute::Position, "#define HAS_POSITIONS"},
                    {VertexAttribute::Normal, "#define HAS_NORMALS"},
                    {VertexAttribute::Tangent, "#define HAS_TANGENTS"},
                    {VertexAttribute::Bitangent, "#define HAS_BITANGENTS"},
                    {VertexAttribute::Color, "#define HAS_COLORS"},
                    {VertexAttribute::UV0, "#define HAS_UV0"},
                    {VertexAttribute::UV0, "#define HAS_UV1"},
                };
                return preamble_map.at(a);
            });

        auto vtx_desc = ShaderDesc{vtx_path, VK_SHADER_STAGE_VERTEX_BIT, "",
                                   preamble.str()};

        auto vtx_shader = shader_map.find(vtx_desc);
        if (vtx_shader == shader_map.end()) {
            vtx_desc.source = ro.vtx_code;

            auto vtx_res = device_->CreateShader(vtx_desc);
            if (!vtx_res) {
                spdlog::error("Could not create vertex shader \"{}\"",
                              vtx_path);
                return;
            }

            shader_map[vtx_desc] = vtx_res.value();
            vtx_shader = shader_map.find(vtx_desc);
        }

        auto frag_desc = ShaderDesc{frag_path, VK_SHADER_STAGE_FRAGMENT_BIT};

        auto frag_shader = shader_map.find(frag_desc);
        if (frag_shader == shader_map.end()) {
            frag_desc.source = ro.frag_code;

            auto frag_res = device_->CreateShader(frag_desc);
            if (!frag_res) {
                spdlog::error("Could not create fragment shader \"{}\"",
                              frag_path);
                return;
            }

            shader_map[frag_desc] = frag_res.value();
            frag_shader = shader_map.find(frag_desc);
        }

        // TODO: material too (also in error message)
        auto pipeline_res = device_->GetPipeline(PipelineDesc{
            {vtx_shader->second, frag_shader->second}, ctx.GetFramebuffer()});
        if (!pipeline_res) {
            spdlog::error("Could not get pipeline for mesh \"{}\"", mesh.name);
            return;
        }
    });
}

}  // namespace goma
