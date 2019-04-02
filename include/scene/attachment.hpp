#pragma once

#include "scene/gen_index.hpp"

#include "common/include.hpp"

namespace goma {

template <typename T>
struct Attachment {
    AttachmentIndex<T> id;
    std::set<NodeIndex> nodes;
    T data = {};

    Attachment(AttachmentIndex<T> id_, const std::set<NodeIndex>& nodes_,
               T&& data_ = T())
        : id(id_), nodes(nodes_), data(std::forward<T>(data_)) {}

    bool operator==(const Attachment<T>& other) const {
        return (this->id == other.id);
    }

    bool operator!=(const Attachment<T>& other) const {
        return (this->id != other.id);
    }

    bool valid() const { return id.valid(); }

    friend std::ostream& operator<<(std::ostream& o, const Attachment<T>& n);
};

class AttachmentManagerBase {
  public:
    virtual ~AttachmentManagerBase() = default;
};

template <typename T>
class AttachmentManager : public AttachmentManagerBase {
  public:
    AttachmentManager() : valid_count_(0) {}
    virtual ~AttachmentManager() = default;

    result<AttachmentIndex<T>> Create(const std::set<NodeIndex>& nodes,
                                      T&& data = T()) {
        AttachmentIndex<T> ret_id;
        if (!recycled_attachments_.empty()) {
            size_t index = recycled_attachments_.front();
            recycled_attachments_.pop();

            // The last valid generation was stored in id.id
            // when the attachment was deleted
            size_t new_gen = attachments_[index].id.id + 1;

            attachments_[index] = {
                {index, new_gen}, nodes, std::forward<T>(data)};
            ret_id = {index, new_gen};
        } else {
            size_t id = attachments_.size();
            attachments_.emplace_back(AttachmentIndex<T>{id}, nodes,
                                      std::forward<T>(data));
            ret_id = {id};
        }
        valid_count_++;
        return ret_id;
    }

    result<AttachmentIndex<T>> Create(T&& data = T()) {
        return Create({}, std::forward<T>(data));
    }

    result<void> Register(AttachmentIndex<T> attachment,
                          const std::string& name, bool overwrite = true) {
        if (!overwrite) {
            auto result = attachment_map_.find(name);
            if (result != attachment_map_.end()) {
                return Error::KeyAlreadyExists;
            }
        }

        attachment_map_[name] = attachment;
        return outcome::success();
    }

    void ForEach(std::function<void(const AttachmentIndex<T>&,
                                    const std::set<NodeIndex>&, T&)>
                     fun) {
        for (auto& a : attachments_) {
            if (!a.valid()) {
                continue;
            }
            fun(a.id, a.nodes, a.data);
        }
    }

    void ForEach(std::function<void(T&)> fun) {
        for (auto& a : attachments_) {
            if (!a.valid()) {
                continue;
            }
            fun(a.data);
        }
    }

    const std::vector<Attachment<T>>& GetAll() { return attachments_; }

    result<T*> Get(AttachmentIndex<T> id) {
        if (!Validate(id)) {
            return Error::InvalidAttachment;
        }
        return &attachments_[id.id].data;
    }

    result<std::pair<AttachmentIndex<T>, T*>> Find(const std::string& name) {
        auto result = attachment_map_.find(name);
        if (result == attachment_map_.end()) {
            return Error::NotFound;
        }

        OUTCOME_TRY(data, Get(result->second));
        return std::make_pair(result->second, data);
    }

    result<void> Attach(AttachmentIndex<T> id, NodeIndex node) {
        if (!Validate(id)) {
            return Error::InvalidAttachment;
        }
        attachments_[id.id].nodes.insert(node);
        return outcome::success();
    }

    result<void> Detach(AttachmentIndex<T> id, NodeIndex node) {
        if (!Validate(id)) {
            return Error::InvalidAttachment;
        }
        attachments_[id.id].nodes.erase(node);
        return outcome::success();
    }

    result<void> DetachAll(AttachmentIndex<T> id) {
        if (!Validate(id)) {
            return Error::InvalidAttachment;
        }
        attachments_[id.id].nodes.clear();
        return outcome::success();
    }

    result<std::set<NodeIndex>*> GetNodes(AttachmentIndex<T> id) {
        if (!Validate(id)) {
            return Error::InvalidAttachment;
        }
        return &attachments_[id.id].nodes;
    }

    size_t count() { return valid_count_; }

  private:
    std::vector<Attachment<T>> attachments_;
    std::map<std::string, AttachmentIndex<T>> attachment_map_;
    std::queue<size_t> recycled_attachments_;

    size_t valid_count_;

    bool Validate(AttachmentIndex<T> id) {
        return id.gen != 0 && id.id < attachments_.size() &&
               attachments_[id.id].id.gen == id.gen;
    }
};

}  // namespace goma
