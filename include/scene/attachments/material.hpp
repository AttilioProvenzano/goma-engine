#pragma once

#include "scene/gen_index.hpp"
#include "renderer/handles.hpp"

#include <glm/glm.hpp>

#include <array>
#include <memory>
#include <unordered_map>
#include <vector>

namespace goma {

struct Texture;

struct TextureBinding {
    AttachmentIndex<Texture> index;
    uint32_t uv_index = 0;
    float blend = 1.0f;
    std::array<TextureWrappingMode, 3> wrapping = {TextureWrappingMode::Repeat,
        TextureWrappingMode::Repeat,
        TextureWrappingMode::Repeat};
};

typedef std::unordered_map<TextureType, std::vector<TextureBinding>>
    TextureBindingMap;

struct MaterialTextureImages {
    std::shared_ptr<Image> diffuse;  // also Albedo for PBR
    std::shared_ptr<Image> specular;
    std::shared_ptr<Image> ambient;
    std::shared_ptr<Image> emissive;
    std::shared_ptr<Image> metallic_roughness;
    std::shared_ptr<Image> height;
    std::shared_ptr<Image> normal;
    std::shared_ptr<Image> shininess;
    std::shared_ptr<Image> opacity;
    std::shared_ptr<Image> displacement;
    std::shared_ptr<Image> light;  // also OcclusionMap
    std::shared_ptr<Image> reflection;
};

struct Material {
    std::string name;
    TextureBindingMap texture_bindings;

    glm::vec3 diffuse_color = glm::vec3(0.0f);
    glm::vec3 specular_color = glm::vec3(0.0f);
    glm::vec3 ambient_color = glm::vec3(0.0f);
    glm::vec3 emissive_color = glm::vec3(0.0f);
    glm::vec3 transparent_color = glm::vec3(0.0f);

    bool two_sided = false;
    float opacity = 1.0f;
    float shininess_exponent = 0.0f;
    float specular_strength = 1.0f;

    MaterialTextureImages texture_images;
};

}  // namespace goma
