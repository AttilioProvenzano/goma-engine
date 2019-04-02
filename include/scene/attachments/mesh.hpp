#pragma once

#include "scene/gen_index.hpp"
#include "renderer/handles.hpp"

#include "common/include.hpp"

namespace goma {

struct Material;

struct Box {
    glm::vec3 min = glm::vec3(0.0f);
    glm::vec3 max = glm::vec3(0.0f);
};

struct MeshBuffers {
    std::shared_ptr<Buffer> vertex;
    std::shared_ptr<Buffer> normal;
    std::shared_ptr<Buffer> tangent;
    std::shared_ptr<Buffer> bitangent;
    std::shared_ptr<Buffer> color;

    std::shared_ptr<Buffer> index;

    std::shared_ptr<Buffer> uv0;
    std::shared_ptr<Buffer> uv1;
    std::shared_ptr<Buffer> uvw;
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

    AttachmentIndex<Material> material = {};

    std::shared_ptr<VertexInputFormat> vertex_input_format;
    Box bounding_box = {};
    MeshBuffers buffers = {};
};

}  // namespace goma
