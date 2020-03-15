#include "renderer/rendering_pipeline.hpp"

#include "engine/engine.hpp"
#include "renderer/renderer.hpp"
#include "rhi/context.hpp"
#include "scene/attachments/mesh.hpp"

namespace goma {

result<void> RenderingPipeline::render_meshes(GfxCtx& ctx, MeshIter first,
                                              MeshIter last) {
    auto& device = renderer_.device();
    auto& platform = renderer_.engine().platform();

    if (!renderer_.engine().scene()) {
        return Error::NoSceneLoaded;
    }
    auto& scene = *renderer_.engine().scene();

    static struct {
        std::string vtx_code;
        std::string frag_code;
        std::vector<Buffer*> mvp_buffer;
        Sampler* base_sampler = nullptr;
    } ro;  // rendering objects

    if (ro.mvp_buffer.empty()) {
        ro.mvp_buffer.resize(renderer_.max_frames_in_flight());
    }

    if (!ro.base_sampler) {
        OUTCOME_TRY(sampler, device.CreateSampler({}));
        ro.base_sampler = sampler;
    }

    static const auto vtx_path = GOMA_ASSETS_DIR "shaders/base.vert";
    static const auto frag_path = GOMA_ASSETS_DIR "shaders/base.frag";

    if (ro.vtx_code.empty()) {
        OUTCOME_TRY(vtx_code, platform.ReadFile(vtx_path));
        ro.vtx_code = std::move(vtx_code);
    }
    if (ro.frag_code.empty()) {
        OUTCOME_TRY(frag_code, platform.ReadFile(frag_path));
        ro.frag_code = std::move(frag_code);
    }

    // Ensure that buffer alignment is a multiple of minBufferAlignment
    auto mask = device.GetMinBufferAlignment() - 1;
    auto buf_alignment =
        static_cast<uint32_t>((sizeof(glm::mat4) + mask) & ~mask);

    if (!ro.mvp_buffer[renderer_.frame_index()]) {
        BufferDesc desc = {};
        desc.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        desc.num_elements = 256;
        desc.stride = buf_alignment;
        desc.size = 256 * buf_alignment;
        desc.storage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        OUTCOME_TRY(buffer, device.CreateBuffer(desc));

        ro.mvp_buffer[renderer_.frame_index()] = buffer;
    }

    uint32_t mesh_count = 0;
    for (auto mesh_it = first; mesh_it != last; ++mesh_it) {
        auto& mesh = *mesh_it;

        if (!scene.materials().is_valid(mesh.material_id)) {
            spdlog::error(
                "Mesh \"{}\" references an invalid material, skipping.",
                mesh.name);
            continue;
        }
        auto& material = scene.materials().at(mesh.material_id);

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

        auto vtx_desc = ShaderDesc{vtx_path, VK_SHADER_STAGE_VERTEX_BIT, "",
                                   mesh.rhi.preamble};

        using MeshMaterialPair = std::pair<std::string, gen_id>;

        struct MeshMaterialHash {
            size_t operator()(const MeshMaterialPair& mm) const {
                size_t seed = 0;
                goma::hash_combine(seed, djb2_hash(mm.first.c_str()));
                goma::hash_combine(seed, mm.second);
                return seed;
            };
        };

        using CombinedPreambleMap =
            std::unordered_map<MeshMaterialPair, std::string, MeshMaterialHash>;

        static CombinedPreambleMap comb_preamble_map;

        auto mesh_mat_pair = std::make_pair(mesh.name, mesh.material_id);
        auto comb_preamble = comb_preamble_map.find(mesh_mat_pair);

        if (comb_preamble == comb_preamble_map.end()) {
            auto cp = mesh.rhi.preamble + material.rhi.preamble;
            comb_preamble_map[mesh_mat_pair] = std::move(cp);

            comb_preamble = comb_preamble_map.find(mesh_mat_pair);
        }

        auto frag_desc = ShaderDesc{frag_path, VK_SHADER_STAGE_FRAGMENT_BIT, "",
                                    comb_preamble->second};

        auto create_shader_async =
            [this, &device](int id, const ShaderDesc& desc) -> Shader* {
            auto v = device.CreateShader(desc);
            shader_map_[desc] = v.value();
            return v.value();
        };

        struct {
            Shader* vtx = nullptr;
            std::future<Shader*> vtx_fut;

            Shader* frag = nullptr;
            std::future<Shader*> frag_fut;
        } shaders;

        auto vtx_it = shader_map_.find(vtx_desc);
        if (vtx_it == shader_map_.end()) {
            vtx_desc.source = ro.vtx_code;
            shaders.vtx_fut =
                renderer_.thread_pool().push(create_shader_async, vtx_desc);
        } else {
            shaders.vtx = vtx_it->second;
        }

        auto frag_it = shader_map_.find(frag_desc);
        if (frag_it == shader_map_.end()) {
            frag_desc.source = ro.frag_code;
            shaders.frag_fut =
                renderer_.thread_pool().push(create_shader_async, frag_desc);
        } else {
            shaders.frag = frag_it->second;
        }

        if (!shaders.vtx) {
            shaders.vtx = shaders.vtx_fut.get();
        }
        if (!shaders.frag) {
            shaders.frag = shaders.frag_fut.get();
        }

        auto pipeline_desc =
            PipelineDesc{{shaders.vtx, shaders.frag}, ctx.GetFramebuffer()};
        pipeline_desc.cull_mode = VK_CULL_MODE_BACK_BIT;
        pipeline_desc.depth_test = true;

        OUTCOME_TRY(pipeline, device.GetPipeline(std::move(pipeline_desc)));

        float rot_speed = 0.2f;
        float rot_angle = glm::radians(rot_speed * renderer_.current_frame());

        auto eye = glm::vec3(35.0f * glm::sin(rot_angle), 0.0f,
                             -35.0f * glm::cos(rot_angle));
        auto center = glm::vec3(0.0f, 10.0f, 0.0f);
        auto up = glm::vec3{0.0f, -1.0f, 0.0f};

        auto mvp = glm::perspective(glm::radians(60.0f),
                                    static_cast<float>(platform.GetWidth()) /
                                        platform.GetHeight(),
                                    0.1f, 100.0f) *
                   glm::lookAt(eye, center, up) *
                   mesh.attached_nodes()[0]->get_transform_matrix();

        auto& mvp_buf = *ro.mvp_buffer[renderer_.frame_index()];

        OUTCOME_TRY(mvp_data, device.MapBuffer(mvp_buf));
        memcpy(mvp_data + buf_alignment * mesh_count, &mvp, sizeof(mvp));
        device.UnmapBuffer(mvp_buf);

        ctx.BindGraphicsPipeline(*pipeline);

        DescriptorSet ds;
        ds[0] = {mvp_buf, static_cast<uint32_t>(buf_alignment * mesh_count),
                 sizeof(mvp)};
        ds[1] = {*material.rhi.diffuse_tex, *ro.base_sampler};

        ctx.BindDescriptorSet(ds);

        ctx.BindVertexBuffer(*mesh.rhi.vertex_buffer);

        if (mesh.rhi.index_buffer) {
            ctx.BindIndexBuffer(*mesh.rhi.index_buffer);
            ctx.DrawIndexed(static_cast<uint32_t>(mesh.indices.size()));
        } else {
            ctx.Draw(mesh.vertices.size);
        }

        ++mesh_count;
    }

    return outcome::success();
}

}  // namespace goma
