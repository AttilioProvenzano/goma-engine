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
    result<std::unique_ptr<Scene>> ConvertScene(const aiScene* ai_scene,
                                                const std::string& base_path);

    result<TextureBinding> LoadMaterialTexture(
        Scene* scene, const aiMaterial* material, const std::string& base_path,
        const std::pair<aiTextureType, TextureType>& texture_type,
        uint32_t texture_index);
};

}  // namespace goma
