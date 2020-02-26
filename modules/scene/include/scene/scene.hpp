#pragma once

#include "scene/node.hpp"
#include "scene/attachment.hpp"
#include "scene/attachments/texture.hpp"

#include "common/include.hpp"

template <typename T>
using TypeMap = std::unordered_map<std::type_index, T>;

namespace goma {

class Scene {
  public:
    Scene();

    result<NodeIndex> CreateNode(const NodeIndex parent,
                                 const Transform& transform = Transform());
    NodeIndex GetRootNode() { return nodes_[0].id; }
    void DeleteNode(NodeIndex id);

    result<NodeIndex> GetParent(NodeIndex id);
    result<std::set<NodeIndex>> GetChildren(NodeIndex id);
    result<Transform> GetTransform(NodeIndex id);
    result<glm::mat4> GetTransformMatrix(NodeIndex id);
    bool SetTransform(NodeIndex id, const Transform& transform);

    template <typename T>
    result<AttachmentIndex<T>> CreateAttachment(const NodeIndex& node_id,
                                                T&& data = T()) {
        return GetAttachmentManager<T>()->Create({node_id},
                                                 std::forward<T>(data));
    }

    template <typename T>
    result<AttachmentIndex<T>> CreateAttachment(T&& data = T()) {
        return GetAttachmentManager<T>()->Create(std::forward<T>(data));
    }

    template <typename T>
    bool RegisterAttachment(AttachmentIndex<T> attachment,
                            const std::string& name, bool overwrite = true) {
        return GetAttachmentManager<T>()->Register(attachment, name, overwrite);
    }

    template <typename T>
    void ForEach(std::function<void(const AttachmentIndex<T>,
                                    const std::set<NodeIndex>&, T&)>
                     fun) {
        return GetAttachmentManager<T>()->ForEach(std::move(fun));
    }

    template <typename T>
    void ForEach(std::function<void(T&)> fun) {
        return GetAttachmentManager<T>()->ForEach(std::move(fun));
    }

    template <typename T>
    const std::vector<Attachment<T>>& GetAttachments() {
        return GetAttachmentManager<T>()->GetAll();
    }

    template <typename T>
    result<std::reference_wrapper<T>> GetAttachment(AttachmentIndex<T> id) {
        return GetAttachmentManager<T>()->Get(id);
    }

    template <typename T>
    result<std::pair<AttachmentIndex<T>, std::reference_wrapper<T>>>
    FindAttachment(const std::string& name) {
        return GetAttachmentManager<T>()->Find(name);
    }

    template <typename T>
    size_t GetAttachmentCount() {
        return GetAttachmentManager<T>()->count();
    }

    template <typename T>
    bool Attach(AttachmentIndex<T> id, NodeIndex node) {
        return GetAttachmentManager<T>()->Attach(id, node);
    }

    template <typename T>
    bool Detach(AttachmentIndex<T> id, NodeIndex node) {
        return GetAttachmentManager<T>()->Detach(id, node);
    }

    template <typename T>
    result<void> DetachAll(AttachmentIndex<T> id) {
        return GetAttachmentManager<T>()->DetachAll(id);
    }

    template <typename T>
    result<std::reference_wrapper<std::set<NodeIndex>>> GetAttachedNodes(
        AttachmentIndex<T> id) {
        return GetAttachmentManager<T>()->GetNodes(id);
    }

  private:
    using AttachmentManagerMap =
        TypeMap<std::unique_ptr<AttachmentManagerBase>>;

    std::vector<Node> nodes_{};
    std::queue<size_t> recycled_nodes_{};

    AttachmentManagerMap attachment_managers_{};

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
    result<void> ComputeTransformMatrix(NodeIndex id);
};

}  // namespace goma
