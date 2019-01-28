#pragma once

#include "scene/scene_loader.hpp"

namespace goma {

class AssimpLoader : public SceneLoader {
  public:
    virtual result<std::unique_ptr<Scene>> ReadSceneFromFile(
        const char* file_path) override;
};

}  // namespace goma
