#pragma once

#include <queue>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace goma {

struct GenIndex {
    size_t id;
    size_t gen;  // 0 reserved for invalid elements

    GenIndex(size_t id_ = 0, size_t gen_ = 1) : id(id_), gen(gen_) {}

    bool operator==(const GenIndex& other) const {
        return (memcmp(this, &other, sizeof(this)) == 0);
    }

    friend std::ostream& operator<<(std::ostream& o, const goma::GenIndex& id);
};
typedef GenIndex NodeIndex;
typedef GenIndex AttachmentIndex;

struct Transform {
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 scale;

    Transform(const glm::vec3& pos = glm::vec3(0.0),
              const glm::quat& rot = glm::quat(1.0, 0.0, 0.0, 0.0),
              const glm::vec3& scale_ = glm::vec3(1.0))
        : position(pos), rotation(rot), scale(scale_) {}

    bool operator==(const Transform& other) const {
        return (memcmp(this, &other, sizeof(this)) == 0);
    }

    friend std::ostream& operator<<(std::ostream& o, const goma::Transform& t);
};

struct Node {
    NodeIndex id;
    NodeIndex parent_id;  // root node has parent_id {0, 0}
    Transform transform;

    Node(const NodeIndex& id_ = NodeIndex(),
         const NodeIndex& parent_id_ = NodeIndex(),
         const Transform& transform_ = Transform())
        : id(id_), parent_id(parent_id_), transform(transform_) {}

    bool operator==(const Node& other) const {
        return (memcmp(this, &other, sizeof(this)) == 0);
    }

    friend std::ostream& operator<<(std::ostream& o, const goma::Node& n);
};

struct Attachment {
    AttachmentIndex id;
    NodeIndex node_id;

    Attachment(const AttachmentIndex& id_ = AttachmentIndex(),
               const NodeIndex& node_id_ = NodeIndex())
        : id(id_), node_id(node_id_) {}

    bool operator==(const Attachment& other) const {
        return (memcmp(this, &other, sizeof(this)) == 0);
    }
};

class Scene {
  public:
    Scene();

    Node* CreateNode(const Node* parent,
                     const Transform& transform = Transform());
    Node* CreateNode(const NodeIndex parent_id,
                     const Transform& transform = Transform());

    Node* GetRootNode() { return &nodes_[0]; }
    Node* GetNode(NodeIndex id);
    void DeleteNode(NodeIndex id);

    Attachment* CreateAttachment(const Node* node);
    Attachment* CreateAttachment(const NodeIndex node_id);

  private:
    std::vector<Node> nodes_;
    std::queue<size_t> recycled_nodes_;

    std::vector<Attachment> attachments_;
    std::queue<size_t> recycled_attachments_;
};

}  // namespace goma
