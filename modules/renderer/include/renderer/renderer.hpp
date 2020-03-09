#pragma once

#include "rhi/device.hpp"

#include "common/include.hpp"
#include "common/hash.hpp"

namespace std {

template <>
struct hash<goma::ShaderDesc> {
    size_t operator()(const goma::ShaderDesc& desc) const {
        size_t seed = 0;

        // Hash either the path or the source, depending on what is present
        if (!desc.name.empty()) {
            goma::hash_combine(seed, goma::djb2_hash(desc.name.c_str()));
        } else {
            goma::hash_combine(seed, goma::djb2_hash(desc.source.c_str()));
        }

        goma::hash_combine(seed, goma::djb2_hash(desc.preamble.c_str()));
        goma::hash_combine(seed, desc.stage);

        return seed;
    };
};

}  // namespace std

namespace goma {

class Engine;
class GraphicsContext;
class Scene;

class Renderer {
  public:
    Renderer(Engine& engine);

    result<void> Render();

    // TODO: skybox and basic shapes will be handled in Scene
    // result<void> CreateSkybox();
    // result<void> CreateSphere();

    // TODO: BRDF lut handled internally by the material system
    // result<void> CreateBRDFLut();

  private:
    Engine& engine_;
    std::unique_ptr<Device> device_{};

    using ShaderMap = std::unordered_map<ShaderDesc, Shader*>;
    ShaderMap shader_map;

    void RenderMeshes(GraphicsContext& ctx, Scene& scene);

    // TODO: most of the following will be moved to rendering pipeline

    /*
        std::map<uint32_t, std::string> vs_preamble_map_{};
        std::map<uint32_t, std::string> fs_preamble_map_{};
        std::unique_ptr<glm::mat4> vp_hold{};
        uint32_t downscale_index_{0};
        uint32_t upscale_index_{0};
        uint32_t skybox_mip_count{0};

        struct RenderSequenceElement {
            AttachmentIndex<Mesh> mesh;
            NodeIndex node;
            glm::vec3 cs_center;
        };
        using RenderSequence = std::vector<RenderSequenceElement>;

        struct LightData {
            glm::vec3 direction;
            int32_t type;

            glm::vec3 color;
            float intensity;

            glm::vec3 position;
            float range;

            float innerConeCos;
            float outerConeCos;

            glm::vec2 padding;
        };

        static constexpr size_t kMaxLights{64};
        struct LightBufferData {
            glm::vec3 ambient_color{glm::vec3(0.0f)};
            int32_t num_lights{0};
            std::array<int32_t, 4> shadow_ids{-1};

            std::array<LightData, kMaxLights> lights{};
        };

        // Rendering setup
        void CreateMeshBuffers(Scene& scene);
        void CreateVertexInputFormats(Scene& scene);
        void UploadTextures(Scene& scene);
        RenderSequence Cull(Scene& scene, const RenderSequence& render_seq,
                            const glm::mat4& vp);
        LightBufferData GetLightBufferData(Scene& scene);

        // Render passes
        result<void> UpdateLightBuffer(FrameIndex frame_id, Scene& scene,
                                       const LightBufferData&
       light_buffer_data); result<void> ShadowPass(FrameIndex frame_id, Scene&
       scene, const RenderSequence& render_seq, const glm::mat4& shadow_vp);
        result<void> ForwardPass(FrameIndex frame_id, Scene& scene,
                                 const RenderSequence& render_seq,
                                 const glm::vec3& camera_ws_pos,
                                 const glm::mat4& camera_vp,
                                 const glm::mat4& shadow_vp);
        result<void> DownscalePass(FrameIndex frame_id, const std::string& src,
                                   const std::string& dst);
        result<void> UpscalePass(FrameIndex frame_id, const std::string& src,
                                 const std::string& dst);
        result<void> PostprocessingPass(FrameIndex frame_id);

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
        const char* GetVertexShaderPreamble(const VertexShaderPreambleDesc&
       desc); const char* GetVertexShaderPreamble(const Mesh& mesh);

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
                bool alpha_mask : 1;
            };

            uint32_t int_repr;
        };
        const char* GetFragmentShaderPreamble(
            const FragmentShaderPreambleDesc& desc);
        const char* GetFragmentShaderPreamble(const Mesh& mesh,
                                              const Material& material);

        result<void> BindMeshBuffers(const Mesh& mesh);
        result<void> BindMaterialTextures(const Material& material);

    */
};

}  // namespace goma
