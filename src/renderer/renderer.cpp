#include "renderer/renderer.hpp"

#include "engine.hpp"
#include "renderer/vez/vez_backend.hpp"
#include "scene/attachments/camera.hpp"
#include "scene/attachments/material.hpp"
#include "scene/attachments/mesh.hpp"

#include <stb_image.h>

namespace goma {

Renderer::Renderer(Engine* engine)
    : engine_(engine), backend_(std::make_unique<VezBackend>(engine)) {
    if (auto result = backend_->InitContext()) {
        spdlog::info("Context initialized.");
    } else {
        spdlog::error(result.error().message());
    }

    if (auto result = backend_->InitSurface(engine_->platform())) {
        spdlog::info("Surface initialized.");
    } else {
        spdlog::error(result.error().message());
    }

    // TODO review multisampling for shadow mapping, remove default render plan
    RenderPlan render_plan{};
    render_plan.render_passes["shadow_pass"] = {
        "shadow_pass", {}, {true, true, true}};
    render_plan.depth_images["shadow_depth"] = {"shadow_depth",
                                                DepthImageType::Depth, 1};
    render_plan.framebuffers["shadow_fb"] = {
        "shadow_fb", 1024.0f,       1024.0f, FramebufferSize::Absolute,
        {},          "shadow_depth"};
    render_plan.render_sequence.push_back(render_plan.render_sequence[0]);
    render_plan.render_sequence[0] = {"shadow_pass", "shadow_fb"};
    backend_->SetRenderPlan(std::move(render_plan));
}

result<void> Renderer::Render() {
    Scene* scene = engine_->scene();
    if (!scene) {
        return Error::NoSceneLoaded;
    }

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

        // Create UV0 buffer
        if (mesh.uv_sets.size() > 0 &&
            (!mesh.buffers.uv0 || !mesh.buffers.uv0->valid)) {
            auto vb_result = backend_->CreateVertexBuffer(
                id, "uv0", mesh.uv_sets[0].size() * sizeof(mesh.uv_sets[0][0]),
                true, mesh.uv_sets[0].data());

            if (vb_result) {
                auto& vb = vb_result.value();
                mesh.buffers.uv0 = vb;
            }
        }

        // Create UV1 buffer
        if (mesh.uv_sets.size() > 1 &&
            (!mesh.buffers.uv1 || !mesh.buffers.uv1->valid)) {
            auto vb_result = backend_->CreateVertexBuffer(
                id, "uv1", mesh.uv_sets[1].size() * sizeof(mesh.uv_sets[1][0]),
                true, mesh.uv_sets[1].data());

            if (vb_result) {
                auto& vb = vb_result.value();
                mesh.buffers.uv1 = vb;
            }
        }

        // Create UVW buffer
        if (mesh.uvw_sets.size() > 0 &&
            (!mesh.buffers.uvw || !mesh.buffers.uvw->valid)) {
            auto vb_result = backend_->CreateVertexBuffer(
                id, "uvw",
                mesh.uvw_sets[0].size() * sizeof(mesh.uvw_sets[0][0]), true,
                mesh.uvw_sets[0].data());

            if (vb_result) {
                auto& vb = vb_result.value();
                mesh.buffers.uvw = vb;
            }
        }
    });

    // Ensure that all meshes have their own vertex input format
    scene->ForEach<Mesh>([&](auto id, auto _, Mesh& mesh) {
        VertexInputFormatDesc input_format_desc;

        uint32_t binding_id = 0;
        if (!mesh.vertices.empty()) {
            input_format_desc.bindings.push_back(
                {binding_id, sizeof(mesh.vertices[0])});
            input_format_desc.attributes.push_back(
                {binding_id, binding_id, Format::SFloatRGB32, 0});
        }

        binding_id++;
        if (!mesh.normals.empty()) {
            input_format_desc.bindings.push_back(
                {binding_id, sizeof(mesh.normals[0])});
            input_format_desc.attributes.push_back(
                {binding_id, binding_id, Format::SFloatRGB32, 0});
        }

        binding_id++;
        if (!mesh.tangents.empty()) {
            input_format_desc.bindings.push_back(
                {binding_id, sizeof(mesh.tangents[0])});
            input_format_desc.attributes.push_back(
                {binding_id, binding_id, Format::SFloatRGB32, 0});
        }

        binding_id++;
        if (!mesh.bitangents.empty()) {
            input_format_desc.bindings.push_back(
                {binding_id, sizeof(mesh.bitangents[0])});
            input_format_desc.attributes.push_back(
                {binding_id, binding_id, Format::SFloatRGB32, 0});
        }

        binding_id++;
        if (!mesh.colors.empty()) {
            input_format_desc.bindings.push_back(
                {binding_id, sizeof(mesh.colors[0])});
            input_format_desc.attributes.push_back(
                {binding_id, binding_id, Format::SFloatRGBA32, 0});
        }

        binding_id++;
        if (mesh.uv_sets.size() > 0 && !mesh.uv_sets[0].empty()) {
            input_format_desc.bindings.push_back(
                {binding_id, sizeof(mesh.uv_sets[0][0])});
            input_format_desc.attributes.push_back(
                {binding_id, binding_id, Format::SFloatRG32, 0});
        }

        binding_id++;
        if (mesh.uv_sets.size() > 1 && !mesh.uv_sets[1].empty()) {
            input_format_desc.bindings.push_back(
                {binding_id, sizeof(mesh.uv_sets[1][0])});
            input_format_desc.attributes.push_back(
                {binding_id, binding_id, Format::SFloatRG32, 0});
        }

        binding_id++;
        if (mesh.uvw_sets.size() > 0 && !mesh.uvw_sets[0].empty()) {
            input_format_desc.bindings.push_back(
                {binding_id, sizeof(mesh.uvw_sets[0][0])});
            input_format_desc.attributes.push_back(
                {binding_id, binding_id, Format::SFloatRGB32, 0});
        }

        auto input_format_res =
            backend_->GetVertexInputFormat(input_format_desc);
        if (input_format_res) {
            mesh.vertex_input_format = input_format_res.value();
        }
    });

    // Ensure that all mesh nodes have world-space model matrices
    scene->ForEach<Mesh>([&](auto id, auto _, Mesh& mesh) {
        auto nodes_result = scene->GetAttachedNodes<Mesh>(id);
        if (!nodes_result || !nodes_result.value()) {
            return;
        }

        for (auto& mesh_node : *nodes_result.value()) {
            if (!scene->HasCachedModel(mesh_node)) {
                scene->ComputeCachedModel(mesh_node);
            }
        }
    });

    // Upload textures
    scene->ForEach<Material>([&](auto id, auto _, Material& material) {
        const std::vector<TextureType> texture_types = {
            TextureType::Diffuse,           TextureType::Specular,
            TextureType::Ambient,           TextureType::Emissive,
            TextureType::MetallicRoughness, TextureType::HeightMap,
            TextureType::NormalMap,         TextureType::Shininess,
            TextureType::Opacity,           TextureType::Displacement,
            TextureType::LightMap,          TextureType::Reflection};

        for (const auto& texture_type : texture_types) {
            auto binding = material.texture_bindings.find(texture_type);

            if (binding != material.texture_bindings.end() &&
                !binding->second.empty()) {
                auto texture_res =
                    scene->GetAttachment<Texture>(binding->second[0].index);
                if (texture_res) {
                    auto& texture = texture_res.value();

                    // Upload texture if necessary
                    if (!texture->image || !texture->image->valid) {
                        // TODO compressed textures
                        if (!texture->compressed) {
                            TextureDesc tex_desc{texture->width,
                                                 texture->height};
                            auto image_res = backend_->CreateTexture(
                                texture->path.c_str(), tex_desc,
                                texture->data.data());

                            if (image_res) {
                                texture->image = image_res.value();
                            }
                        }
                    }
                }
            }
        }
    });

    // Get the VP matrix
    OUTCOME_TRY(camera, scene->GetAttachment<Camera>(engine_->main_camera()));

    // Get the camera node (TODO main camera index)
    auto camera_nodes = scene->GetAttachedNodes<Camera>({0});
    glm::mat4 camera_transform = glm::mat4(1.0f);

    if (camera_nodes && camera_nodes.value()->size() > 0) {
        auto camera_node = *camera_nodes.value()->begin();
        // TODO compute cached model returns the model
        scene->ComputeCachedModel(camera_node);
        camera_transform = scene->GetCachedModel(camera_node).value();
    }

    float aspect_ratio = float(engine_->platform()->GetWidth()) /
                         engine_->platform()->GetHeight();
    auto fovy = camera->h_fov / aspect_ratio;

    // Compute position, look at and up vector in world space
    glm::vec3 ws_pos = camera_transform * glm::vec4(camera->position, 1.0f);
    glm::vec3 ws_look_at =
        glm::normalize(camera_transform * glm::vec4(camera->look_at, 0.0f));
    glm::vec3 ws_up =
        glm::normalize(camera_transform * glm::vec4(camera->up, 0.0f));
    glm::mat4 view = glm::lookAt(ws_pos, ws_pos + ws_look_at, ws_up);

    glm::mat4 proj = glm::perspective(glm::radians(fovy), aspect_ratio,
                                      camera->near_plane, camera->far_plane);
    proj[1][1] *= -1;  // Vulkan-style projection

    glm::mat4 vp = proj * view;

    const auto keypresses = engine_->input_system()->GetFrameInput().keypresses;
    if (keypresses.find(KeyInput::H) != keypresses.end()) {
        vp_hold = std::make_unique<glm::mat4>(vp);
    } else if (keypresses.find(KeyInput::R) != keypresses.end()) {
        vp_hold = {};
    }

    struct RenderSequenceElement {
        AttachmentIndex<Mesh> mesh;
        NodeIndex node;
        glm::vec3 cs_center;
    };
    std::vector<RenderSequenceElement> render_sequence;
    render_sequence.reserve(scene->GetAttachmentCount<Mesh>());

    // Frustum culling
    scene->ForEach<Mesh>([&](auto id, auto _, Mesh& mesh) {
        auto& culling_vp = vp_hold ? *vp_hold : vp;

        auto nodes_result = scene->GetAttachedNodes<Mesh>(id);
        if (!nodes_result || !nodes_result.value()) {
            return;
        }

        std::vector<glm::vec4> cs_vertices(8);
        for (auto& mesh_node : *nodes_result.value()) {
            glm::mat4 mvp =
                culling_vp * scene->GetCachedModel(mesh_node).value();

            cs_vertices[0] = mvp * glm::vec4(mesh.bounding_box->min, 1.0f);
            cs_vertices[1] = mvp * glm::vec4(mesh.bounding_box->max, 1.0f);
            cs_vertices[2] = mvp * glm::vec4(mesh.bounding_box->min.x,
                                             mesh.bounding_box->min.y,
                                             mesh.bounding_box->max.z, 1.0f);
            cs_vertices[3] = mvp * glm::vec4(mesh.bounding_box->min.x,
                                             mesh.bounding_box->max.y,
                                             mesh.bounding_box->max.z, 1.0f);
            cs_vertices[4] = mvp * glm::vec4(mesh.bounding_box->min.x,
                                             mesh.bounding_box->max.y,
                                             mesh.bounding_box->min.z, 1.0f);
            cs_vertices[5] = mvp * glm::vec4(mesh.bounding_box->max.x,
                                             mesh.bounding_box->max.y,
                                             mesh.bounding_box->min.z, 1.0f);
            cs_vertices[6] = mvp * glm::vec4(mesh.bounding_box->max.x,
                                             mesh.bounding_box->min.y,
                                             mesh.bounding_box->min.z, 1.0f);
            cs_vertices[7] = mvp * glm::vec4(mesh.bounding_box->max.x,
                                             mesh.bounding_box->min.y,
                                             mesh.bounding_box->max.z, 1.0f);

            bool culled =
                std::all_of(cs_vertices.begin(), cs_vertices.end(),
                            [](const auto& v) { return v.x <= -v.w; }) ||
                std::all_of(cs_vertices.begin(), cs_vertices.end(),
                            [](const auto& v) { return v.x >= v.w; }) ||
                std::all_of(cs_vertices.begin(), cs_vertices.end(),
                            [](const auto& v) { return v.y <= -v.w; }) ||
                std::all_of(cs_vertices.begin(), cs_vertices.end(),
                            [](const auto& v) { return v.y >= v.w; }) ||
                std::all_of(cs_vertices.begin(), cs_vertices.end(),
                            [](const auto& v) { return v.z <= 0; }) ||
                std::all_of(cs_vertices.begin(), cs_vertices.end(),
                            [](const auto& v) { return v.z >= v.w; });

            if (!culled) {
                glm::vec3 bbox_center =
                    (mesh.bounding_box->min + mesh.bounding_box->max) * 0.5f;
                glm::vec4 cs_center = mvp * glm::vec4(bbox_center, 1.0f);
                cs_center /= cs_center.w;

                render_sequence.push_back({id, mesh_node, cs_center});
            }
        }
    });

    // Sorting
    std::sort(render_sequence.begin(), render_sequence.end(),
              [](const auto& a, const auto& b) {
                  return a.cs_center.z < b.cs_center.z;
              });

    RenderPassFn forward_pass = [&](RenderPassDesc rp, FramebufferDesc fb,
                                    FrameIndex frame_id) {
        // TODO pass color images here
        uint32_t sample_count = 1;
        auto image =
            backend_->render_plan().color_images.find(fb.color_images[0]);
        if (image != backend_->render_plan().color_images.end()) {
            sample_count = image->second.samples;
        }

        backend_->BindDepthStencilState(DepthStencilState{});
        backend_->BindRasterizationState(RasterizationState{});
        backend_->BindMultisampleState(
            MultisampleState{sample_count, sample_count > 1});

        // Render meshes
        AttachmentIndex<Mesh> last_mesh_id{0, 0};
        Mesh* mesh{nullptr};
        for (const auto& seq_entry : render_sequence) {
            auto mesh_id = seq_entry.mesh;
            auto node_id = seq_entry.node;

            if (mesh_id != last_mesh_id) {
                auto mesh_res = scene->GetAttachment<Mesh>(mesh_id);
                if (!mesh_res) {
                    spdlog::error("Couldn't find mesh {}.", mesh_id);
                    continue;
                }

                mesh = mesh_res.value();
                last_mesh_id = mesh_id;

                // Bind the new mesh
                auto material_result =
                    scene->GetAttachment<Material>(mesh->material);
                if (!material_result) {
                    spdlog::error("Couldn't find material for mesh {}.",
                                  mesh->name);
                    continue;
                }
                auto& material = *material_result.value();

                auto pipeline_res = backend_->GetGraphicsPipeline(
                    {"../../../assets/shaders/pbr.vert",
                     ShaderSourceType::Filename,
                     GetVertexShaderPreamble(*mesh)},
                    {"../../../assets/shaders/pbr.frag",
                     ShaderSourceType::Filename,
                     GetFragmentShaderPreamble(*mesh, material)});
                if (!pipeline_res) {
                    spdlog::error("Couldn't get pipeline for material {}.",
                                  material.name);
                    continue;
                }

                backend_->BindGraphicsPipeline(*pipeline_res.value());

                backend_->BindVertexInputFormat(*mesh->vertex_input_format);
                BindMeshBuffers(*mesh);
                BindMaterialTextures(material);

                auto frag_ubo_res = backend_->GetUniformBuffer(
                    BufferType::PerNode, node_id, "frag_ubo");
                if (!frag_ubo_res) {
                    frag_ubo_res = backend_->CreateUniformBuffer(
                        BufferType::PerNode, node_id, "frag_ubo", 3 * 256,
                        false);
                }

                if (!mesh->indices.empty()) {
                    backend_->BindIndexBuffer(*mesh->buffers.index);
                }

                auto frag_ubo = frag_ubo_res.value();

                struct FragUBO {
                    float exposure;
                    float gamma;
                    float metallic;
                    float roughness;
                    glm::vec4 base_color;
                    glm::vec3 camera;
                    float alpha_cutoff;
                };
                FragUBO frag_ubo_data{
                    4.5f,   2.2f, 0.2f, 0.5f, {material.diffuse_color, 1.0f},
                    ws_pos, 0.0f};

                backend_->UpdateBuffer(*frag_ubo, frame_id * 256,
                                       sizeof(frag_ubo_data), &frag_ubo_data);
                backend_->BindUniformBuffer(*frag_ubo, frame_id * 256,
                                            sizeof(frag_ubo_data), 13);
            }

            // Draw the mesh node
            auto& model = scene->GetCachedModel(node_id).value();
            glm::mat4 mvp = vp * model;

            auto vtx_ubo_res = backend_->GetUniformBuffer(BufferType::PerNode,
                                                          node_id, "vtx_ubo");
            if (!vtx_ubo_res) {
                vtx_ubo_res = backend_->CreateUniformBuffer(
                    BufferType::PerNode, node_id, "vtx_ubo", 3 * 256, false);
            }

            auto vtx_ubo = vtx_ubo_res.value();

            struct VtxUBO {
                glm::mat4 mvp;
                glm::mat4 model;
                glm::mat4 normals;
            };
            VtxUBO vtx_ubo_data{std::move(mvp), model,
                                glm::inverseTranspose(model)};

            backend_->UpdateBuffer(*vtx_ubo, frame_id * 256,
                                   sizeof(vtx_ubo_data), &vtx_ubo_data);
            backend_->BindUniformBuffer(*vtx_ubo, frame_id * 256,
                                        sizeof(vtx_ubo_data), 12);

            if (!mesh->indices.empty()) {
                backend_->DrawIndexed(
                    static_cast<uint32_t>(mesh->indices.size()));
            } else {
                backend_->Draw(static_cast<uint32_t>(mesh->vertices.size()));
            }
        }

        // Draw skybox
        auto pipeline_res = backend_->GetGraphicsPipeline(
            {"../../../assets/shaders/skybox.vert", ShaderSourceType::Filename},
            {"../../../assets/shaders/skybox.frag",
             ShaderSourceType::Filename});
        if (!pipeline_res) {
            spdlog::error("Couldn't get pipeline for skybox.");
            // TODO return
        }

        auto sphere_res = scene->FindAttachment<Mesh>("goma_sphere");
        if (!sphere_res) {
            spdlog::error("Couldn't find the sphere mesh for skybox.");
            // TODO return
        }
        auto sphere = sphere_res.value().second;

        backend_->BindGraphicsPipeline(*pipeline_res.value());
        backend_->BindVertexInputFormat(*sphere->vertex_input_format);
        BindMeshBuffers(*sphere);

        // TODO also bind index buffer in BindMeshBuffers
        if (!sphere->indices.empty()) {
            backend_->BindIndexBuffer(*sphere->buffers.index);
        }

        // Set depth clamp to 1.0f and depth test to Equal
        backend_->BindDepthStencilState({true, false, CompareOp::Equal});
        backend_->BindRasterizationState(
            RasterizationState{true, false, PolygonMode::Fill, CullMode::None});
        backend_->SetViewport({{float(engine_->platform()->GetWidth()),
                                float(engine_->platform()->GetHeight()), 0.0f,
                                0.0f, 1.0f, 1.0f}});

        auto skybox_tex_res = backend_->GetTexture("goma_skybox");
        if (!skybox_tex_res) {
            spdlog::error("Couldn't get skybox texture.");
            // TODO return
        }

        backend_->BindTexture(skybox_tex_res.value()->vez, 0);
        backend_->BindVertexUniforms({vp});
        backend_->DrawIndexed(static_cast<uint32_t>(sphere->indices.size()));

        return outcome::success();
    };

    backend_->RenderFrame({std::move(forward_pass)}, "color");

    return outcome::success();
}

result<void> Renderer::CreateSkybox() {
    CreateSphere();

    /*
    const std::vector<char*> filenames{"bluecloud_ft.jpg", "bluecloud_bk.jpg",
                                       "bluecloud_up.jpg", "bluecloud_dn.jpg",
                                       "bluecloud_rt.jpg", "bluecloud_lf.jpg"};
    */

    const std::vector<char*> filenames{"posx.jpg", "negx.jpg", "posy.jpg",
                                       "negy.jpg", "posz.jpg", "negz.jpg"};

    std::string base_path{"../../../assets/textures/skybox/yokohama/"};

    std::vector<void*> stbi_images;

    int width, height;
    for (auto& filename : filenames) {
        // Load skybox using stb_image
        int n;
        auto full_path = base_path + filename;
        auto image_data = stbi_load(full_path.c_str(), &width, &height, &n, 4);

        if (!image_data) {
            spdlog::warn("Decompressing \"{}\" failed with error: {}",
                         full_path, stbi_failure_reason());
            for (auto& stbi_image : stbi_images) {
                stbi_image_free(stbi_image);
            }
            return Error::DecompressionFailed;
        }

        stbi_images.push_back(image_data);
    }

    TextureDesc tex_desc{static_cast<uint32_t>(width),
                         static_cast<uint32_t>(height)};
    tex_desc.mipmapping = false;
    tex_desc.cubemap = true;

    backend_->CreateTexture("goma_skybox", tex_desc, stbi_images);

    for (auto& stbi_image : stbi_images) {
        stbi_image_free(stbi_image);
    }

    return outcome::success();
}

// From https://github.com/Erkaman/cute-deferred-shading
result<void> Renderer::CreateSphere() {
    int stacks = 20;
    int slices = 20;
    const float PI = glm::pi<float>();

    Mesh mesh{"goma_sphere"};

    // loop through stacks.
    for (int i = 0; i <= stacks; ++i) {
        float V = (float)i / (float)stacks;
        float phi = V * PI;

        // loop through the slices.
        for (int j = 0; j <= slices; ++j) {
            float U = (float)j / (float)slices;
            float theta = U * (PI * 2);

            // use spherical coordinates to calculate the positions.
            float x = cos(theta) * sin(phi);
            float y = cos(phi);
            float z = sin(theta) * sin(phi);

            mesh.vertices.push_back({x, y, z});
        }
    }

    // Calc The Index Positions
    for (int i = 0; i < slices * stacks + slices; ++i) {
        mesh.indices.push_back(static_cast<uint32_t>(i));
        mesh.indices.push_back(static_cast<uint32_t>(i + slices + 1));
        mesh.indices.push_back(static_cast<uint32_t>(i + slices));

        mesh.indices.push_back(static_cast<uint32_t>(i + slices + 1));
        mesh.indices.push_back(static_cast<uint32_t>(i));
        mesh.indices.push_back(static_cast<uint32_t>(i + 1));
    }

    OUTCOME_TRY(attachment,
                engine_->scene()->CreateAttachment<Mesh>(std::move(mesh)));
    engine_->scene()->RegisterAttachment<Mesh>(attachment, "goma_sphere");

    return outcome::success();
}

const char* Renderer::GetVertexShaderPreamble(
    const VertexShaderPreambleDesc& desc) {
    auto result = vs_preamble_map_.find(desc.int_repr);

    if (result != vs_preamble_map_.end()) {
        return result->second.c_str();
    } else {
        // TODO stringstream
        std::string preamble;

        if (desc.has_positions) {
            preamble += "#define HAS_POSITIONS\n";
        }
        if (desc.has_normals) {
            preamble += "#define HAS_NORMALS\n";
        }
        if (desc.has_tangents) {
            preamble += "#define HAS_TANGENTS\n";
        }
        if (desc.has_bitangents) {
            preamble += "#define HAS_BITANGENTS\n";
        }
        if (desc.has_colors) {
            preamble += "#define HAS_COLORS\n";
        }
        if (desc.has_uv0) {
            preamble += "#define HAS_UV0\n";
        }
        if (desc.has_uv1) {
            preamble += "#define HAS_UV1\n";
        }
        if (desc.has_uvw) {
            preamble += "#define HAS_UVW\n";
        }

        vs_preamble_map_[desc.int_repr] = std::move(preamble);
        return vs_preamble_map_[desc.int_repr].c_str();
    }
}

const char* Renderer::GetVertexShaderPreamble(const Mesh& mesh) {
    return GetVertexShaderPreamble(VertexShaderPreambleDesc{
        !mesh.vertices.empty(), !mesh.normals.empty(), !mesh.tangents.empty(),
        !mesh.bitangents.empty(), !mesh.colors.empty(), mesh.uv_sets.size() > 0,
        mesh.uv_sets.size() > 1, mesh.uvw_sets.size() > 0});
}

const char* Renderer::GetFragmentShaderPreamble(
    const FragmentShaderPreambleDesc& desc) {
    auto result = fs_preamble_map_.find(desc.int_repr);

    if (result != fs_preamble_map_.end()) {
        return result->second.c_str();
    } else {
        std::string preamble;

        // Mesh
        if (desc.has_positions) {
            preamble += "#define HAS_POSITIONS\n";
        }
        if (desc.has_normals) {
            preamble += "#define HAS_NORMALS\n";
        }
        if (desc.has_tangents) {
            preamble += "#define HAS_TANGENTS\n";
        }
        if (desc.has_bitangents) {
            preamble += "#define HAS_BITANGENTS\n";
        }
        if (desc.has_colors) {
            preamble += "#define HAS_COLORS\n";
        }
        if (desc.has_uv0) {
            preamble += "#define HAS_UV0\n";
        }
        if (desc.has_uv1) {
            preamble += "#define HAS_UV1\n";
        }
        if (desc.has_uvw) {
            preamble += "#define HAS_UVW\n";
        }

        // Material
        if (desc.has_diffuse_map) {
            preamble += "#define HAS_DIFFUSE_MAP\n";
        }
        if (desc.has_specular_map) {
            preamble += "#define HAS_SPECULAR_MAP\n";
        }
        if (desc.has_ambient_map) {
            preamble += "#define HAS_AMBIENT_MAP\n";
        }
        if (desc.has_emissive_map) {
            preamble += "#define HAS_EMISSIVE_MAP\n";
        }
        if (desc.has_metallic_roughness_map) {
            preamble += "#define HAS_METALLIC_ROUGHNESS_MAP\n";
        }
        if (desc.has_height_map) {
            preamble += "#define HAS_HEIGHT_MAP\n";
        }
        if (desc.has_normal_map) {
            preamble += "#define HAS_NORMAL_MAP\n";
        }
        if (desc.has_shininess_map) {
            preamble += "#define HAS_SHININESS_MAP\n";
        }
        if (desc.has_opacity_map) {
            preamble += "#define HAS_OPACITY_MAP\n";
        }
        if (desc.has_displacement_map) {
            preamble += "#define HAS_DISPLACEMENT_MAP\n";
        }
        if (desc.has_light_map) {
            preamble += "#define HAS_LIGHT_MAP\n";
        }
        if (desc.has_reflection_map) {
            preamble += "#define HAS_REFLECTION_MAP\n";
        }

        // preamble += "#define DEBUG_OUTPUT\n";
        preamble += "#define DEBUG_NORMAL\n";

        fs_preamble_map_[desc.int_repr] = std::move(preamble);
        return fs_preamble_map_[desc.int_repr].c_str();
    }
}

const char* Renderer::GetFragmentShaderPreamble(const Mesh& mesh,
                                                const Material& material) {
    auto check_type = [&](TextureType type) {
        return material.texture_bindings.find(type) !=
               material.texture_bindings.end();
    };

    return GetFragmentShaderPreamble(
        FragmentShaderPreambleDesc{!mesh.vertices.empty(),
                                   !mesh.normals.empty(),
                                   !mesh.tangents.empty(),
                                   !mesh.bitangents.empty(),
                                   !mesh.colors.empty(),
                                   mesh.uv_sets.size() > 0,
                                   mesh.uv_sets.size() > 1,
                                   mesh.uvw_sets.size() > 0,
                                   check_type(TextureType::Diffuse),
                                   check_type(TextureType::Specular),
                                   check_type(TextureType::Ambient),
                                   check_type(TextureType::Emissive),
                                   check_type(TextureType::MetallicRoughness),
                                   check_type(TextureType::HeightMap),
                                   check_type(TextureType::NormalMap),
                                   check_type(TextureType::Shininess),
                                   check_type(TextureType::Opacity),
                                   check_type(TextureType::Displacement),
                                   check_type(TextureType::LightMap),
                                   check_type(TextureType::Reflection)});
}

result<void> Renderer::BindMeshBuffers(const Mesh& mesh) {
    // TODO this copies shared pointers
    uint32_t binding = 0;
    auto bind = [&](std::shared_ptr<Buffer> buf) {
        if (buf && buf->valid) {
            backend_->BindVertexBuffer(*buf, binding, 0);
        }
        binding++;
    };

    bind(mesh.buffers.vertex);
    bind(mesh.buffers.normal);
    bind(mesh.buffers.tangent);
    bind(mesh.buffers.bitangent);
    bind(mesh.buffers.color);
    bind(mesh.buffers.uv0);
    bind(mesh.buffers.uv1);
    bind(mesh.buffers.uvw);

    return outcome::success();
}

result<void> Renderer::BindMaterialTextures(const Material& material) {
    Scene* scene = engine_->scene();
    if (!scene) {
        return Error::NoSceneLoaded;
    }

    uint32_t binding_id = 0;

    const std::vector<TextureType> texture_types = {
        TextureType::Diffuse,           TextureType::Specular,
        TextureType::Ambient,           TextureType::Emissive,
        TextureType::MetallicRoughness, TextureType::HeightMap,
        TextureType::NormalMap,         TextureType::Shininess,
        TextureType::Opacity,           TextureType::Displacement,
        TextureType::LightMap,          TextureType::Reflection};

    for (const auto& texture_type : texture_types) {
        auto binding = material.texture_bindings.find(texture_type);

        if (binding != material.texture_bindings.end() &&
            !binding->second.empty()) {
            auto texture_res =
                scene->GetAttachment<Texture>(binding->second[0].index);
            if (texture_res) {
                auto& texture = texture_res.value();
                if (texture->image && texture->image->valid) {
                    backend_->BindTexture(*texture->image, binding_id);
                }
            }
        }
        binding_id++;
    }

    return outcome::success();
}

}  // namespace goma
