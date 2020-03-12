#pragma once

#include "assimp/material.h"

#include "scene/gen_index.hpp"

#include "common/include.hpp"
#include "common/vulkan.hpp"

namespace goma {

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
    AttachmentIndex<Texture> index;
    VkSamplerAddressMode wrapping = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    uint32_t uv_index = 0;
    float blend = 1.0f;
};

using TextureBindingMap =
    std::unordered_map<TextureType, std::vector<TextureBinding>>;

struct Material {
    std::string name;
    TextureBindingMap texture_bindings;

    glm::vec3 diffuse_color{glm::vec3(0.0f)};
    glm::vec3 specular_color{glm::vec3(0.0f)};
    glm::vec3 ambient_color{glm::vec3(0.0f)};
    glm::vec3 emissive_color{glm::vec3(0.0f)};
    glm::vec3 transparent_color{glm::vec3(0.0f)};

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
};

}  // namespace goma
