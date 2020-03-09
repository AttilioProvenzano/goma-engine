#include "common/hash.hpp"

namespace goma {

size_t djb2_hash(const char* str) {
    size_t hash = 5381;

    int c;
    while (c = *str++) {
        hash = ((hash << 5) + hash) ^ c; /* hash * 33 ^ c */
    }

    return hash;
}

}  // namespace goma
