#pragma once

#include "scene/attachments.hpp"
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

    bool valid() const { return gen > 0; }

    bool operator==(const GenIndex& other) const {
        return (memcmp(this, &other, sizeof(this)) == 0);
    }

    bool operator<(const GenIndex& other) const {
        return (memcmp(this, &other, sizeof(this)) < 0);
    }

    friend std::ostream& operator<<(std::ostream& o, const goma::GenIndex& id);
};

typedef GenIndex NodeIndex;

template <typename T>
using AttachmentIndex = GenIndex;

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
    Transform transform = Transform();
    std::set<NodeIndex> children = {};

    Node(NodeIndex id_, NodeIndex parent_,
         const Transform& transform_ = Transform())
        : id(id_), parent(parent_), transform(transform_) {}

    bool operator==(const Node& other) const { return (this->id == other.id); }

    bool valid() const { return id.valid(); }

    friend std::ostream& operator<<(std::ostream& o, const goma::Node& n);
};

template <typename T>
struct Attachment {
    AttachmentIndex<T> id;
    NodeIndex node_id;
    T data = {};

    Attachment(AttachmentIndex<T> id_, NodeIndex node_id_, const T& data_ = T())
        : id(id_), node_id(node_id_), data(data_) {}

    Attachment(AttachmentIndex<T> id_, NodeIndex node_id_,
               const T&& data_ = T())
        : id(id_), node_id(node_id_), data(std::move(data_)) {}

    bool operator==(const Attachment<T>& other) const {
        return (this->id == other.id);
    }
};

template <typename T>
class AttachmentManager {
  public:
    result<AttachmentIndex<T>> CreateAttachment(const NodeIndex node_id,
                                                const T&& data = T()) {
        if (!node_id.valid()) {
            return Error::InvalidNode;
        }

        AttachmentIndex<T> ret_id;
        if (!recycled_attachments_.empty()) {
            size_t index = recycled_attachments_.front();
            recycled_attachments_.pop();

            // The last valid generation was stored in id.id
            // when the attachment was deleted
            size_t new_gen = attachments_[index].id.id + 1;

            attachments_[index] = {{index, new_gen}, node_id, std::move(data)};
            ret_id = {index, new_gen};
        } else {
            size_t id = attachments_.size();
            attachments_.emplace_back(AttachmentIndex<T>{id}, node_id,
                                      std::move(data));
            ret_id = {id};
        }
        return ret_id;
    };

  private:
    std::vector<Attachment<Texture>> attachments_;
    std::queue<size_t> recycled_attachments_;
};

class Scene {
  public:
    Scene();

    result<NodeIndex> CreateNode(const NodeIndex parent,
                                 const Transform& transform = Transform());
    NodeIndex GetRootNode() { return nodes_[0].id; }
    result<void> DeleteNode(NodeIndex id);

    result<NodeIndex> GetParent(NodeIndex id);
    result<std::set<NodeIndex>> GetChildren(NodeIndex id);
    result<Transform*> GetTransform(NodeIndex id);

    auto& texture_manager() { return texture_manager_; }

  private:
    std::vector<Node> nodes_;
    std::queue<size_t> recycled_nodes_;

    AttachmentManager<Texture> texture_manager_;

    bool ValidateNode(NodeIndex id);
};

}  // namespace goma
