#include "renderer/renderer.hpp"

#include "engine.hpp"
#include "renderer/vez/vez_backend.hpp"
#include "scene/attachments/camera.hpp"
#include "scene/attachments/light.hpp"
#include "scene/attachments/material.hpp"
#include "scene/attachments/mesh.hpp"

#include <stb_image.h>

#ifndef GOMA_ASSETS_DIR
#define GOMA_ASSETS_DIR "assets/"
#endif

namespace goma {

Renderer::Renderer(Engine& engine)
    : engine_(engine), backend_(std::make_unique<VezBackend>()) {
    if (auto result = backend_->InitContext()) {
        spdlog::info("Context initialized.");
    } else {
        spdlog::error(result.error().message());
    }

    if (auto result = backend_->InitSurface(engine_.platform())) {
        spdlog::info("Surface initialized.");
    } else {
        spdlog::error(result.error().message());
    }

    RenderPlan render_plan{};

    render_plan.color_images = {
        {"color", {{}, 4}},
        {"blur_full", {}},
        {"blur_half", {{0.5f, 0.5f}}},
        {"blur_quarter", {{0.25f, 0.25f}}},
        {"resolved_image", {}},
        {"postprocessing", {}},
    };

    render_plan.depth_images = {
        {"depth", {{}, 4, Format::DepthOnly}},
        {
            "shadow_depth",
            {{2048.0f, 2048.0f, ExtentType::Absolute}, 1, Format::DepthOnly},
        },
    };

    std::vector<BlitDesc> blits;
    blits.push_back({{"resolved_image", {}}, {"blur_half", {0.5f, 0.5f}}});
    blits.push_back(
        {{"blur_half", {0.5f, 0.5f}}, {"blur_quarter", {0.25f, 0.25f}}});
    blits.push_back({{"blur_quarter", {0.25f, 0.25f}}, {"blur_full", {}}});

    render_plan.passes = {
        GeneralPassEntry{"update_light_buffer"},
        RenderPassEntry{
            "shadow", RenderPassDesc{{}, DepthAttachmentDesc{"shadow_depth"}}},
        RenderPassEntry{
            "forward",
            RenderPassDesc{{ColorAttachmentDesc{"color",
                                                true,
                                                true,
                                                {0.1f, 0.1f, 0.1f, 1.0f},
                                                "resolved_image"}},
                           DepthAttachmentDesc{"depth"}},
            std::move(blits)},
        RenderPassEntry{
            "postprocessing",
            RenderPassDesc{{ColorAttachmentDesc{"postprocessing"}}}},
    };

    backend_->SetRenderPlan(std::move(render_plan));
}

result<void> Renderer::Render() {
    if (!engine_.scene()) {
        return Error::NoSceneLoaded;
    }
    Scene& scene = *engine_.scene();

    // Ensure that all meshes have their own buffers
    CreateMeshBuffers(scene);

    // Ensure that all meshes have their own vertex input format
    CreateVertexInputFormats(scene);

    // Upload textures
    UploadTextures(scene);

    // Set up light buffer
    auto light_buffer_data = GetLightBufferData(scene);

    // Get the VP matrix
    OUTCOME_TRY(camera_ref, scene.GetAttachment<Camera>(engine_.main_camera()));
    auto& camera = camera_ref.get();

    // Get the camera node
    auto camera_nodes = scene.GetAttachedNodes<Camera>(engine_.main_camera());
    glm::mat4 camera_transform = glm::mat4(1.0f);

    if (camera_nodes && camera_nodes.value().get().size() > 0) {
        auto camera_node = *camera_nodes.value().get().begin();
        camera_transform = scene.GetTransformMatrix(camera_node).value();
    }

    float aspect_ratio =
        float(engine_.platform().GetWidth()) / engine_.platform().GetHeight();
    auto fovy = camera.h_fov / aspect_ratio;

    // Compute position, look at and up vector in world space
    glm::vec3 ws_pos = camera_transform * glm::vec4(camera.position, 1.0f);
    glm::vec3 ws_look_at =
        glm::normalize(camera_transform * glm::vec4(camera.look_at, 0.0f));
    glm::vec3 ws_up =
        glm::normalize(camera_transform * glm::vec4(camera.up, 0.0f));
    glm::mat4 view = glm::lookAt(ws_pos, ws_pos + ws_look_at, ws_up);

    glm::mat4 proj = glm::perspective(glm::radians(fovy), aspect_ratio,
                                      camera.near_plane, camera.far_plane);
    proj[1][1] *= -1;  // Vulkan-style projection

    glm::mat4 vp = proj * view;

    // Compute transform matrices for shadow maps
    auto shadow_vp = glm::mat4(1.0f);
    bool shadow_map_found{false};
    int32_t i{0};

    scene.ForEach<Light>([&](auto id, auto nodes, Light& light) {
        for (const auto& light_node : nodes) {
            auto model = scene.GetTransformMatrix(light_node).value();

            if (!shadow_map_found && light.type == LightType::Directional) {
                shadow_map_found = true;
                light_buffer_data.shadow_ids[0] = i;

                glm::vec3 ws_eye = model * glm::vec4(light.position, 1.0f);
                glm::vec3 ws_direction =
                    glm::normalize(model * glm::vec4(light.direction, 0.0f));
                glm::vec3 ws_up =
                    glm::normalize(model * glm::vec4(light.up, 0.0f));
                auto shadow_view =
                    glm::lookAt(ws_eye, ws_eye + ws_direction, ws_up);

                constexpr float size = 20.0f;
                auto shadow_proj =
                    glm::ortho(-size, size, -size, size, -size, size);
                shadow_vp = shadow_proj * shadow_view;
            }

            i++;
        }
    });

    // Hold/release the current culling state
    const auto keypresses = engine_.input_system().GetFrameInput().keypresses;
    const auto last_keypresses =
        engine_.input_system().GetLastFrameInput().keypresses;

    if (keypresses.find(KeyInput::H) != keypresses.end()) {
        vp_hold = std::make_unique<glm::mat4>(vp);
    } else if (keypresses.find(KeyInput::R) != keypresses.end()) {
        vp_hold = {};
    }

    // Clear the shader cache (reload shaders)
    if (keypresses.find(KeyInput::C) != keypresses.end() &&
        last_keypresses.find(KeyInput::C) == last_keypresses.end()) {
        spdlog::info("Reloading shaders!");
        backend_->ClearShaderCache();
    }

    // Get main render sequence
    RenderSequence render_seq;
    render_seq.reserve(scene.GetAttachmentCount<Mesh>());

    scene.ForEach<Mesh>([&](auto id, auto nodes, Mesh& mesh) {
        for (auto& mesh_node : nodes) {
            glm::mat4 mvp = vp * scene.GetTransformMatrix(mesh_node).value();
            glm::vec3 bbox_center =
                (mesh.bounding_box->min + mesh.bounding_box->max) * 0.5f;
            glm::vec4 cs_center = mvp * glm::vec4(bbox_center, 1.0f);
            cs_center /= cs_center.w;
            render_seq.push_back({id, mesh_node, cs_center});
        }
    });

    // Frustum culling
    RenderSequence visible_seq = Cull(scene, render_seq, vp);

    // Sorting
    std::sort(visible_seq.begin(), visible_seq.end(),
              [](const auto& a, const auto& b) {
                  return a.cs_center.z < b.cs_center.z;
              });

    backend_->RenderFrame(
        {
            [this, &scene, &light_buffer_data](FrameIndex frame_id,
                                               const RenderPassDesc*) {
                return UpdateLightBuffer(frame_id, scene, light_buffer_data);
            },
            [this, &scene, &render_seq, &shadow_vp](FrameIndex frame_id,
                                                    const RenderPassDesc*) {
                return ShadowPass(frame_id, scene, render_seq, shadow_vp);
            },
            [this, &scene, &visible_seq, &ws_pos, &vp, &shadow_vp](
                FrameIndex frame_id, const RenderPassDesc*) {
                return ForwardPass(frame_id, scene, visible_seq, ws_pos, vp,
                                   shadow_vp);
            },
            [this, &scene](FrameIndex frame_id, const RenderPassDesc*) {
                return PostprocessingPass(frame_id);
            },
        },
        "postprocessing");

    return outcome::success();
}

result<void> Renderer::CreateSkybox() {
    CreateSphere();

    const std::vector<char*> filenames{"posx.jpg", "negx.jpg", "posy.jpg",
                                       "negy.jpg", "posz.jpg", "negz.jpg"};

    std::string base_path{GOMA_ASSETS_DIR "textures/skybox/cloudy/"};

    std::vector<void*> stbi_images;

    int width = 0;
    int height = 0;
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
                engine_.scene()->CreateAttachment<Mesh>(std::move(mesh)));
    engine_.scene()->RegisterAttachment<Mesh>(attachment, "goma_sphere");

    return outcome::success();
}

void Renderer::CreateMeshBuffers(Scene& scene) {
    scene.ForEach<Mesh>([&](auto id, auto, Mesh& mesh) {
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
}

void Renderer::CreateVertexInputFormats(Scene& scene) {
    scene.ForEach<Mesh>([&](auto, auto, Mesh& mesh) {
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
}

void Renderer::UploadTextures(Scene& scene) {
    scene.ForEach<Material>([&](auto, auto, Material& material) {
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
                    scene.GetAttachment<Texture>(binding->second[0].index);
                if (texture_res) {
                    auto& texture = texture_res.value().get();

                    // Upload texture if necessary
                    if (!texture.image || !texture.image->valid) {
                        if (!texture.compressed) {
                            TextureDesc tex_desc{texture.width, texture.height};
                            auto image_res = backend_->CreateTexture(
                                texture.path.c_str(), tex_desc,
                                texture.data.data());

                            if (image_res) {
                                texture.image = image_res.value();
                            }
                        } else {
                            spdlog::warn(
                                "Compressed texture \"{}\" not supported.",
                                texture.path);
                        }
                    }
                }
            }
        }
    });
}

Renderer::RenderSequence Renderer::Cull(Scene& scene,
                                        const RenderSequence& render_seq,
                                        const glm::mat4& vp) {
    RenderSequence visible_seq;
    visible_seq.reserve(render_seq.size());

    std::vector<glm::vec4> cs_vertices(8);
    const auto& culling_vp = vp_hold ? *vp_hold : vp;

    std::copy_if(
        render_seq.begin(), render_seq.end(), std::back_inserter(visible_seq),
        [&culling_vp, &cs_vertices, &scene](const RenderSequenceElement& e) {
            glm::mat4 mvp =
                culling_vp * scene.GetTransformMatrix(e.node).value();
            auto& mesh = scene.GetAttachment<Mesh>(e.mesh).value().get();

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

            return !culled;
        });

    visible_seq.shrink_to_fit();
    return visible_seq;
}

Renderer::LightBufferData Renderer::GetLightBufferData(Scene& scene) {
    uint32_t num_lights = static_cast<uint32_t>(
        std::min(kMaxLights, scene.GetAttachmentCount<Light>()));
    LightBufferData light_buffer_data{glm::vec3(0.05f),
                                      static_cast<int32_t>(num_lights)};

    size_t i = 0;
    auto shadow_vp = glm::mat4(1.0f);
    scene.ForEach<Light>([&](auto id, auto nodes, Light& light) {
        if (i >= num_lights) {
            return;
        }

        for (const auto& light_node : nodes) {
            auto model = scene.GetTransformMatrix(light_node).value();

            auto& buf_light = light_buffer_data.lights[i];
            buf_light.direction = model * glm::vec4(light.direction, 0.0f);
            buf_light.type = static_cast<int32_t>(light.type);
            buf_light.color = light.diffuse_color;
            buf_light.intensity = light.intensity;
            buf_light.position = model * glm::vec4(light.position, 1.0f);
            buf_light.range = (100 + light.attenuation[0]) /
                              light.attenuation[1];  // range for 99% reduction
            buf_light.innerConeCos = std::cos(light.inner_cone_angle);
            buf_light.outerConeCos = std::cos(light.outer_cone_angle);

            i++;
        }
    });

    return light_buffer_data;
}

result<void> Renderer::UpdateLightBuffer(
    FrameIndex frame_id, Scene& scene,
    const LightBufferData& light_buffer_data) {
    constexpr auto padded_light_size =
        (sizeof(LightBufferData) / 256 + 1) * 256;
    auto light_buffer_res = backend_->GetUniformBuffer("lights");
    if (!light_buffer_res) {
        light_buffer_res = backend_->CreateUniformBuffer(
            "lights", 3 * padded_light_size, false);
    }

    auto light_buffer = light_buffer_res.value();

    backend_->UpdateBuffer(*light_buffer, frame_id * padded_light_size,
                           sizeof(LightBufferData), &light_buffer_data);

    return outcome::success();
}

result<void> Renderer::ShadowPass(FrameIndex frame_id, Scene& scene,
                                  const RenderSequence& render_seq,
                                  const glm::mat4& shadow_vp) {
    backend_->BindDepthStencilState(DepthStencilState{});
    backend_->BindRasterizationState(RasterizationState{});
    backend_->BindMultisampleState(MultisampleState{});

    backend_->SetViewport({{2048.0f, 2048.0f}});
    backend_->SetScissor({{2048, 2048}});

    // Render meshes
    AttachmentIndex<Mesh> last_mesh_id{0, 0};
    Mesh* mesh{nullptr};
    for (const auto& seq_entry : render_seq) {
        auto mesh_id = seq_entry.mesh;
        auto node_id = seq_entry.node;

        if (mesh_id != last_mesh_id) {
            auto mesh_res = scene.GetAttachment<Mesh>(mesh_id);
            if (!mesh_res) {
                spdlog::error("Couldn't find mesh {}.", mesh_id);
                continue;
            }

            mesh = &mesh_res.value().get();
            last_mesh_id = mesh_id;

            // Bind the new mesh
            auto material_result =
                scene.GetAttachment<Material>(mesh->material);
            if (!material_result) {
                spdlog::error("Couldn't find material for mesh {}.",
                              mesh->name);
                continue;
            }
            auto& material = material_result.value().get();

            auto pipeline_res = backend_->GetGraphicsPipeline(
                {GOMA_ASSETS_DIR "shaders/shadow.vert",
                 ShaderSourceType::Filename, GetVertexShaderPreamble(*mesh)});
            if (!pipeline_res) {
                spdlog::error("Couldn't get pipeline for the shadow pass.");
                continue;
            }

            backend_->BindGraphicsPipeline(*pipeline_res.value());

            backend_->BindVertexInputFormat(*mesh->vertex_input_format);
            BindMeshBuffers(*mesh);
            BindMaterialTextures(material);
        }

        // Draw the mesh node
        auto model = scene.GetTransformMatrix(node_id).value();
        glm::mat4 mvp = shadow_vp * model;

        auto vtx_ubo_res = backend_->GetUniformBuffer(
            BufferType::PerNode, node_id, "shadow_vtx_ubo");
        if (!vtx_ubo_res) {
            vtx_ubo_res = backend_->CreateUniformBuffer(
                BufferType::PerNode, node_id, "shadow_vtx_ubo", 3 * 256, false);
        }

        auto vtx_ubo = vtx_ubo_res.value();

        struct VtxUBO {
            glm::mat4 mvp;
            glm::mat4 model;
            glm::mat4 normals;
        };
        VtxUBO vtx_ubo_data{std::move(mvp), model,
                            glm::inverseTranspose(model)};

        backend_->UpdateBuffer(*vtx_ubo, frame_id * 256, sizeof(vtx_ubo_data),
                               &vtx_ubo_data);
        backend_->BindUniformBuffer(*vtx_ubo, frame_id * 256,
                                    sizeof(vtx_ubo_data), 12);

        if (!mesh->indices.empty()) {
            backend_->DrawIndexed(static_cast<uint32_t>(mesh->indices.size()));
        } else {
            backend_->Draw(static_cast<uint32_t>(mesh->vertices.size()));
        }
    }

    return outcome::success();
}

result<void> Renderer::ForwardPass(FrameIndex frame_id, Scene& scene,
                                   const RenderSequence& render_seq,
                                   const glm::vec3& camera_ws_pos,
                                   const glm::mat4& camera_vp,
                                   const glm::mat4& shadow_vp) {
    uint32_t sample_count = 1;
    auto image = backend_->render_plan().color_images.find("color");
    if (image != backend_->render_plan().color_images.end()) {
        sample_count = image->second.samples;
    }

    auto w = engine_.platform().GetWidth();
    auto h = engine_.platform().GetHeight();
    backend_->SetViewport({{static_cast<float>(w), static_cast<float>(h)}});
    backend_->SetScissor({{w, h}});

    backend_->BindDepthStencilState(DepthStencilState{});
    backend_->BindRasterizationState(RasterizationState{});
    backend_->BindMultisampleState(
        MultisampleState{sample_count, sample_count > 1});

    constexpr auto padded_light_size =
        (sizeof(LightBufferData) / 256 + 1) * 256;
    auto light_buffer_res = backend_->GetUniformBuffer("lights");
    if (light_buffer_res) {
        backend_->BindUniformBuffer(*light_buffer_res.value(),
                                    frame_id * padded_light_size,
                                    sizeof(LightBufferData), 15);
    }

    // Render meshes
    AttachmentIndex<Mesh> last_mesh_id{0, 0};
    Mesh* mesh{nullptr};
    for (const auto& seq_entry : render_seq) {
        auto mesh_id = seq_entry.mesh;
        auto node_id = seq_entry.node;

        if (mesh_id != last_mesh_id) {
            auto mesh_res = scene.GetAttachment<Mesh>(mesh_id);
            if (!mesh_res) {
                spdlog::error("Couldn't find mesh {}.", mesh_id);
                continue;
            }

            mesh = &mesh_res.value().get();
            last_mesh_id = mesh_id;

            // Bind the new mesh
            auto material_result =
                scene.GetAttachment<Material>(mesh->material);
            if (!material_result) {
                spdlog::error("Couldn't find material for mesh {}.",
                              mesh->name);
                continue;
            }
            auto& material = material_result.value().get();

            auto pipeline_res = backend_->GetGraphicsPipeline(
                {GOMA_ASSETS_DIR "shaders/pbr.vert", ShaderSourceType::Filename,
                 GetVertexShaderPreamble(*mesh)},
                {GOMA_ASSETS_DIR "shaders/pbr.frag", ShaderSourceType::Filename,
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

            auto shadow_depth_res =
                backend_->GetRenderTarget(frame_id, "shadow_depth");
            if (!shadow_depth_res) {
                spdlog::error("Couldn't get the shadow map.");
                continue;
            }

            backend_->BindTexture(*shadow_depth_res.value(), 14);

            auto frag_ubo_res = backend_->GetUniformBuffer(BufferType::PerNode,
                                                           node_id, "frag_ubo");
            if (!frag_ubo_res) {
                frag_ubo_res = backend_->CreateUniformBuffer(
                    BufferType::PerNode, node_id, "frag_ubo", 3 * 256, false);
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
            FragUBO frag_ubo_data{4.5f,
                                  2.2f,
                                  material.metallic_factor,
                                  material.roughness_factor,
                                  {material.diffuse_color, 1.0f},
                                  camera_ws_pos,
                                  material.alpha_cutoff};

            backend_->UpdateBuffer(*frag_ubo, frame_id * 256,
                                   sizeof(frag_ubo_data), &frag_ubo_data);
            backend_->BindUniformBuffer(*frag_ubo, frame_id * 256,
                                        sizeof(frag_ubo_data), 13);
        }

        // Draw the mesh node
        auto model = scene.GetTransformMatrix(node_id).value();
        glm::mat4 mvp = camera_vp * model;

        // Shadow correction maps X and Y for the shadow MVP
        // to the range [0, 1], useful to sample from the shadow map
        const glm::mat4 shadow_correction = {{0.5f, 0.0f, 0.0f, 0.0f},
                                             {0.0f, 0.5f, 0.0f, 0.0f},
                                             {0.0f, 0.0f, 1.0f, 0.0f},
                                             {0.5f, 0.5f, 0.0f, 1.0f}};
        glm::mat4 shadow_mvp = shadow_correction * shadow_vp * model;

        auto vtx_ubo_res =
            backend_->GetUniformBuffer(BufferType::PerNode, node_id, "vtx_ubo");
        if (!vtx_ubo_res) {
            vtx_ubo_res = backend_->CreateUniformBuffer(
                BufferType::PerNode, node_id, "vtx_ubo", 3 * 256, false);
        }

        auto vtx_ubo = vtx_ubo_res.value();

        struct VtxUBO {
            glm::mat4 mvp;
            glm::mat4 model;
            glm::mat4 normals;
            glm::mat4 shadow_mvp;
        };
        VtxUBO vtx_ubo_data{std::move(mvp), model, glm::inverseTranspose(model),
                            std::move(shadow_mvp)};

        backend_->UpdateBuffer(*vtx_ubo, frame_id * 256, sizeof(vtx_ubo_data),
                               &vtx_ubo_data);
        backend_->BindUniformBuffer(*vtx_ubo, frame_id * 256,
                                    sizeof(vtx_ubo_data), 12);

        if (!mesh->indices.empty()) {
            backend_->DrawIndexed(static_cast<uint32_t>(mesh->indices.size()));
        } else {
            backend_->Draw(static_cast<uint32_t>(mesh->vertices.size()));
        }
    }

    // Draw skybox
    auto pipeline_res = backend_->GetGraphicsPipeline(
        {GOMA_ASSETS_DIR "shaders/skybox.vert", ShaderSourceType::Filename},
        {GOMA_ASSETS_DIR "shaders/skybox.frag", ShaderSourceType::Filename});
    if (!pipeline_res) {
        spdlog::error("Couldn't get pipeline for skybox.");
        return Error::NotFound;
    }

    auto sphere_res = scene.FindAttachment<Mesh>("goma_sphere");
    if (!sphere_res) {
        spdlog::error("Couldn't find the sphere mesh for skybox.");
        return Error::NotFound;
    }
    auto& sphere = sphere_res.value().second.get();

    backend_->BindGraphicsPipeline(*pipeline_res.value());
    backend_->BindVertexInputFormat(*sphere.vertex_input_format);
    BindMeshBuffers(sphere);

    // Set depth clamp to 1.0f and depth test to Equal
    backend_->BindDepthStencilState({true, false, CompareOp::Equal});
    backend_->BindRasterizationState(
        RasterizationState{true, false, PolygonMode::Fill, CullMode::None});
    backend_->SetViewport(
        {{float(engine_.platform().GetWidth()),
          float(engine_.platform().GetHeight()), 0.0f, 0.0f, 1.0f, 1.0f}});

    auto skybox_tex_res = backend_->GetTexture("goma_skybox");
    if (!skybox_tex_res) {
        spdlog::error("Couldn't get skybox texture.");
        return Error::NotFound;
    }

    auto skybox_ubo_res = backend_->GetUniformBuffer("skybox");
    if (!skybox_ubo_res) {
        skybox_ubo_res =
            backend_->CreateUniformBuffer("skybox", 3 * 256, false);
    }
    auto skybox_ubo = skybox_ubo_res.value();

    backend_->UpdateBuffer(*skybox_ubo, frame_id * 256, sizeof(camera_vp),
                           &camera_vp);
    backend_->BindUniformBuffer(*skybox_ubo, frame_id * 256, sizeof(camera_vp),
                                12);

    backend_->BindTexture(skybox_tex_res.value()->vez, 0);
    backend_->DrawIndexed(static_cast<uint32_t>(sphere.indices.size()));

    return outcome::success();
}

result<void> Renderer::PostprocessingPass(FrameIndex frame_id) {
    auto resolved_image_res =
        backend_->GetRenderTarget(frame_id, "resolved_image");
    if (!resolved_image_res) {
        spdlog::error("Couldn't get the resolved image.");
    }

    auto blur_full_res = backend_->GetRenderTarget(frame_id, "blur_full");
    if (!blur_full_res) {
        spdlog::error("Couldn't get the blur image.");
    }

    auto depth_res = backend_->GetRenderTarget(frame_id, "depth");
    if (!depth_res) {
        spdlog::error("Couldn't get the resolved depth image.");
    }

    auto blur_full = blur_full_res.value();
    auto resolved_image = resolved_image_res.value();
    auto depth = depth_res.value();

    auto w = engine_.platform().GetWidth();
    auto h = engine_.platform().GetHeight();
    backend_->SetViewport({{static_cast<float>(w), static_cast<float>(h)}});
    backend_->SetScissor({{w, h}});

    backend_->BindDepthStencilState(DepthStencilState{});
    backend_->BindRasterizationState(RasterizationState{});
    backend_->BindMultisampleState(MultisampleState{});

    auto no_input = backend_->GetVertexInputFormat({});
    backend_->BindVertexInputFormat(*no_input.value());

    auto pipeline_res = backend_->GetGraphicsPipeline(
        {GOMA_ASSETS_DIR "shaders/fullscreen.vert", ShaderSourceType::Filename},
        {GOMA_ASSETS_DIR "shaders/postprocessing.frag",
         ShaderSourceType::Filename});
    if (!pipeline_res) {
        spdlog::error(
            "Couldn't get pipeline for "
            "postprocessing");
    }

    backend_->BindGraphicsPipeline(*pipeline_res.value());
    backend_->BindTexture(resolved_image->vez, 0);
    backend_->BindTexture(blur_full->vez, 1);
    backend_->BindTexture(depth->vez, 2);
    backend_->Draw(3);

    return outcome::success();
}

const char* Renderer::GetVertexShaderPreamble(
    const VertexShaderPreambleDesc& desc) {
    auto res = vs_preamble_map_.find(desc.int_repr);

    if (res != vs_preamble_map_.end()) {
        return res->second.c_str();
    } else {
        std::stringstream preamble;

        if (desc.has_positions) {
            preamble << "#define HAS_POSITIONS\n";
        }
        if (desc.has_normals) {
            preamble << "#define HAS_NORMALS\n";
        }
        if (desc.has_tangents) {
            preamble << "#define HAS_TANGENTS\n";
        }
        if (desc.has_bitangents) {
            preamble << "#define HAS_BITANGENTS\n";
        }
        if (desc.has_colors) {
            preamble << "#define HAS_COLORS\n";
        }
        if (desc.has_uv0) {
            preamble << "#define HAS_UV0\n";
        }
        if (desc.has_uv1) {
            preamble << "#define HAS_UV1\n";
        }
        if (desc.has_uvw) {
            preamble << "#define HAS_UVW\n";
        }

        vs_preamble_map_[desc.int_repr] = preamble.str();
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
    auto res = fs_preamble_map_.find(desc.int_repr);

    if (res != fs_preamble_map_.end()) {
        return res->second.c_str();
    } else {
        std::stringstream preamble;

        // Mesh
        if (desc.has_positions) {
            preamble << "#define HAS_POSITIONS\n";
        }
        if (desc.has_normals) {
            preamble << "#define HAS_NORMALS\n";
        }
        if (desc.has_tangents) {
            preamble << "#define HAS_TANGENTS\n";
        }
        if (desc.has_bitangents) {
            preamble << "#define HAS_BITANGENTS\n";
        }
        if (desc.has_colors) {
            preamble << "#define HAS_COLORS\n";
        }
        if (desc.has_uv0) {
            preamble << "#define HAS_UV0\n";
        }
        if (desc.has_uv1) {
            preamble << "#define HAS_UV1\n";
        }
        if (desc.has_uvw) {
            preamble << "#define HAS_UVW\n";
        }

        // Material
        if (desc.has_diffuse_map) {
            preamble << "#define HAS_DIFFUSE_MAP\n";
        }
        if (desc.has_specular_map) {
            preamble << "#define HAS_SPECULAR_MAP\n";
        }
        if (desc.has_ambient_map) {
            preamble << "#define HAS_AMBIENT_MAP\n";
        }
        if (desc.has_emissive_map) {
            preamble << "#define HAS_EMISSIVE_MAP\n";
        }
        if (desc.has_metallic_roughness_map) {
            preamble << "#define HAS_METALLIC_ROUGHNESS_MAP\n";
        }
        if (desc.has_height_map) {
            preamble << "#define HAS_HEIGHT_MAP\n";
        }
        if (desc.has_normal_map) {
            preamble << "#define HAS_NORMAL_MAP\n";
        }
        if (desc.has_shininess_map) {
            preamble << "#define HAS_SHININESS_MAP\n";
        }
        if (desc.has_opacity_map) {
            preamble << "#define HAS_OPACITY_MAP\n";
        }
        if (desc.has_displacement_map) {
            preamble << "#define HAS_DISPLACEMENT_MAP\n";
        }
        if (desc.has_light_map) {
            preamble << "#define HAS_LIGHT_MAP\n";
        }
        if (desc.has_reflection_map) {
            preamble << "#define HAS_REFLECTION_MAP\n";
        }
        if (desc.alpha_mask) {
            preamble << "#define ALPHAMODE_MASK\n";
        }

        fs_preamble_map_[desc.int_repr] = preamble.str();
        return fs_preamble_map_[desc.int_repr].c_str();
    }
}

const char* Renderer::GetFragmentShaderPreamble(const Mesh& mesh,
                                                const Material& material) {
    auto check_type = [&](TextureType type) {
        return material.texture_bindings.find(type) !=
               material.texture_bindings.end();
    };

    return GetFragmentShaderPreamble(FragmentShaderPreambleDesc{
        !mesh.vertices.empty(),
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
        check_type(TextureType::Reflection),
        material.alpha_cutoff < 1.0f,
    });
}

result<void> Renderer::BindMeshBuffers(const Mesh& mesh) {
    uint32_t binding = 0;
    auto bind = [&](const Buffer* buf) {
        if (buf && buf->valid) {
            backend_->BindVertexBuffer(*buf, binding, 0);
        }
        binding++;
    };

    bind(mesh.buffers.vertex.get());
    bind(mesh.buffers.normal.get());
    bind(mesh.buffers.tangent.get());
    bind(mesh.buffers.bitangent.get());
    bind(mesh.buffers.color.get());
    bind(mesh.buffers.uv0.get());
    bind(mesh.buffers.uv1.get());
    bind(mesh.buffers.uvw.get());

    if (!mesh.indices.empty()) {
        backend_->BindIndexBuffer(*mesh.buffers.index);
    }

    return outcome::success();
}

result<void> Renderer::BindMaterialTextures(const Material& material) {
    Scene* scene = engine_.scene();
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
                auto& texture = texture_res.value().get();
                if (texture.image && texture.image->valid) {
                    backend_->BindTexture(*texture.image, binding_id);
                }
            }
        }
        binding_id++;
    }

    return outcome::success();
}

}  // namespace goma
