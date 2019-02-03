#pragma once

#include "scene/node.hpp"
#include "scene/attachment.hpp"
#include "scene/attachments/texture.hpp"

#include <outcome.hpp>
namespace outcome = OUTCOME_V2_NAMESPACE;
using outcome::result;

#include <queue>
#include <typeindex>
#include <map>
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
    result<AttachmentIndex<T>> CreateAttachment(const NodeIndex& node_id,
                                                T&& data = T()) {
        return GetAttachmentManager<T>()->Create(node_id,
                                                 std::forward<T>(data));
    }

    template <typename T>
    result<AttachmentIndex<T>> CreateAttachment(T&& data = T()) {
        return GetAttachmentManager<T>()->Create(std::forward<T>(data));
    }

    template <typename T>
    result<void> RegisterAttachment(AttachmentIndex<T> attachment,
                                  const std::string& name,
                                  bool overwrite = true) {
        return GetAttachmentManager<T>()->Register(attachment, name, overwrite);
    }

    template <typename T>
    result<T*> GetAttachment(AttachmentIndex<T> id) {
        return GetAttachmentManager<T>()->Get(id);
    }

    template <typename T>
    result<std::pair<AttachmentIndex<T>, T*>> FindAttachment(
        const std::string& name) {
        return GetAttachmentManager<T>()->Find(name);
    }

    template <typename T>
    size_t GetAttachmentCount() {
        return GetAttachmentManager<T>()->count();
    }

  private:
    typedef TypeMap<std::unique_ptr<AttachmentManagerBase>>
        AttachmentManagerMap;

    std::vector<Node> nodes_;
    std::queue<size_t> recycled_nodes_;

    AttachmentManagerMap attachment_managers_;

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

    bool ValidateNode(NodeIndex id);
};

}  // namespace goma
