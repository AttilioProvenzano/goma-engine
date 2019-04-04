#include "scene/scene.hpp"

#include "common/error_codes.hpp"

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

template <typename T>
std::ostream& operator<<(std::ostream& o, const Attachment<T>& a) {
    o << "Attachment(id: " << a.id << ", nodes: (";
    if (!a.nodes.empty()) {
        for (const auto& n : a.nodes) {
            o << n.id << ", ";
        }
        // Remove the last ", "
        o.seekp(o.tellp() - 2);
    }
    o << "))";
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
    if (id == GetRootNode()) {
        return Error::InvalidParentNode;
    }

    return nodes_[id.id].parent;
}

result<std::set<NodeIndex>> Scene::GetChildren(NodeIndex id) {
    if (!ValidateNode(id)) {
        return Error::InvalidNode;
    }
    return nodes_[id.id].children;
}

result<Transform> Scene::GetTransform(NodeIndex id) {
    if (!ValidateNode(id)) {
        return Error::InvalidNode;
    }
    return nodes_[id.id].transform;
}

result<void> Scene::SetTransform(NodeIndex id, const Transform& transform) {
    if (!ValidateNode(id)) {
        return Error::InvalidNode;
    }
    nodes_[id.id].transform = transform;
    return outcome::success();
}

bool Scene::HasCachedModel(NodeIndex id) {
    if (!ValidateNode(id)) {
        return false;
    }
    return static_cast<bool>(nodes_[id.id].cached_model);
}

result<glm::mat4> Scene::GetCachedModel(NodeIndex id) {
    if (!ValidateNode(id)) {
        return Error::InvalidNode;
    }
    return *nodes_[id.id].cached_model.get();
}

result<void> Scene::SetCachedModel(NodeIndex id, const glm::mat4& model) {
    if (!ValidateNode(id)) {
        return Error::InvalidNode;
    }
    nodes_[id.id].cached_model = std::make_unique<glm::mat4>(model);

    return outcome::success();
}

result<void> Scene::ComputeCachedModel(NodeIndex id) {
    std::stack<NodeIndex> node_stack;

    // Fill the stack with nodes for which we need
    // to compute model matrix. Always push the current node.
    auto current_node = id;
    do {
        node_stack.push(current_node);

        auto parent = GetParent(current_node);
        if (!parent) {
            break;
        } else {
            current_node = parent.value();
        }
    } while (!HasCachedModel(current_node));

    // Compute model matrices
    auto current_model = glm::mat4(1.0f);
    if (!node_stack.empty()) {
        auto parent_res = GetParent(node_stack.top());
        if (parent_res.has_value()) {
            auto& parent = parent_res.value();
            if (HasCachedModel(parent)) {
                current_model = GetCachedModel(parent).value();
            }
        }
    }

    while (!node_stack.empty()) {
        auto id = node_stack.top();
        node_stack.pop();

        auto transform = GetTransform(id).value();
        current_model = glm::scale(current_model, transform.scale);
        current_model = glm::mat4_cast(transform.rotation) * current_model;
        current_model = glm::translate(current_model, transform.position);
        SetTransform(id, transform);

        SetCachedModel(id, current_model);
    }

    return outcome::success();
}

result<void> Scene::InvalidateCachedModel(NodeIndex id) {
    if (!ValidateNode(id)) {
        return Error::InvalidNode;
    }
    nodes_[id.id].cached_model = std::unique_ptr<glm::mat4>();

    return outcome::success();
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
    nodes_[id.id].id = {nodes_[id.id].id.gen, 0};

    return outcome::success();
}

bool Scene::ValidateNode(NodeIndex id) {
    return id.gen != 0 && id.id < nodes_.size() &&
           nodes_[id.id].id.gen == id.gen;
}

}  // namespace goma
