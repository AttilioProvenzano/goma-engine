#pragma once

#include "common/include.hpp"
#include "scene/node.hpp"

namespace goma {

// Each attachment will forward calls to its AttachmentComponent
class AttachmentComponent {
  public:
    ~AttachmentComponent() {
        for (auto node : attached_nodes_) {
            detach_from(*node);
        }
    }

    void attach_to(Node& node) { attached_nodes_.push_back(&node); }
    void detach_from(Node& node) {
        attached_nodes_.erase(
            std::remove(attached_nodes_.begin(), attached_nodes_.end(), &node),
            attached_nodes_.end());
    }
    void detach_all() { attached_nodes_.clear(); }

    const std::vector<Node*>& attached_nodes() const { return attached_nodes_; }

  private:
    std::vector<Node*> attached_nodes_;
};

}  // namespace goma
