#pragma once

#include "common/include.hpp"

namespace goma {

enum LightType {
    Directional,
    Point,
    Spot,
    Ambient,
    Area,
};

struct Light {
    std::string name;

    LightType type = LightType::Directional;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 direction = {0.0f, 0.0f, 1.0f};
    glm::vec3 up = {0.0f, 1.0f, 0.0f};

    glm::vec3 diffuse_color = glm::vec3(1.0f);
    glm::vec3 specular_color = glm::vec3(1.0f);
    glm::vec3 ambient_color = glm::vec3(1.0f);

    std::array<float, 3> attenuation = {1.0f, 1.0f, 1.0f};
    float inner_cone_angle = 360.0f;
    float outer_cone_angle = 360.0f;
    glm::vec2 area_size = glm::vec2(0.0f);
};

}  // namespace goma
