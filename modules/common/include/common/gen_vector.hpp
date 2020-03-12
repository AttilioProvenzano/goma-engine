#pragma once

#include "common/include.hpp"
#include "scene/gen_index.hpp"

namespace goma {

struct gen_id {
    using id_type = size_t;
    using gen_type = int;

    id_type id;
    gen_type gen;

    bool operator==(const gen_id& rhs) const {
        auto& lhs = *this;
        return lhs.id == rhs.id && lhs.gen == rhs.gen;
    }
};

template <typename T, typename I>
class gen_vector_iter;

template <typename T, typename I = gen_id>
class gen_vector {
  public:
    using iterator = gen_vector_iter<T, I>;
    using difference_type = ptrdiff_t;
    using size_type = size_t;
    using index_type = I;
    using value_type = T;
    using pointer = T*;
    using reference = T&;

    // Pushes an element to the end of the vector, without the option to recycle
    // an index
    void push_back(value_type&& item) {
        items_.push_back(std::forward<T>(item));
        gens_.push_back(0);
        last_valid_id_ = items_.size() - 1;
    }

    index_type insert(value_type&& item) {
        if (recycled_ids_.empty()) {
            push_back(std::forward<value_type>(item));
            return {items_.size() - 1, 0};
        } else {
            auto id = recycled_ids_.back();
            recycled_ids_.pop_back();
            auto& gen = gens_[id];

            if (id < first_valid_id_) {
                first_valid_id_ = id;
            }
            if (id > last_valid_id_) {
                last_valid_id_ = id;
            }

            items_[id] = std::forward<value_type>(item);
            gen = ~gen + 1;
            return {id, gen};
        }
    }

    void erase(iterator position) {
        auto id = position.offset_;
        do_erase_(id);

        if (valid_size() == 0) {
            // No valid elements left
            first_valid_id_ = items_.size();
            last_valid_id_ = items_.size();
        } else if (id == first_valid_id_) {
            auto f = std::find_if(gens_.begin() + id + 1,
                                  gens_.begin() + last_valid_id_,
                                  [](const auto& gen) { return gen >= 0; });
            first_valid_id_ = std::distance(gens_.begin(), f);
        } else if (id == last_valid_id_) {
            auto l = std::find_if(
                gens_.rbegin() + (gens_.size() - 1 - (id - 1)),
                gens_.rbegin() + (gens_.size() - 1 - first_valid_id_),
                [](const auto& gen) { return gen >= 0; });
            last_valid_id_ = std::distance(l, gens_.rend() - 1);
        }
    }

    size_type erase(const index_type& id) {
        if (is_valid(id)) {
            erase(iterator(this, id.id));
            return 1;
        }
        return 0;
    }

    void erase(iterator first, iterator last) {
        auto current = first;
        while (current < last) {
            do_erase_(current.offset_);
            ++current;
        }

        if (valid_size() == 0) {
            // No valid elements left
            first_valid_id_ = items_.size();
            last_valid_id_ = items_.size();
        } else {
            if (first_valid_id_ >= first.offset_ &&
                first_valid_id_ < last.offset_) {
                auto f = std::find_if(gens_.begin() + last.offset_, gens_.end(),
                                      [](const auto& gen) { return gen >= 0; });
                first_valid_id_ = std::distance(gens_.begin(), f);
            }

            if (last_valid_id_ >= first.offset_ &&
                last_valid_id_ < last.offset_) {
                auto l = std::find_if(
                    gens_.rbegin() + (gens_.size() - 1 - first.offset_),
                    gens_.rend(), [](const auto& gen) { return gen >= 0; });
                last_valid_id_ = std::distance(l, gens_.rend() - 1);
            }
        }
    }

    void clear() {
        items_.clear();
        gens_.clear();
        recycled_ids_.clear();

        size_type first_valid_id_ = 0;
        size_type last_valid_id_ = std::numeric_limits<size_type>::max();
    }

    bool is_valid(const index_type& id) const {
        return id.id < items_.size() && id.gen >= 0 && id.gen == gens_[id.id];
    }

    bool is_valid(const size_t& id) const {
        return id < items_.size() && gens_[id] >= 0;
    }

    reference operator[](const index_type& id) { return items_[id.id]; }

    reference at(const index_type& id) {
        if (!is_valid(id)) {
            throw std::out_of_range("Invalid index");
        }
        return operator[](id);
    }

    // Resizes the vector
    void resize(size_type n, value_type val = value_type()) {
        auto prev_size = items_.size();
        items_.resize(n, val);
        gens_.resize(n, 0);

        auto new_size = items_.size();
        if (new_size < prev_size) {
            // Remove recycled ids that are no longer relevant
            recycled_ids_.erase(
                std::remove_if(
                    recycled_ids_.begin(), recycled_ids_.end(),
                    [s = new_size](const auto& id) { return id >= s; }),
                recycled_ids_.end());

            // Update first and last valid indices
            if (first_valid_id_ >= new_size) {
                // No valid indices are left
                first_valid_id_ = new_size;
                last_valid_id_ = new_size;
            } else if (last_valid_id_ >= new_size) {
                // We iterate backwards and stop at the first valid id, or at
                // first_valid_id, which we know is still valid
                last_valid_id_ = new_size - 1;
                while (last_valid_id_ > first_valid_id_ &&
                       gens_[last_valid_id_] < 0) {
                    --last_valid_id_;
                }
            }
        } else if (new_size > prev_size) {
            last_valid_id_ = new_size - 1;
        }
    }

    // Reserves space in the underlying vector(s)
    void reserve(size_type size) {
        items_.reserve(size);
        gens_.reserve(size);
    }

    // Returns the number of valid elements
    size_type valid_size() const {
        return items_.size() - recycled_ids_.size();
    }

    bool empty() const { return size() == 0; }

    // Returns the size of the underlying vector
    size_type size() const { return items_.size(); }

    // Return a pointer to the raw data
    pointer data() { return items_.data(); }

    // Returns a reference to the first valid element
    reference front() { return *begin(); }

    // Returns a reference to the last valid element
    reference back() { return *iterator(this, last_valid_id_); }

    iterator begin() { return iterator(this, first_valid_id_); }
    iterator end() { return iterator(this, last_valid_id_ + 1); }

  private:
    std::vector<T> items_;
    std::vector<typename I::gen_type> gens_;
    std::vector<size_t> recycled_ids_;

    size_type first_valid_id_ = 0;
    size_type last_valid_id_ = std::numeric_limits<size_type>::max();

    void do_erase_(size_type id) {
        auto& gen = gens_[id];
        if (gen >= 0) {
            gen = ~gen;
            recycled_ids_.push_back(id);
        }
    }

    friend class gen_vector_iter<T, I>;
};

template <typename T, typename I = gen_id>
class gen_vector_iter {
  public:
    using iterator = gen_vector_iter<T, I>;
    using container_type = gen_vector<T, I>;
    using difference_type = ptrdiff_t;
    using index_type = I;
    using value_type = T;
    using pointer = T*;
    using reference = T&;
    using iterator_category = std::bidirectional_iterator_tag;

    gen_vector_iter(container_type* vec, size_t offset = 0)
        : vec_(vec), offset_(offset){};

    reference operator*() { return vec_->items_[offset_]; }
    pointer operator->() { return &vec_->items_[offset_]; }

    iterator& operator++() {
        do {
            ++offset_;
        } while (!vec_->is_valid(offset_) && offset_ <= vec_->last_valid_id_);
        return *this;
    }
    iterator operator++(int) {
        iterator clone(*this);
        this->operator++();
        return clone;
    }

    iterator& operator--() {
        do {
            --offset_;
        } while (!vec_->is_valid(offset_) && offset_ >= vec_->first_valid_id_);
        return *this;
    }
    iterator operator--(int) {
        iterator clone(*this);
        this->operator--();
        return clone;
    }

    bool operator<(const iterator& rhs) const {
        const auto& lhs = *this;
        return lhs.offset_ < rhs.offset_;
    }
    bool operator==(const iterator& rhs) const {
        const auto& lhs = *this;
        return lhs.vec_ == rhs.vec_ && lhs.offset_ == rhs.offset_;
    }
    bool operator!=(const iterator& rhs) const {
        const auto& lhs = *this;
        return !(lhs == rhs);
    }

  private:
    container_type* vec_;
    size_t offset_;

    friend class gen_vector<T, I>;
};

}  // namespace goma
