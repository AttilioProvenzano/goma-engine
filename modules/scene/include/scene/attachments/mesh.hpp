#pragma once

#include "scene/gen_index.hpp"

#include "common/include.hpp"

namespace goma {

class Buffer;
struct Material;

struct Box {
    glm::vec3 min{glm::vec3(std::numeric_limits<float>::max())};
    glm::vec3 max{glm::vec3(std::numeric_limits<float>::min())};
};

struct MeshBuffers {
    Buffer* vertex = nullptr;
    Buffer* normal = nullptr;
    Buffer* tangent = nullptr;
    Buffer* bitangent = nullptr;
    Buffer* color = nullptr;

    Buffer* index = nullptr;

    Buffer* uv0 = nullptr;
    Buffer* uv1 = nullptr;
    Buffer* uvw = nullptr;
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

    AttachmentIndex<Material> material{};

    // std::shared_ptr<VertexInputFormat> vertex_input_format;
    std::unique_ptr<Box> bounding_box{};
    MeshBuffers buffers{};
};

}  // namespace goma
