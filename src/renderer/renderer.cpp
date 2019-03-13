#include "renderer/renderer.hpp"

#include "engine.hpp"
#include "renderer/vez/vez_backend.hpp"
#include "scene/attachments/camera.hpp"
#include "scene/attachments/mesh.hpp"

#include <stack>

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

    if (auto result = backend_->InitSurface(engine_->platform())) {
        LOGI("Surface initialized.");
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
        if (!mesh.uv_sets[0].empty()) {
            input_format_desc.bindings.push_back(
                {binding_id, sizeof(mesh.uv_sets[0][0])});
            input_format_desc.attributes.push_back(
                {binding_id, binding_id, Format::SFloatRG32, 0});
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
            std::stack<NodeIndex> node_stack;

            // Fill the stack with nodes for which we need
            // to compute model matrix
            auto current_node = mesh_node;
            while (!scene->HasCachedModel(current_node)) {
                node_stack.push(current_node);

                auto parent = scene->GetParent(current_node);
                if (!parent) {
                    break;
                } else {
                    current_node = parent.value();
                }
            }

            // Compute model matrices
            auto current_model = glm::mat4(1.0f);
            if (!node_stack.empty()) {
                auto parent_res = scene->GetParent(node_stack.top());
                if (parent_res.has_value()) {
                    auto& parent = parent_res.value();
                    if (scene->HasCachedModel(parent)) {
                        current_model = scene->GetCachedModel(parent).value();
                    }
                }
            }

            while (!node_stack.empty()) {
                auto node = node_stack.top();
                node_stack.pop();

                auto transform = scene->GetTransform(node).value();
                current_model = glm::scale(current_model, transform->scale);
                current_model =
                    glm::mat4_cast(transform->rotation) * current_model;
                current_model =
                    glm::translate(current_model, transform->position);

                scene->SetCachedModel(node, current_model);
            }
        }
    });

    // Upload textures
    scene->ForEach<Texture>([&](auto id, auto _, Texture& texture) {
        // TODO compressed textures
        if (!texture.compressed) {
            // TODO mipmaps
            TextureDesc tex_desc{texture.width, texture.height};
            auto image_res = backend_->CreateTexture(
                texture.path.c_str(), tex_desc, texture.data.data());

            if (image_res) {
                texture.image = image_res.value();
            }
        }
    });

    // Get the VP matrix
    auto camera_res = scene->GetAttachment<Camera>({0});  // TODO main camera
    if (!camera_res) {
        // TODO proper aspect ratio
        Camera camera = {};
        camera.aspect_ratio = 800.0f / 600.0f;

        auto new_camera_res =
            scene->CreateAttachment<Camera>(std::move(camera));
        if (!new_camera_res) {
            return Error::NoMainCamera;
        }

        auto new_camera_id = new_camera_res.value();
        camera_res = scene->GetAttachment<Camera>(new_camera_id);
        if (!camera_res) {
            return Error::NoMainCamera;
        }
    }

    // TODO proper camera transform
    // (also move cached transform to a function)
    auto& camera = camera_res.value();
    auto fovy =
        camera->h_fov / camera->aspect_ratio;  // TODO use proper aspect ratio
    glm::mat4 view =
        glm::lookAt(glm::vec3(0, 0, 3.0f), glm::vec3(0, 0, 0), camera->up);
    //	    glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1.0f), camera->up);
    glm::mat4 proj =
        glm::perspective(glm::radians(camera->h_fov), camera->aspect_ratio,
                         camera->near_plane, camera->far_plane);
    proj[1][1] *= -1;  // Vulkan-style projection

    glm::mat4 vp = proj * view;

    // Ensure framebuffers are created for all frames
    backend_->SetupFrames(3);

    if (framebuffers_.size() < 3) {
        FramebufferDesc fb_desc = {800, 600};
        for (uint32_t i = 0; i < 3; i++) {
            framebuffers_.push_back(
                backend_->CreateFramebuffer(i, "fb", fb_desc).value());
        }
    }

    auto frame_id = backend_->StartFrame().value();
    backend_->StartRenderPass(framebuffers_[frame_id], {});

    // Render meshes
    scene->ForEach<Mesh>([&](auto id, auto _, Mesh& mesh) {
        auto nodes_result = scene->GetAttachedNodes<Mesh>(id);
        if (!nodes_result || !nodes_result.value()) {
            return;
        }

        auto material_result = scene->GetAttachment<Material>(mesh.material);
        if (!material_result) {
            return;
        }
        auto& material = *material_result.value();

        static const char* vert = R"(
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUVs;

layout(location = 0) out vec2 outUVs;

layout(push_constant) uniform PC {
	mat4 mvp;
} pc;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 1.0);
    outUVs = inUVs;
}
)";

        static const char* frag = R"(
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform sampler2D mainTexture;

layout(location = 0) in vec2 inUVs;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(mainTexture, vec2(1.0, -1.0) * inUVs);
}
)";

        // TODO preamble based on textures/vertex input format
        auto pipeline_res = backend_->GetGraphicsPipeline(
            {vert, ShaderSourceType::Source, GetVertexShaderPreamble(mesh)},
            {frag, ShaderSourceType::Source,
             GetFragmentShaderPreamble(material)});
        if (!pipeline_res) {
            return;
        }
        backend_->BindGraphicsPipeline(*pipeline_res.value());

        // TODO support no index buffer
        backend_->BindVertexInputFormat(*mesh.vertex_input_format);
        backend_->BindVertexBuffers(
            {*mesh.buffers.vertex, *mesh.buffers.uv[0]});
        backend_->BindIndexBuffer(*mesh.buffers.index);

        auto diffuse_binding = material.textures.find(TextureType::Diffuse);
        if (diffuse_binding != material.textures.end() &&
            !diffuse_binding->second.empty()) {
            auto texture_res =
                scene->GetAttachment<Texture>(diffuse_binding->second[0].index);
            if (texture_res) {
                auto& texture = texture_res.value();
                backend_->BindTextures({*texture->image});
            }
        }

        for (auto& mesh_node : *nodes_result.value()) {
            backend_->BindVertexUniforms(
                {vp * scene->GetCachedModel(mesh_node).value()});
        }

        // TODO backend functions to set state
        VezDepthStencilState ds_state = {};
        ds_state.depthTestEnable = VK_TRUE;
        ds_state.depthCompareOp = VK_COMPARE_OP_LESS;
        ds_state.depthWriteEnable = VK_TRUE;
        vezCmdSetDepthStencilState(&ds_state);

        VezRasterizationState raster_state = {};
        raster_state.cullMode = VK_CULL_MODE_BACK_BIT;
        raster_state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        vezCmdSetRasterizationState(&raster_state);

        backend_->DrawIndexed(static_cast<uint32_t>(mesh.indices.size()));
    });

    backend_->FinishFrame();
    backend_->PresentImage("color");

    return outcome::success();
}

const char* Renderer::GetVertexShaderPreamble(
    const VertexShaderPreambleDesc& desc) {
    auto result = vs_preamble_map_.find(desc.int_repr);

    if (result != vs_preamble_map_.end()) {
        return result->second.c_str();
    } else {
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
            preamble += "#define HAS_REFLECTIONS_MAP\n";
        }

        fs_preamble_map_[desc.int_repr] = std::move(preamble);
        return fs_preamble_map_[desc.int_repr].c_str();
    }
}

const char* Renderer::GetFragmentShaderPreamble(const Material& material) {
    auto check_type = [&](TextureType type) {
        return material.textures.find(type) != material.textures.end();
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

}  // namespace goma
