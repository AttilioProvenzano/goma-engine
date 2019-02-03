#pragma once

#include "scene/attachment.hpp"

#include <array>
#include <unordered_map>
#include <vector>

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
    Reflection,
};

enum class TextureOp {
    Multiply,
    Add,
    Subtract,
    Divide,
    SmoothAdd,  // (T1 + T2) - (T1 * T2)
    SignedAdd   // T1 + (T2 - 0.5)
};

enum class TextureWrappingMode { Repeat, MirroredRepeat, ClampToEdge, Decal };

struct TextureBinding {
    AttachmentIndex<Texture> index;
    uint32_t uv_index = 0;
    float blend = 1.0f;
    std::array<TextureWrappingMode, 3> wrapping = {TextureWrappingMode::Repeat,
                                                   TextureWrappingMode::Repeat,
                                                   TextureWrappingMode::Repeat};
};

typedef std::unordered_map<TextureType, std::vector<TextureBinding>>
    MaterialTextureMap;

struct Material {
    std::string name;
    MaterialTextureMap textures;

    glm::vec3 diffuse_color = glm::vec3(0.0f);
    glm::vec3 specular_color = glm::vec3(0.0f);
    glm::vec3 ambient_color = glm::vec3(0.0f);
    glm::vec3 emissive_color = glm::vec3(0.0f);
    glm::vec3 transparent_color = glm::vec3(0.0f);

    bool two_sided = false;
    float opacity = 1.0f;
    float shininess_exponent = 0.0f;
    float specular_strength = 1.0f;
};

}  // namespace goma
