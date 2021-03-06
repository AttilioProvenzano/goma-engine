#pragma once

#include "scene/gen_index.hpp"

#include "common/include.hpp"

namespace goma {

struct Transform {
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 scale;

    Transform(const glm::vec3& pos = glm::vec3(0.0),
              const glm::quat& rot = glm::quat(1.0, 0.0, 0.0, 0.0),
              const glm::vec3& scale_ = glm::vec3(1.0))
        : position(pos), rotation(rot), scale(scale_) {}

    bool operator==(const Transform& other) const {
        return (memcmp(this, &other, sizeof(*this)) == 0);
    }

    bool operator!=(const Transform& other) const {
        return (memcmp(this, &other, sizeof(*this)) != 0);
    }

    friend std::ostream& operator<<(std::ostream& o, const goma::Transform& t);
};

struct Node {
    NodeIndex id;
    NodeIndex parent;  // root node has parent {0, 0}
    Transform transform{};

    std::set<NodeIndex> children{};
    std::unique_ptr<glm::mat4> cached_model{};

    Node(NodeIndex id_, NodeIndex parent_,
         const Transform& transform_ = Transform())
        : id(id_), parent(parent_), transform(transform_) {}

    bool operator==(const Node& other) const { return (this->id == other.id); }

    bool operator!=(const Node& other) const { return (this->id != other.id); }

    bool valid() const { return id.valid(); }

    friend std::ostream& operator<<(std::ostream& o, const Node& n);
};

}  // namespace goma
