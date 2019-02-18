#pragma once

#include <glm/glm.hpp>

namespace goma {

struct Box {
    glm::vec3 min = glm::vec3(0.0f);
    glm::vec3 max = glm::vec3(0.0f);
};

struct Mesh {
    std::string name;

    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec3> tangents;
    std::vector<glm::vec3> bitangents;

    std::vector<uint32_t> indices;

    std::vector<glm::vec4> colors;
    std::vector<std::vector<glm::vec2>> uv_sets;
    std::vector<std::vector<glm::vec3>> uvw_sets;

    Box bounding_box = {};
};

}  // namespace goma
