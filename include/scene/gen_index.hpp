#pragma once

#include "common/include.hpp"

namespace goma {

struct GenIndex {
    size_t id;
    size_t gen;  // 0 reserved for invalid elements

    GenIndex() : id(0), gen(0) {}
    GenIndex(size_t id_, size_t gen_ = 1) : id(id_), gen(gen_) {}

    bool valid() const { return gen > 0; }

    bool operator==(const GenIndex& other) const {
        return (memcmp(this, &other, sizeof(*this)) == 0);
    }

    bool operator!=(const GenIndex& other) const {
        return (memcmp(this, &other, sizeof(*this)) != 0);
    }

    bool operator<(const GenIndex& other) const {
        return (memcmp(this, &other, sizeof(*this)) < 0);
    }

    friend std::ostream& operator<<(std::ostream& o, const goma::GenIndex& id);
};

typedef GenIndex NodeIndex;

template <typename T>
using AttachmentIndex = GenIndex;

}  // namespace goma
