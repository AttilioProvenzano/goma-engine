#pragma once

#include "renderer/backend.hpp"

#include "common/include.hpp"

namespace goma {

class Engine;

class Renderer {
  public:
    Renderer(Engine* engine);

    result<void> Render();

    result<void> CreateSkybox();
    result<void> CreateSphere();

  private:
    Engine* engine_ = nullptr;
    std::unique_ptr<Backend> backend_;

    union VertexShaderPreambleDesc {
        struct {
            bool has_positions : 1;
            bool has_normals : 1;
            bool has_tangents : 1;
            bool has_bitangents : 1;
            bool has_colors : 1;
            bool has_uv0 : 1;
            bool has_uv1 : 1;
            bool has_uvw : 1;
        };

        uint32_t int_repr;
    };

    std::map<uint32_t, std::string> vs_preamble_map_;
    const char* GetVertexShaderPreamble(const VertexShaderPreambleDesc& desc);
    const char* GetVertexShaderPreamble(const Mesh& mesh);

    union FragmentShaderPreambleDesc {
        struct {
            // Mesh
            bool has_positions : 1;
            bool has_normals : 1;
            bool has_tangents : 1;
            bool has_bitangents : 1;
            bool has_colors : 1;
            bool has_uv0 : 1;
            bool has_uv1 : 1;
            bool has_uvw : 1;

            // Material
            bool has_diffuse_map : 1;
            bool has_specular_map : 1;
            bool has_ambient_map : 1;
            bool has_emissive_map : 1;
            bool has_metallic_roughness_map : 1;
            bool has_height_map : 1;
            bool has_normal_map : 1;
            bool has_shininess_map : 1;
            bool has_opacity_map : 1;
            bool has_displacement_map : 1;
            bool has_light_map : 1;
            bool has_reflection_map : 1;
        };

        uint32_t int_repr;
    };

    std::map<uint32_t, std::string> fs_preamble_map_;
    const char* GetFragmentShaderPreamble(
        const FragmentShaderPreambleDesc& desc);
    const char* GetFragmentShaderPreamble(const Mesh& mesh,
                                          const Material& material);

    result<void> BindMeshBuffers(const Mesh& mesh);
    result<void> BindMaterialTextures(const Material& material);

    std::unique_ptr<glm::mat4> vp_hold;
};

}  // namespace goma
