#pragma once

#include "common/include.hpp"
#include "scene/attachment.hpp"

namespace goma {

struct Camera {
    std::string name;

    float h_fov{60.0f};
    float near_plane{0.1f};
    float far_plane{1000.0f};
    float aspect_ratio{1.78f};

    glm::vec3 position{glm::vec3(0.0f)};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    glm::vec3 look_at{0.0f, 0.0f, 1.0f};

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
