#include "renderer/renderer.hpp"

#include "engine/engine.hpp"
#include "rhi/context.hpp"
#include "rhi/utils.hpp"
#include "scene/attachments/material.hpp"
#include "scene/attachments/mesh.hpp"
#include "scene/utils.hpp"

namespace goma {

Renderer::Renderer(Engine& engine)
    : engine_(engine), device_(), graphics_ctx_(device_), upload_ctx_(device_) {
    auto res = device_.InitWindow(engine_.platform());
    if (res.has_error()) {
        throw std::runtime_error(res.error().message());
    }
}

namespace {

void CreateMeshBuffers(Device& device, UploadContext& ctx, Scene& scene) {
    scene.ForEach<Mesh>([&](auto id, auto&, Mesh& mesh) {
        if (mesh.rhi.valid) {
            return;
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

            auto vtx_buf_res = device.CreateBuffer(vtx_buf_desc);
            if (!vtx_buf_res) {
                spdlog::error("Vertex buffer creation failed for mesh \"{}\"",
                              mesh.name);
                return;
            }

            mesh.rhi.vertex_buffer = vtx_buf_res.value();

            if (!ctx.UploadBuffer(
                    *vtx_buf_res.value(),
                    {vtx_data.size() * sizeof(vtx_data[0]), vtx_data.data()})) {
                spdlog::error("Vertex buffer upload failed for mesh \"{}\"",
                              mesh.name);
                return;
            }
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

            auto idx_buf_res = device.CreateBuffer(idx_buf_desc);
            if (!idx_buf_res) {
                spdlog::error("Index buffer creation failed for mesh \"{}\"",
                              mesh.name);
                return;
            }

            mesh.rhi.index_buffer = idx_buf_res.value();

            if (!ctx.UploadBuffer(
                    *idx_buf_res.value(),
                    {idx_data.size() * sizeof(idx_data[0]), idx_data.data()})) {
                spdlog::error("Index buffer upload failed for mesh \"{}\"",
                              mesh.name);
                return;
            }
        }

        mesh.rhi.valid = true;
    });
}

void UploadTextures(Device& device, UploadContext& ctx, Scene& scene) {
    scene.ForEach<Texture>([&](auto id, auto&, Texture& texture) {
        if (texture.rhi.valid) {
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

        texture.rhi.image = image;
        texture.rhi.valid = true;
    });
}

void BindMaterialTextures(Scene& scene) {
    scene.ForEach<Material>([&scene](auto id, auto&, Material& material) {
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

            auto& tex_index = tex_binding->second[0].index;

            // TODO: outcome_try (or new signature)
            auto& tex = scene.GetAttachment<Texture>(tex_index).value().get();
            if (tex.rhi.valid) {
                material.rhi.*(mt.second) = tex.rhi.image;
            }
        }

        material.rhi.valid = true;
    });
}

}  // namespace

result<void> Renderer::Render() {
    if (!engine_.scene()) {
        return Error::NoSceneLoaded;
    }
    auto& scene = *engine_.scene();

    OUTCOME_TRY(upload_ctx_.Begin());

    // Ensure that all meshes have their own buffers
    CreateMeshBuffers(device_, upload_ctx_, scene);

    // Upload any missing textures
    UploadTextures(device_, upload_ctx_, scene);

    // Bind texture handles to materials for efficient retrieval
    BindMaterialTextures(scene);

    upload_ctx_.End();

    // TODO: avoid unnecessary upload submission when possible
    //       could be achieved via implicit Begin() and End()
    OUTCOME_TRY(receipt, device_.Submit(upload_ctx_));

    // TODO: wait for previous receipt in ring buffer
    OUTCOME_TRY(device_.WaitOnWork(std::move(receipt)));

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

    // TODO: Alternate constructors to make it more obvious
    auto fb_desc = FramebufferDesc{{{swapchain_image}}};
    fb_desc.depth_attachment.image = depth_image;
    OUTCOME_TRY(graphics_ctx_.BindFramebuffer(fb_desc));

    OUTCOME_TRY(RenderMeshes(graphics_ctx_, scene));

    graphics_ctx_.End();
    OUTCOME_TRY(graphics_receipt, device_.Submit(graphics_ctx_));
    OUTCOME_TRY(device_.Present());

    // TODO: Wait for previous receipt
    OUTCOME_TRY(device_.WaitOnWork(std::move(graphics_receipt)));

    current_frame_++;
    frame_index_ = (frame_index_ + 1) % kMaxFramesInFlight;

    return outcome::success();
}

result<void> Renderer::RenderMeshes(GraphicsContext& ctx, Scene& scene) {
    auto& device = device_;
    auto& platform = engine_.platform();

    static struct {
        std::string vtx_code;
        std::string frag_code;
        std::vector<Buffer*> mvp_buffer;
        Sampler* base_sampler = nullptr;
    } ro;  // rendering objects

    if (ro.mvp_buffer.empty()) {
        ro.mvp_buffer.resize(kMaxFramesInFlight);
    }

    if (!ro.base_sampler) {
        OUTCOME_TRY(sampler, device_.CreateSampler({}));
        ro.base_sampler = sampler;
    }

    static const auto vtx_path = GOMA_ASSETS_DIR "shaders/base.vert";
    static const auto frag_path = GOMA_ASSETS_DIR "shaders/base.frag";

    if (ro.vtx_code.empty()) {
        OUTCOME_TRY(vtx_code, platform.ReadFileToString(vtx_path));
        ro.vtx_code = std::move(vtx_code);
    }
    if (ro.frag_code.empty()) {
        OUTCOME_TRY(frag_code, platform.ReadFileToString(frag_path));
        ro.frag_code = std::move(frag_code);
    }

    if (!ro.mvp_buffer[frame_index_]) {
        BufferDesc desc = {};
        desc.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        desc.num_elements = 256;
        desc.stride = 256;
        desc.size = 256 * 256;
        desc.storage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        OUTCOME_TRY(buffer, device_.CreateBuffer(desc));

        ro.mvp_buffer[frame_index_] = buffer;
    }

    scene.ForEach<Mesh>([&](auto mesh_id, auto& nodes, Mesh& mesh) {
        // TODO: make it more compact (no ref wrapper)
        auto& material =
            scene.GetAttachment<Material>(mesh.material).value().get();

        if (material.rhi.preamble.empty()) {
            std::stringstream preamble;

            if (material.rhi.diffuse_tex) {
                preamble << "#define HAS_DIFFUSE_TEX\n";
            }
            if (material.rhi.normal_tex) {
                preamble << "#define HAS_NORMAL_TEX\n";
            }
            if (material.rhi.metallic_roughness_tex) {
                preamble << "#define HAS_METALLIC_ROUGHNESS_TEX\n";
            }
            if (material.rhi.ambient_tex) {
                preamble << "#define HAS_AMBIENT_TEX\n";
            }
            if (material.rhi.emissive_tex) {
                preamble << "#define HAS_EMISSIVE_TEX\n";
            }

            material.rhi.preamble = preamble.str();
        }

        if (mesh.rhi.preamble.empty()) {
            using MeshPreambleMap =
                std::unordered_map<VertexAttribute, const char*>;
            static const MeshPreambleMap mesh_preamble_map = {
                {VertexAttribute::Position, "#define HAS_POSITIONS"},
                {VertexAttribute::Normal, "#define HAS_NORMALS"},
                {VertexAttribute::Tangent, "#define HAS_TANGENTS"},
                {VertexAttribute::Bitangent, "#define HAS_BITANGENTS"},
                {VertexAttribute::Color, "#define HAS_COLORS"},
                {VertexAttribute::UV0, "#define HAS_UV0"},
                {VertexAttribute::UV0, "#define HAS_UV1"},
            };

            std::stringstream preamble;
            std::transform(
                mesh.vertices.layout.begin(), mesh.vertices.layout.end(),
                std::ostream_iterator<const char*>(preamble, "\n"),
                [](VertexAttribute a) { return mesh_preamble_map.at(a); });

            mesh.rhi.preamble = preamble.str();
        }

        // TODO: ShaderDesc only needs references to source and preamble, shader
        //       will store hashed versions
        auto vtx_desc = ShaderDesc{vtx_path, VK_SHADER_STAGE_VERTEX_BIT, "",
                                   mesh.rhi.preamble};

        auto vtx_shader = shader_map_.find(vtx_desc);
        if (vtx_shader == shader_map_.end()) {
            vtx_desc.source = ro.vtx_code;

            auto vtx_res = device_.CreateShader(vtx_desc);
            if (!vtx_res) {
                spdlog::error("Could not create vertex shader \"{}\"",
                              vtx_path);
                return;
            }

            shader_map_[vtx_desc] = vtx_res.value();
            vtx_shader = shader_map_.find(vtx_desc);
        }

        // TODO: Convenience types, MeshIndex, AttachmentIndex etc.
        using MeshMaterialPair =
            std::pair<AttachmentIndex<Mesh>, AttachmentIndex<Material>>;

        struct MeshMaterialHash {
            size_t operator()(const MeshMaterialPair& mm) const {
                size_t seed = 0;

                goma::hash_combine(seed, mm.first);
                goma::hash_combine(seed, mm.second);

                return seed;
            };
        };

        using CombinedPreambleMap =
            std::unordered_map<MeshMaterialPair, std::string, MeshMaterialHash>;

        static CombinedPreambleMap comb_preamble_map;

        auto mesh_mat_pair = std::make_pair(mesh_id, mesh.material);
        auto comb_preamble = comb_preamble_map.find(mesh_mat_pair);

        if (comb_preamble == comb_preamble_map.end()) {
            auto cp = mesh.rhi.preamble + material.rhi.preamble;
            comb_preamble_map[mesh_mat_pair] = std::move(cp);

            comb_preamble = comb_preamble_map.find(mesh_mat_pair);
        }

        auto frag_desc = ShaderDesc{frag_path, VK_SHADER_STAGE_FRAGMENT_BIT, "",
                                    comb_preamble->second};

        auto frag_shader = shader_map_.find(frag_desc);
        if (frag_shader == shader_map_.end()) {
            frag_desc.source = ro.frag_code;

            auto frag_res = device_.CreateShader(frag_desc);
            if (!frag_res) {
                spdlog::error("Could not create fragment shader \"{}\"",
                              frag_path);
                return;
            }

            shader_map_[frag_desc] = frag_res.value();
            frag_shader = shader_map_.find(frag_desc);
        }

        auto pipeline_desc = PipelineDesc{
            {vtx_shader->second, frag_shader->second}, ctx.GetFramebuffer()};
        pipeline_desc.cull_mode = VK_CULL_MODE_BACK_BIT;
        pipeline_desc.depth_test = true;

        auto pipeline = device_.GetPipeline(std::move(pipeline_desc)).value();

        float rot_speed = 0.2f;
        float rot_angle = glm::radians(rot_speed * current_frame_);

        auto eye = glm::vec3(35.0f * glm::sin(rot_angle), 0.0f,
                             -35.0f * glm::cos(rot_angle));
        auto center = glm::vec3(0.0f, 10.0f, 0.0f);
        auto up = glm::vec3{0.0f, -1.0f, 0.0f};

        // TODO: for each node! outcome_try and no ref_wrapper
        // TODO: keep pointers back to scene and call functions like
        //       node->GetTransformMatrix
        // TODO: std::set inconvenient for nodes, maybe ordered vector?

        auto mvp = glm::perspective(glm::radians(60.0f),
                                    static_cast<float>(platform.GetWidth()) /
                                        platform.GetHeight(),
                                    0.1f, 100.0f) *
                   glm::lookAt(eye, center, up) *
                   scene.GetTransformMatrix(*nodes.begin())
                       .value();  // TODO: outcome_try

        auto& mvp_buf = *ro.mvp_buffer[frame_index_];

        // TODO: really need outcome_try!!
        // TODO: get alignment
        // Also MapBuffer could map as uint8_t* for convenience
        auto mvp_data = device.MapBuffer(mvp_buf).value();
        memcpy(static_cast<uint8_t*>(mvp_data) + 256 * mesh_id.id, &mvp,
               sizeof(mvp));
        device.UnmapBuffer(mvp_buf);

        ctx.BindGraphicsPipeline(*pipeline);

        DescriptorSet ds;
        ds[0] = {mvp_buf, static_cast<uint32_t>(256 * mesh_id.id), sizeof(mvp)};
        ds[1] = {*material.rhi.diffuse_tex, *ro.base_sampler};

        ctx.BindDescriptorSet(ds);

        ctx.BindVertexBuffer(*mesh.rhi.vertex_buffer);

        if (mesh.rhi.index_buffer) {
            ctx.BindIndexBuffer(*mesh.rhi.index_buffer);
            ctx.DrawIndexed(static_cast<uint32_t>(mesh.indices.size()));
        } else {
            ctx.Draw(mesh.vertices.size);
        }
    });

    return outcome::success();
}

}  // namespace goma
