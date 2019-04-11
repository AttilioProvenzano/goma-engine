#include "renderer/renderer.hpp"

#include "engine.hpp"
#include "renderer/vez/vez_backend.hpp"
#include "scene/attachments/camera.hpp"
#include "scene/attachments/material.hpp"
#include "scene/attachments/mesh.hpp"

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

    // Ensure that all meshes have world-space model matrices
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

    struct RenderSequenceElement {
        AttachmentIndex<Mesh> mesh;
        NodeIndex node;
        glm::vec3 cs_center;
    };
    std::vector<RenderSequenceElement> render_sequence;
    render_sequence.reserve(scene->GetAttachmentCount<Mesh>());

    // Frustum culling
    scene->ForEach<Mesh>([&](auto id, auto _, Mesh& mesh) {
        auto nodes_result = scene->GetAttachedNodes<Mesh>(id);
        if (!nodes_result || !nodes_result.value()) {
            return;
        }

        for (auto& mesh_node : *nodes_result.value()) {
            glm::mat4 mvp = vp * scene->GetCachedModel(mesh_node).value();

            glm::vec3 bbox_center =
                (mesh.bounding_box->min + mesh.bounding_box->max) * 0.5f;
            glm::vec4 cs_center = mvp * glm::vec4(bbox_center, 1.0f);
            cs_center /= cs_center.w;

            glm::vec4 cs_1 = mvp * glm::vec4(mesh.bounding_box->min, 1.0f);
            cs_1 /= cs_1.w;

            glm::vec4 cs_2 = mvp * glm::vec4(mesh.bounding_box->max, 1.0f);
            cs_2 /= cs_2.w;

            auto visible = [](glm::vec3 v1, glm::vec3 v2) {
                // Compress the bounding box to the frustum
                v1.x = std::max(-1.0f, std::min(v1.x, 1.0f));
                v1.y = std::max(-1.0f, std::min(v1.y, 1.0f));
                v1.z = std::max(0.0f, std::min(v1.z, 1.0f));
                v2.x = std::max(-1.0f, std::min(v2.x, 1.0f));
                v2.y = std::max(-1.0f, std::min(v2.y, 1.0f));
                v2.z = std::max(0.0f, std::min(v2.z, 1.0f));

                // Check that none of the dimensions is compressed to zero
                return v1.x != v2.x && v1.y != v2.y && v1.z != v2.z;
            };

            if (visible(std::move(cs_1), std::move(cs_2))) {
                render_sequence.push_back({id, mesh_node, cs_center});
            }
        }
    });

    // TODO ordering

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
        scene->ForEach<Mesh>([&](auto id, auto _, Mesh& mesh) {
            auto nodes_result = scene->GetAttachedNodes<Mesh>(id);
            if (!nodes_result || !nodes_result.value()) {
                return;
            }

            auto material_result =
                scene->GetAttachment<Material>(mesh.material);
            if (!material_result) {
                return;
            }
            auto& material = *material_result.value();

            static const char* vert = R"(
#version 450

#ifdef HAS_POSITIONS
layout(location = 0) in vec3 inPosition;
#endif

#ifdef HAS_NORMALS
layout(location = 1) in vec3 inNormal;
#endif

#ifdef HAS_TANGENTS
layout(location = 2) in vec3 inTangent;
#endif

#ifdef HAS_BITANGENTS
layout(location = 3) in vec3 inBitangent;
#endif

#ifdef HAS_COLORS
layout(location = 4) in vec4 inColor;
#endif

#ifdef HAS_UV0
layout(location = 5) in vec2 inUV0;
#endif

#ifdef HAS_UV1
layout(location = 6) in vec2 inUV1;
#endif

#ifdef HAS_UVW
layout(location = 7) in vec2 inUVW;
#endif

layout(location = 0) out vec2 outUV0;

layout(push_constant) uniform PC {
	mat4 mvp;
} pc;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 1.0);
    outUV0 = inUV0;
}
)";

            static const char* frag = R"(
#version 450
#extension GL_ARB_separate_shader_objects : enable

#ifdef HAS_DIFFUSE_MAP
layout(set = 0, binding = 0) uniform sampler2D diffuseTex;
#endif

#ifdef HAS_SPECULAR_MAP
layout(set = 0, binding = 1) uniform sampler2D specularTex;
#endif

#ifdef HAS_AMBIENT_MAP
layout(set = 0, binding = 2) uniform sampler2D ambientTex;
#endif

#ifdef HAS_EMISSIVE_MAP
layout(set = 0, binding = 3) uniform sampler2D emissiveTex;
#endif

#ifdef HAS_METALLIC_ROUGHNESS_MAP
layout(set = 0, binding = 4) uniform sampler2D metallicRoughnessTex;
#endif

#ifdef HAS_HEIGHT_MAP
layout(set = 0, binding = 5) uniform sampler2D heightTex;
#endif

#ifdef HAS_NORMAL_MAP
layout(set = 0, binding = 6) uniform sampler2D normalTex;
#endif

#ifdef HAS_SHININESS_MAP
layout(set = 0, binding = 7) uniform sampler2D shininessTex;
#endif

#ifdef HAS_OPACITY_MAP
layout(set = 0, binding = 8) uniform sampler2D opacityTex;
#endif

#ifdef HAS_DISPLACEMENT_MAP
layout(set = 0, binding = 9) uniform sampler2D displacementTex;
#endif

#ifdef HAS_LIGHT_MAP
layout(set = 0, binding = 10) uniform sampler2D lightTex;
#endif

#ifdef HAS_REFLECTION_MAP
layout(set = 0, binding = 11) uniform sampler2D reflectionTex;
#endif

layout(location = 0) in vec2 inUVs;
layout(location = 0) out vec4 outColor;

void main() {
#ifdef false
    outColor = texture(normalTex, vec2(1.0, -1.0) * inUVs);
#else
    outColor = texture(diffuseTex, vec2(1.0, -1.0) * inUVs);
#endif
}
)";

            auto pipeline_res = backend_->GetGraphicsPipeline(
                {vert, ShaderSourceType::Source, GetVertexShaderPreamble(mesh)},
                {frag, ShaderSourceType::Source,
                 GetFragmentShaderPreamble(material)});
            if (!pipeline_res) {
                return;
            }

            backend_->BindGraphicsPipeline(*pipeline_res.value());

            backend_->BindVertexInputFormat(*mesh.vertex_input_format);
            BindMeshBuffers(mesh);
            BindMaterialTextures(material);

            for (auto& mesh_node : *nodes_result.value()) {
                glm::mat4 mvp = vp * scene->GetCachedModel(mesh_node).value();
                backend_->BindVertexUniforms({std::move(mvp)});
            }

            if (!mesh.indices.empty()) {
                backend_->BindIndexBuffer(*mesh.buffers.index);
                backend_->DrawIndexed(
                    static_cast<uint32_t>(mesh.indices.size()));
            } else {
                backend_->Draw(static_cast<uint32_t>(mesh.vertices.size()));
            }
        });

        return outcome::success();
    };

    backend_->RenderFrame({std::move(forward_pass)}, "color");

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

        fs_preamble_map_[desc.int_repr] = std::move(preamble);
        return fs_preamble_map_[desc.int_repr].c_str();
    }
}

const char* Renderer::GetFragmentShaderPreamble(const Material& material) {
    auto check_type = [&](TextureType type) {
        return material.texture_bindings.find(type) !=
               material.texture_bindings.end();
    };

    return GetFragmentShaderPreamble(FragmentShaderPreambleDesc{
        check_type(TextureType::Diffuse), check_type(TextureType::Specular),
        check_type(TextureType::Ambient), check_type(TextureType::Emissive),
        check_type(TextureType::MetallicRoughness),
        check_type(TextureType::HeightMap), check_type(TextureType::NormalMap),
        check_type(TextureType::Shininess), check_type(TextureType::Opacity),
        check_type(TextureType::Displacement),
        check_type(TextureType::LightMap),
        check_type(TextureType::Reflection)});
}

result<void> Renderer::BindMeshBuffers(const Mesh& mesh) {
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
