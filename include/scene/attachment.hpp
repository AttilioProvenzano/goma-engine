#pragma once

#include "scene/gen_index.hpp"

#include <outcome.hpp>
namespace outcome = OUTCOME_V2_NAMESPACE;
using outcome::result;

#include <vector>
#include <queue>
#include <map>
#include <string>

namespace goma {

template <typename T>
struct Attachment {
    AttachmentIndex<T> id;
    NodeIndex node_id;
    T data = {};

    Attachment(AttachmentIndex<T> id_, NodeIndex node_id_, T&& data_ = T())
        : id(id_), node_id(node_id_), data(std::forward<T>(data_)) {}

    bool operator==(const Attachment<T>& other) const {
        return (this->id == other.id);
    }
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

    result<AttachmentIndex<T>> Create(const NodeIndex& node_id,
                                      T&& data = T()) {
        AttachmentIndex<T> ret_id;
        if (!recycled_attachments_.empty()) {
            size_t index = recycled_attachments_.front();
            recycled_attachments_.pop();

            // The last valid generation was stored in id.id
            // when the attachment was deleted
            size_t new_gen = attachments_[index].id.id + 1;

            attachments_[index] = {
                {index, new_gen}, node_id, std::forward<T>(data)};
            ret_id = {index, new_gen};
        } else {
            size_t id = attachments_.size();
            attachments_.emplace_back(AttachmentIndex<T>{id}, node_id,
                                      std::forward<T>(data));
            ret_id = {id};
        }
        valid_count_++;
        return ret_id;
    }

    result<AttachmentIndex<T>> Create(T&& data = T()) {
        return Create({0, 0}, std::forward<T>(data));
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

        OUTCOME_TRY(data, Get(result.second));
        return {result.second, data};
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
