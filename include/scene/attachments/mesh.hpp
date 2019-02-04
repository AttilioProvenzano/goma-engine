#pragma once

#include <glm/glm.hpp>

namespace goma {

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
};

}  // namespace goma
