#pragma once

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

class Node {
  public:
    using NodePtr = std::unique_ptr<Node>;

    Node(const std::string& name = "", const Transform& t = {})
        : parent_(nullptr), name_(name), transform_(t) {}

    const std::string& name() const { return name_; }

    const Node* parent() const { return parent_; }
    Node* parent() { return parent_; }

    const std::vector<NodePtr>& children() const { return children_; }
    void set_children(std::vector<NodePtr>&& c) {
        children_.swap(std::move(c));
    }

    Node& add_child(const std::string& name = "", const Transform& t = {}) {
        auto child = std::make_unique<Node>(name, t);
        child->parent_ = this;

        children_.push_back(std::move(child));
        return *children_.back().get();
    }

    void remove_child(Node& child) {
        children_.erase(std::remove_if(children_.begin(), children_.end(),
                                       [&child](const auto& c) {
                                           return c.get() == &child;
                                       }),
                        children_.end());
    }

    void clear_children() { children_.clear(); }

    const Transform& transform() const { return transform_; }
    void set_transform(const Transform& t) {
        transform_ = t;
        invalidate_cached_transform_();
    }

    const glm::mat4& get_transform_matrix() {
        if (!cached_transform_.valid) {
            compute_transform_matrix_();
        }
        return cached_transform_.matrix;
    }

    Node* find(const std::string& name) {
        if (name_ == name) {
            return this;
        }

        for (const auto& c : children_) {
            auto found = c->find(name);
            if (found) {
                return found;
            }
        }

        return nullptr;
    }

    bool operator==(const Node& rhs) const {
        auto& lhs = *this;
        return lhs.name_ == rhs.name_ && lhs.parent_ == rhs.parent_ &&
               lhs.transform_ == rhs.transform_;
    }

    bool operator!=(const Node& rhs) const { return !(*this == rhs); }

  private:
    Node* parent_;
    std::string name_;
    std::vector<NodePtr> children_;

    Transform transform_;

    struct {
        glm::mat4 matrix;
        bool valid = false;
    } cached_transform_;

    void invalidate_cached_transform_() {
        cached_transform_.valid = false;

        for (auto& child : children_) {
            child->invalidate_cached_transform_();
        }
    }

    void compute_transform_matrix_() {
        std::vector<Node*> nodes_to_compute = {this};
        auto n = parent_;
        while (n != nullptr && !n->cached_transform_.valid) {
            nodes_to_compute.push_back(n);
            n = n->parent_;
        }

        auto current_matrix = n ? n->cached_transform_.matrix : glm::mat4{1.0f};

        std::for_each(nodes_to_compute.rbegin(), nodes_to_compute.rend(),
                      [&current_matrix](auto& n) {
                          const auto& t = n->transform_;
                          auto local_transform = glm::translate(t.position) *
                                                 glm::mat4_cast(t.rotation) *
                                                 glm::scale(t.scale);

                          current_matrix = current_matrix * local_transform;

                          n->cached_transform_.matrix = current_matrix;
                          n->cached_transform_.valid = true;
                      });
    }
};

}  // namespace goma
