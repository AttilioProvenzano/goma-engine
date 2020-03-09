#pragma once

#include "common/include.hpp"

namespace goma {

size_t djb2_hash(const char*);

template <class T>
inline void hash_combine(size_t& seed, const T& v) {
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template <typename T>
struct vector_hash {
    size_t operator()(const std::vector<T>& vec) const {
        size_t seed = 0;
        for (auto& element : vec) {
            hash_combine(seed, std::hash<T>()(element));
        }
        return seed;
    }
};

}  // namespace goma
