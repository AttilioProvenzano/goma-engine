#pragma once

#include "common/error_codes.hpp"

#include <queue>
#include <vector>
#include <set>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <outcome.hpp>
namespace outcome = OUTCOME_V2_NAMESPACE;
using outcome::result;
#include <vector>

namespace goma {

struct GenIndex {
    size_t id;
    size_t gen;  // 0 reserved for invalid elements

    GenIndex(size_t id_ = 0, size_t gen_ = 1) : id(id_), gen(gen_) {}

    bool operator==(const GenIndex& other) const {
        return (memcmp(this, &other, sizeof(this)) == 0);
    }

	bool operator<(const GenIndex& other) const {
        return (memcmp(this, &other, sizeof(this)) < 0);
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
    NodeIndex parent;  // root node has parent {0, 0}
    std::set<NodeIndex> children;

    Transform transform;

    Node(const NodeIndex& id_ = NodeIndex(),
         const NodeIndex& parent_ = NodeIndex(),
         const Transform& transform_ = Transform())
        : id(id_), parent(parent_), transform(transform_) {}

    bool operator==(const Node& other) const {
        return (memcmp(this, &other, sizeof(this)) == 0);
    }

    bool valid() const { return id.gen > 0; }

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

    result<NodeIndex> CreateNode(const NodeIndex parent,
                                 const Transform& transform = Transform());
    NodeIndex GetRootNode() { return nodes_[0].id; }
    result<void> DeleteNode(NodeIndex id);

    result<NodeIndex> GetParent(NodeIndex id);
    result<const std::set<NodeIndex>*> GetChildren(NodeIndex id);
    result<Transform*> GetTransform(NodeIndex id);

    Attachment* CreateAttachment(const Node* node);
    Attachment* CreateAttachment(const NodeIndex node_id);

  private:
    std::vector<Node> nodes_;
    std::queue<size_t> recycled_nodes_;

    std::vector<Attachment> attachments_;
    std::queue<size_t> recycled_attachments_;

    bool ValidateNode(NodeIndex id);
};

}  // namespace goma
