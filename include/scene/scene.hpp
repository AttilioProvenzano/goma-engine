#pragma once

#include "scene/node.hpp"
#include "scene/attachment.hpp"

#include <outcome.hpp>
namespace outcome = OUTCOME_V2_NAMESPACE;
using outcome::result;

#include <queue>
#include <typeindex>
#include <unordered_map>

template <typename T>
using TypeMap = std::unordered_map<std::type_index, T>;

namespace goma {

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

    template <typename T>
    AttachmentManager<T>* GetAttachmentManager() {
        auto type_id = std::type_index(typeid(T));
        auto result = attachment_managers_.find(type_id);
        if (result != attachment_managers_.end()) {
            return static_cast<AttachmentManager<T>*>(result->second.get());
        }

        attachment_managers_[type_id] =
            std::make_unique<AttachmentManager<T>>();
        return static_cast<AttachmentManager<T>*>(
            attachment_managers_[type_id].get());
    }

  private:
    typedef TypeMap<std::unique_ptr<AttachmentManagerBase>>
        AttachmentManagerMap;

    std::vector<Node> nodes_;
    std::queue<size_t> recycled_nodes_;

    AttachmentManagerMap attachment_managers_;

    bool ValidateNode(NodeIndex id);
};  // namespace gomaclassScene

}  // namespace goma
