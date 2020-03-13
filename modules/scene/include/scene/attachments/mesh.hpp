#pragma once

#include "common/include.hpp"
#include "common/gen_vector.hpp"
#include "scene/attachment.hpp"

namespace goma {

class Buffer;

struct AABB {
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

    gen_id material_id;
    std::unique_ptr<AABB> aabb;

    struct {
        bool valid = false;
        Buffer* vertex_buffer = nullptr;
        Buffer* index_buffer = nullptr;
        std::string preamble;
    } rhi;

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
