#pragma once

#include "assimp/material.h"

#include "common/include.hpp"
#include "common/gen_vector.hpp"
#include "common/vulkan.hpp"
#include "scene/attachment.hpp"

namespace goma {

class Image;
struct Texture;

enum class TextureType {
    Diffuse,  // also Albedo for PBR
    Specular,
    Ambient,
    Emissive,
    MetallicRoughness,
    HeightMap,
    NormalMap,
    Shininess,
    Opacity,
    Displacement,
    LightMap,  // also OcclusionMap
    Reflection
};

struct TextureBinding {
    gen_id index;
    VkSamplerAddressMode wrapping = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    uint32_t uv_index = 0;
    float blend = 1.0f;
};

using TextureBindingMap =
    std::unordered_map<TextureType, std::vector<TextureBinding>>;

struct Material {
    std::string name;
    TextureBindingMap texture_bindings;

    glm::vec3 diffuse_color{0.0f};
    glm::vec3 specular_color{0.0f};
    glm::vec3 ambient_color{0.0f};
    glm::vec3 emissive_color{0.0f};
    glm::vec3 transparent_color{0.0f};

    bool two_sided = false;
    float opacity = 1.0f;
    float alpha_cutoff = 1.0f;
    float shininess_exponent = 0.0f;
    float specular_strength = 1.0f;
    float metallic_factor = 0.0f;
    float roughness_factor = 1.0f;

    struct {
        bool valid = false;
        Image* diffuse_tex = nullptr;
        Image* normal_tex = nullptr;
        Image* metallic_roughness_tex = nullptr;
        Image* ambient_tex = nullptr;
        Image* emissive_tex = nullptr;
        std::string preamble;
    } rhi;

    // Attachment component and convenience functions
    AttachmentComponent att_;
    void attach_to(Node& node) { att_.attach_to(node); }
    void detach_from(Node& node) { att_.detach_from(node); }
    void detach_all() { att_.detach_all(); }
    const std::vector<Node*>& attached_nodes() const {
        return att_.attached_nodes();
    }
};

}  // namespace goma
