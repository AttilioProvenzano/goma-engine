#include "renderer/renderer.hpp"

#include "common/hash.hpp"
#include "engine/engine.hpp"
#include "renderer/pipelines/forward_pipeline.hpp"
#include "rhi/context.hpp"
#include "rhi/utils.hpp"
#include "scene/attachments/material.hpp"
#include "scene/attachments/mesh.hpp"
#include "scene/utils.hpp"

namespace goma {

Renderer::Renderer(Engine& engine)
    : engine_(engine),
      device_(),
      graphics_ctx_(device_),
      upload_ctx_(device_),
      thread_pool_(kNumThreads) {
    auto res = device_.InitWindow(engine_.platform());
    if (res.has_error()) {
        throw std::runtime_error(res.error().message());
    }

    rp_ = std::make_unique<ForwardPipeline>(*this);

    frame_receipts_.resize(kMaxFramesInFlight);
}

Renderer::~Renderer() {
    // Wait for any outstanding work
    for (auto& fr : frame_receipts_) {
        for (auto& r : fr) {
            auto _ = device_.WaitOnWork(std::move(r));
        }
    }
}

namespace {

result<void> CreateMeshBuffers(Device& device, UploadContext& ctx,
                               Scene& scene) {
    for (auto& mesh : scene.meshes()) {
        if (mesh.rhi.valid) {
            continue;
        }

        if (!mesh.vertices.data.empty()) {
            // Upload vertex buffer
            auto& vtx_data = mesh.vertices.data;

            BufferDesc vtx_buf_desc = {};
            vtx_buf_desc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            vtx_buf_desc.num_elements = mesh.vertices.size;
            vtx_buf_desc.stride =
                static_cast<uint32_t>(utils::GetStride(mesh.vertices.layout));
            vtx_buf_desc.size = vtx_data.size() * sizeof(vtx_data[0]);
            vtx_buf_desc.storage = VMA_MEMORY_USAGE_GPU_ONLY;

            OUTCOME_TRY(vtx_buf, device.CreateBuffer(vtx_buf_desc));
            mesh.rhi.vertex_buffer = vtx_buf;

            OUTCOME_TRY(ctx.UploadBuffer(
                *vtx_buf,
                {vtx_data.size() * sizeof(vtx_data[0]), vtx_data.data()}));
        }

        if (!mesh.indices.empty()) {
            // Upload index buffer
            auto& idx_data = mesh.indices;

            BufferDesc idx_buf_desc = {};
            idx_buf_desc.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            idx_buf_desc.num_elements = idx_data.size();
            idx_buf_desc.stride = sizeof(idx_data[0]);
            idx_buf_desc.size = idx_data.size() * sizeof(idx_data[0]);
            idx_buf_desc.storage = VMA_MEMORY_USAGE_GPU_ONLY;

            OUTCOME_TRY(idx_buf, device.CreateBuffer(idx_buf_desc));
            mesh.rhi.index_buffer = idx_buf;

            OUTCOME_TRY(ctx.UploadBuffer(
                *idx_buf,
                {idx_data.size() * sizeof(idx_data[0]), idx_data.data()}));
        }

        mesh.rhi.valid = true;
    }

    return outcome::success();
}

result<void> UploadTextures(Device& device, UploadContext& ctx, Scene& scene) {
    for (auto& texture : scene.textures()) {
        if (texture.rhi.valid) {
            continue;
        }

        auto desc = ImageDesc::TextureDesc;
        desc.size = {texture.width, texture.height, 1};
        desc.mip_levels = texture.mip_levels;
        desc.format = texture.format;

        ImageMipData mip_data;

        bool uncompressed = utils::GetFormatCompression(texture.format) ==
                            FormatCompression::Uncompressed;
        if (uncompressed) {
            // In the uncompressed case we generate mipmaps
            desc.mip_levels =
                utils::ComputeMipLevels(texture.width, texture.height);

            mip_data.push_back(texture.data.data());
        } else {
            // In the compressed case we fill mip_data with pointers to the
            // preloaded mip levels
            auto format_info = utils::GetFormatInfo(desc.format);
            auto format_block_size = utils::GetFormatBlockSize(desc.format);
            auto format_scale = format_info.size / (format_block_size.width *
                                                    format_block_size.height);

            uint32_t min_offset = 1;
            if (utils::GetFormatCompression(desc.format) ==
                FormatCompression::BC) {
                if (desc.format == VK_FORMAT_BC1_RGB_UNORM_BLOCK ||
                    desc.format == VK_FORMAT_BC1_RGBA_SRGB_BLOCK ||
                    desc.format == VK_FORMAT_BC1_RGBA_UNORM_BLOCK) {
                    min_offset = 8;
                } else {
                    min_offset = 16;
                }
            }

            std::vector<size_t> offsets(desc.mip_levels);
            std::iota(offsets.begin() + 1, offsets.end(), 0);
            std::transform(offsets.begin() + 1, offsets.end(),
                           offsets.begin() + 1,
                           [&texture, format_scale, min_offset](size_t mip) {
                               uint32_t size = (texture.width >> mip) *
                                               (texture.height >> mip);
                               return std::max(min_offset, size * format_scale);
                           });
            std::partial_sum(offsets.begin(), offsets.end(), offsets.begin());

            std::transform(offsets.begin(), offsets.end(),
                           std::back_inserter(mip_data),
                           [&texture](size_t offset) {
                               return texture.data.data() + offset;
                           });
        }

        OUTCOME_TRY(image, device.CreateImage(desc));

        OUTCOME_TRY(ctx.UploadImage(*image, std::move(mip_data)));

        if (uncompressed && desc.mip_levels > 1) {
            ctx.GenerateMipmaps(*image);
        }

        texture.rhi.image = image;
        texture.rhi.valid = true;
    }

    return outcome::success();
}

void BindMaterialTextures(Scene& scene) {
    for (auto& material : scene.materials()) {
        if (material.rhi.valid) {
            return;
        }

        using MaterialTexMap =
            std::unordered_map<TextureType, Image* decltype(material.rhi)::*>;
        static const MaterialTexMap material_tex_map = {
            {TextureType::Diffuse, &decltype(material.rhi)::diffuse_tex},
            {TextureType::NormalMap, &decltype(material.rhi)::normal_tex},
            {TextureType::MetallicRoughness,
             &decltype(material.rhi)::metallic_roughness_tex},
            {TextureType::Ambient, &decltype(material.rhi)::ambient_tex},
            {TextureType::Emissive, &decltype(material.rhi)::emissive_tex},
        };

        for (auto& mt : material_tex_map) {
            auto tex_binding = material.texture_bindings.find(mt.first);
            if (tex_binding == material.texture_bindings.end() ||
                tex_binding->second.size() == 0) {
                continue;
            }

            auto& tex_id = tex_binding->second[0].index;
            if (scene.textures().is_valid(tex_id)) {
                auto& tex = scene.textures().at(tex_id);
                if (tex.rhi.valid) {
                    material.rhi.*(mt.second) = tex.rhi.image;
                }
            }
        }

        material.rhi.valid = true;
    }
}

}  // namespace

result<void> Renderer::Render() {
    if (!engine_.scene()) {
        return Error::NoSceneLoaded;
    }
    auto& scene = *engine_.scene();

    for (auto& r : frame_receipts_[frame_index_]) {
        OUTCOME_TRY(device_.WaitOnWork(std::move(r)));
    }

    // This allows for proper destruction of staging buffers
    upload_ctx_.NextFrame();

    if (needs_upload_) {
        OUTCOME_TRY(upload_ctx_.Begin());

        // Ensure that all meshes have their own buffers
        OUTCOME_TRY(CreateMeshBuffers(device_, upload_ctx_, scene));

        // Upload any missing textures
        OUTCOME_TRY(UploadTextures(device_, upload_ctx_, scene));

        // Bind texture handles to materials for efficient retrieval
        BindMaterialTextures(scene);

        upload_ctx_.End();

        OUTCOME_TRY(receipt, device_.Submit(upload_ctx_));
        frame_receipts_[frame_index_].push_back(std::move(receipt));

        needs_upload_ = false;
    }

    graphics_ctx_.NextFrame();
    OUTCOME_TRY(graphics_ctx_.Begin());

    static Image* depth_image = nullptr;
    if (!depth_image) {
        auto depth_desc = ImageDesc::DepthAttachmentDesc;
        depth_desc.size = {engine_.platform().GetWidth(),
                           engine_.platform().GetHeight(), 1};

        OUTCOME_TRY(d, device_.CreateImage(depth_desc));
        depth_image = d;
    }

    OUTCOME_TRY(swapchain_image, device_.AcquireSwapchainImage());

    OUTCOME_TRY(rp_->run(graphics_ctx_,
                         {{"color", swapchain_image}, {"depth", depth_image}}));

    graphics_ctx_.End();

    OUTCOME_TRY(graphics_receipt, device_.Submit(graphics_ctx_));
    frame_receipts_[frame_index_].push_back(std::move(graphics_receipt));

    OUTCOME_TRY(device_.Present());

    current_frame_++;
    frame_index_ = (frame_index_ + 1) % kMaxFramesInFlight;

    return outcome::success();
}

}  // namespace goma
