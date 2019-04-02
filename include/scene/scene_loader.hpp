#pragma once

#include "scene/scene.hpp"

#include "common/include.hpp"

namespace goma {

class SceneLoader {
  public:
    virtual result<std::unique_ptr<Scene>> ReadSceneFromFile(
        const char* file_path) = 0;
};

}  // namespace goma
