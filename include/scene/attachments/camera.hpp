#pragma once

#include <glm/glm.hpp>

#define _USE_MATH_DEFINES
#include <math.h>
#include <string>

namespace goma {

struct Camera {
    std::string name;

    float h_fov = 60.0f;
    float near_plane = 0.1f;
    float far_plane = 1000.0f;
    float aspect_ratio = 0.0f;

	glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 up = {0.0f, 1.0f, 0.0f};
    glm::vec3 look_at = {0.0f, 0.0f, 1.0f};
};

}  // namespace goma
