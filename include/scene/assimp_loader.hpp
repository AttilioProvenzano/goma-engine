#pragma once

#include "scene/scene_loader.hpp"

#include "assimp/scene.h"

namespace goma {

class AssimpLoader : public SceneLoader {
  public:
    virtual result<std::unique_ptr<Scene>> ReadSceneFromFile(
        const char* file_path) override;

  private:
    result<std::unique_ptr<Scene>> ConvertScene(const aiScene* ai_scene);
};

}  // namespace goma
