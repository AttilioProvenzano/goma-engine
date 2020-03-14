#pragma once

#include "scene/scene_loader.hpp"
#include "scene/attachments/material.hpp"

#include "assimp/scene.h"

#include "common/include.hpp"

namespace goma {

class AssimpLoader : public SceneLoader {
  public:
    virtual result<std::unique_ptr<Scene>> ReadSceneFromFile(
        const char* file_path) override;

  private:
    const int kNumThreads = 8;
    ctpl::thread_pool thread_pool_{kNumThreads};

    result<std::unique_ptr<Scene>> ConvertScene(const aiScene* ai_scene,
                                                const std::string& base_path);
};

}  // namespace goma
