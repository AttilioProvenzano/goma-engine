#pragma once

#include "scene/scene.hpp"

#include <future>

namespace goma {

class SceneLoader {
  public:
    virtual result<std::future<std::unique_ptr<Scene>>> ReadSceneFromFile(
        const char* file_path) = 0;
};

}  // namespace goma
