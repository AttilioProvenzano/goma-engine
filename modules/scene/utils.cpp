#include "scene/utils.hpp"

#include "scene/attachments/mesh.hpp"

namespace goma {
namespace utils {

size_t GetSize(VertexAttribute attribute) {
    static const std::unordered_map<VertexAttribute, size_t> width_map = {
        {VertexAttribute::Position, sizeof(glm::vec3)},
        {VertexAttribute::Normal, sizeof(glm::vec3)},
        {VertexAttribute::Tangent, sizeof(glm::vec3)},
        {VertexAttribute::Bitangent, sizeof(glm::vec3)},
        {VertexAttribute::Color, sizeof(glm::vec4)},
        {VertexAttribute::UV0, sizeof(glm::vec2)},
        {VertexAttribute::UV1, sizeof(glm::vec2)},
    };
    return width_map.at(attribute);
}

size_t GetStride(const VertexLayout& layout) {
    return std::accumulate(
        layout.begin(), layout.end(), size_t{0},
        [](size_t acc, VertexAttribute attr) { return acc + GetSize(attr); });
}

size_t GetOffset(const VertexLayout& layout, VertexAttribute attribute) {
    auto a = std::find(layout.begin(), layout.end(), attribute);
    if (a == layout.end()) {
        return SIZE_MAX;
    }

    // Accumulate until the element just before the one we found
    return std::accumulate(
        layout.begin(), a, size_t{0},
        [](size_t acc, VertexAttribute attr) { return acc + GetSize(attr); });
}

}  // namespace utils
}  // namespace goma
