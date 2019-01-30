#include "scene/loaders/assimp_loader.hpp"

#include "scene/attachments/texture.hpp"
#include "scene/attachments/material.hpp"
#include "common/error_codes.hpp"

#include "assimp/Importer.hpp"

#define LOG(prefix, format, ...) printf(prefix format "\n", __VA_ARGS__)
#define LOGE(format, ...) LOG("** ERROR: ", format, __VA_ARGS__)
#define LOGW(format, ...) LOG("* Warning: ", format, __VA_ARGS__)
#define LOGI(format, ...) LOG("", format, __VA_ARGS__)

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include "stb_image.h"

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

            if (!ai_texture->mHeight) {
                int width, height, n;
                auto image_data = stbi_load_from_memory(
                    reinterpret_cast<const stbi_uc*>(ai_texture->pcData),
                    ai_texture->mWidth, &width, &height, &n, 4);

                if (!image_data) {
                    LOGW("Decompressing \"%s\" failed with error: %s",
                         ai_texture->mFilename.C_Str(), stbi_failure_reason());

                    std::vector<uint8_t> data;
                    data.resize(4 * ai_texture->mWidth * ai_texture->mHeight);
                    memcpy(data.data(), ai_texture->pcData, data.size());

                    scene->GetAttachmentManager<Texture>()->CreateAttachment(
                        {0, 0},
                        {ai_texture->mFilename.C_Str(), ai_texture->mWidth, 1,
                         std::move(data), true, ai_texture->achFormatHint});
                } else {
                    std::vector<uint8_t> data;
                    data.resize(4 * width * height);
                    memcpy(data.data(), image_data, data.size());

                    scene->GetAttachmentManager<Texture>()->CreateAttachment(
                        {0, 0}, {ai_texture->mFilename.C_Str(),
                                 static_cast<uint32_t>(width),
                                 static_cast<uint32_t>(height), std::move(data),
                                 false, ai_texture->achFormatHint});
                }

                stbi_image_free(image_data);
            } else {
                // Uncompressed texture
                std::vector<uint8_t> data;
                data.resize(4 * ai_texture->mWidth * ai_texture->mHeight);
                memcpy(data.data(), ai_texture->pcData, data.size());

                scene->GetAttachmentManager<Texture>()->CreateAttachment(
                    {0, 0}, {ai_texture->mFilename.C_Str(), ai_texture->mWidth,
                             ai_texture->mHeight, std::move(data), false, ""});
            }
        }
    }

    if (ai_scene->HasMaterials()) {
        for (unsigned int i = 0; i < ai_scene->mNumMaterials; i++) {
            aiMaterial* ai_material = ai_scene->mMaterials[i];
        }
    }

    return std::move(scene);
}

}  // namespace goma
