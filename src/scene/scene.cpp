#include "scene/scene.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <iostream>

namespace goma {

std::ostream& operator<<(std::ostream& o, const goma::GenIndex& id) {
    o << "{id: " << id.id << ", gen: " << id.gen << "}";
    return o;
}

std::ostream& operator<<(std::ostream& o, const goma::Transform& t) {
    o << "{pos: " << glm::to_string(t.position) << "," << std::endl
      << "rot : " << glm::to_string(t.rotation) << "," << std::endl
      << "scale: " << glm::to_string(t.scale) << "}";
    return o;
}

std::ostream& operator<<(std::ostream& o, const goma::Node& n) {
    o << "Node(id: " << n.id << ", parent: " << n.parent << ")";
    return o;
}

Scene::Scene() { nodes_.emplace_back(NodeIndex{0}, NodeIndex{0, 0}); }

result<NodeIndex> Scene::CreateNode(const NodeIndex parent,
                                    const Transform& transform) {
    if (!ValidateNode(parent)) {
        return Error::InvalidParentNode;
    }

    NodeIndex ret_id;
    if (!recycled_nodes_.empty()) {
        size_t index = recycled_nodes_.front();
        recycled_nodes_.pop();

        // The last valid generation was stored in id.id
        // when the node was deleted
        size_t new_gen = nodes_[index].id.id + 1;

        nodes_[index] = {{index, new_gen}, parent, transform};
        ret_id = {index, new_gen};
    } else {
        size_t id = nodes_.size();
        nodes_.emplace_back(NodeIndex{id}, parent, transform);
        ret_id = {id};
    }

    nodes_[parent.id].children.insert(ret_id);
    return ret_id;
}

result<NodeIndex> Scene::GetParent(NodeIndex id) {
    if (!ValidateNode(id)) {
        return Error::InvalidNode;
    }
    return nodes_[id.id].parent;
}

result<const std::set<NodeIndex>*> Scene::GetChildren(NodeIndex id) {
    if (!ValidateNode(id)) {
        return Error::InvalidNode;
    }
    return &nodes_[id.id].children;
}

result<Transform*> Scene::GetTransform(NodeIndex id) {
    if (!ValidateNode(id)) {
        return Error::InvalidNode;
    }
    return &nodes_[id.id].transform;
}

result<void> Scene::DeleteNode(NodeIndex id) {
    // Root node cannot be deleted
    if (id.id == 0) {
        return Error::RootNodeCannotBeDeleted;
    }

    // Validate id
    if (!ValidateNode(id)) {
        return Error::InvalidNode;
    }

    // Fix parent for any child node(s)
    auto new_parent = nodes_[id.id].parent;
    nodes_[new_parent.id].children.erase(id);
    for (auto& child_node : nodes_) {
        if (child_node.parent == id) {
            child_node.parent = new_parent;
            nodes_[new_parent.id].children.insert(child_node.id);
        }
    }

    // We add the node's index to the recycled nodes
    recycled_nodes_.push(id.id);

    // We set generation to 0 to mark the node as
    // invalid and store the last valid generation into the node's id
    // (which is unused while the node is invalid)
    nodes_[id.id] = {{nodes_[id.id].id.gen, 0}};

    return outcome::success();
}

Attachment* Scene::CreateAttachment(const Node* node) {
    if (node) {
        return CreateAttachment(node->id);
    }
    return nullptr;
}

Attachment* Scene::CreateAttachment(const NodeIndex node_id) {
    size_t id = attachments_.size();
    attachments_.emplace_back(AttachmentIndex{id}, node_id);
    return &attachments_.back();
}

bool Scene::ValidateNode(NodeIndex id) {
    return id.gen != 0 && id.id < nodes_.size() &&
           nodes_[id.id].id.gen == id.gen;
}

}  // namespace goma
