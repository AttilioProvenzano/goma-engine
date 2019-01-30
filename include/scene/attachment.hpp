#pragma once

#include "scene/gen_index.hpp"

#include <outcome.hpp>
namespace outcome = OUTCOME_V2_NAMESPACE;
using outcome::result;

#include <vector>
#include <queue>

namespace goma {

template <typename T>
using AttachmentIndex = GenIndex;

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

class AttachmentManagerBase {};

template <typename T>
class AttachmentManager : public AttachmentManagerBase {
  public:
    result<AttachmentIndex<T>> CreateAttachment(const NodeIndex node_id,
                                                T&& data = T()) {
        if (!node_id.valid()) {
            return Error::InvalidNode;
        }

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
        return ret_id;
    };

  private:
    std::vector<Attachment<T>> attachments_;
    std::queue<size_t> recycled_attachments_;
};

}  // namespace goma