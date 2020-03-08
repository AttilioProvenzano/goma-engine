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

enum class VertexAttribute {
    Position,
    Normal,
    Tangent,
    Bitangent,
    Color,
    UV0,
    UV1
};

using VertexLayout = std::vector<VertexAttribute>;

struct Mesh {
    std::string name;

    struct {
        std::vector<uint8_t> data;
        uint32_t size = 0;
        VertexLayout layout = {};
    } vertices;
    std::vector<uint32_t> indices;

    Buffer* vertex_buffer = nullptr;
    Buffer* index_buffer = nullptr;

    AttachmentIndex<Material> material{};

    std::unique_ptr<Box> aabb{};
};

}  // namespace goma
