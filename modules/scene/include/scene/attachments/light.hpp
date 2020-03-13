#pragma once

#include "common/include.hpp"
#include "scene/attachment.hpp"

namespace goma {

enum LightType {
    Directional = 0,
    Point = 1,
    Spot = 2,
    Ambient = 3,
    Area = 4,
};

struct Light {
    std::string name;

    LightType type{LightType::Directional};
    glm::vec3 position{glm::vec3(0.0f)};
    glm::vec3 direction{0.0f, 0.0f, 1.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};

    float intensity{1.0f};
    glm::vec3 diffuse_color{glm::vec3(1.0f)};
    glm::vec3 specular_color{glm::vec3(1.0f)};
    glm::vec3 ambient_color{glm::vec3(1.0f)};

    std::array<float, 3> attenuation{1.0f, 1.0f, 1.0f};
    float inner_cone_angle{360.0f};
    float outer_cone_angle{360.0f};
    glm::vec2 area_size{glm::vec2(0.0f)};

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
