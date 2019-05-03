#pragma once

#include <memory>
#include <unordered_map>

namespace goma {

template <typename T, typename KeyT = T::Key, typename KeyHashT = T::KeyHash>
class Cache {
  public:
    template <typename... Ts>
    std::shared_ptr<T> create(KeyT key, Ts&&... params) {
        auto p = std::make_shared<T>(std::forward<Ts>(params)...);
        map_[key] = p;
        return p;
    }

    std::shared_ptr<T> get(KeyT key) {
        auto res = map_.find(key);
        if (res != map_.end()) {
            return res->second.lock();
        }
        return {};
    }

    size_t erase(KeyT key) { return map_.erase(key); }

    void clear() { map_.clear(); }

  private:
    using MapT = std::unordered_map<KeyT, std::weak_ptr<T>, KeyHashT>;
    MapT map_{};
};

}  // namespace goma
