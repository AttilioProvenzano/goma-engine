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
    o << "Node(id: " << n.id << ", parent: " << n.parent_id << ")";
    return o;
}

Scene::Scene() { nodes_.emplace_back(NodeIndex{0}, NodeIndex{0, 0}); }

// TODO convert to result<NodeIndex>, as pointer might be invalid after realloc
Node* Scene::CreateNode(const Node* parent, const Transform& transform) {
    if (parent) {
        return CreateNode(parent->id, transform);
    }
    return nullptr;
}

Node* Scene::CreateNode(const NodeIndex parent_id, const Transform& transform) {
    if (!recycled_nodes_.empty()) {
        size_t index = recycled_nodes_.front();
        recycled_nodes_.pop();

        // The last valid generation was stored in id.id
        // when the node was deleted
        Node* node = &nodes_[index];
        node->id = {index, node->id.id + 1};
        node->parent_id = parent_id;
        node->transform = transform;
        return node;
    } else {
        size_t id = nodes_.size();
        nodes_.emplace_back(NodeIndex{id}, parent_id, transform);
        return &nodes_.back();
    }
}

Node* Scene::GetNode(NodeIndex id) {
    if (id.id < nodes_.size()) {
        Node* node = &nodes_[id.id];
        if (node->id.gen == id.gen) {
            return node;
        }
    }
    return nullptr;
}

void Scene::DeleteNode(NodeIndex id) {
    // Root node cannot be deleted
    if (id.id > 0 && id.id < nodes_.size()) {
        Node* node = &nodes_[id.id];
        if (node->id.gen == id.gen) {
            // Fix parent for any child node(s)
            for (auto& child_node : nodes_) {
                if (child_node.parent_id == id) {
                    child_node.parent_id = node->parent_id;
                }
            }

            // We add the node's index to the recycled nodes
            recycled_nodes_.push(id.id);

            // We set generation to 0 to mark the node as
            // invalid and store the last valid generation into the node's id
            // (which is unused while the node is invalid)
            node->id.id = node->id.gen;
            node->id.gen = 0;
        }
    }
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

}  // namespace goma
