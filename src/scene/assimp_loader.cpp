#include "scene/assimp_loader.hpp"

#include "assimp/Importer.hpp"

#define LOG(prefix, format, ...) printf(prefix format "\n", __VA_ARGS__)
#define LOGE(format, ...) LOG("** ERROR: ", format, __VA_ARGS__)
#define LOGW(format, ...) LOG("* Warning: ", format, __VA_ARGS__)
#define LOGI(format, ...) LOG("", format, __VA_ARGS__)

namespace goma {

result<std::unique_ptr<Scene>> AssimpLoader::ReadSceneFromFile(
    const char* file_path) {
    Assimp::Importer importer;
    auto ai_scene = importer.ReadFile(file_path, 0);

    if (!ai_scene) {
        LOGE("%s", importer.GetErrorString());
        return Error::SceneImportFailed;
    }

    return std::move(ConvertScene(ai_scene));
}

result<std::unique_ptr<Scene>> AssimpLoader::ConvertScene(
    const aiScene* ai_scene) {
    std::unique_ptr<Scene> scene = std::make_unique<Scene>();

    if (ai_scene->HasTextures()) {
        for (unsigned int i = 0; i < ai_scene->mNumTextures; i++) {
            aiTexture* ai_texture = ai_scene->mTextures[i];
        }
    }

    return std::move(scene);
}

}  // namespace goma
