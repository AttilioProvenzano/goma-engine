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

#include <map>

namespace goma {

result<std::unique_ptr<Scene>> AssimpLoader::ReadSceneFromFile(
    const char *file_path) {
    Assimp::Importer importer;
    auto ai_scene = importer.ReadFile(file_path, 0);

    if (!ai_scene) {
        LOGE("%s", importer.GetErrorString());
        return Error::SceneImportFailed;
    }

    std::string base_path = file_path;
    base_path = base_path.substr(0, base_path.find_last_of('/') + 1);
    return std::move(ConvertScene(ai_scene, std::move(base_path)));
}

result<std::unique_ptr<Scene>> AssimpLoader::ConvertScene(
    const aiScene *ai_scene, std::string base_path) {
    std::unique_ptr<Scene> scene = std::make_unique<Scene>();

    // Convert embedded textures (if any)
    if (ai_scene->HasTextures()) {
        for (unsigned int i = 0; i < ai_scene->mNumTextures; i++) {
            aiTexture *ai_texture = ai_scene->mTextures[i];

            if (ai_texture->mFilename.length == 0) {
                LOGW("No filename specified for texture, skipping.");
                continue;
            }

            if (!ai_texture->mHeight) {
                int width, height, n;
                auto image_data = stbi_load_from_memory(
                    reinterpret_cast<const stbi_uc *>(ai_texture->pcData),
                    ai_texture->mWidth, &width, &height, &n, 4);

                if (!image_data) {
                    LOGW("Decompressing \"%s\" failed with error: %s",
                         ai_texture->mFilename.C_Str(), stbi_failure_reason());

                    // Decompression failed, store the compressed texture
                    std::vector<uint8_t> data;
                    data.resize(4 * ai_texture->mWidth * ai_texture->mHeight);
                    memcpy(data.data(), ai_texture->pcData, data.size());

                    std::string path = ai_texture->mFilename.C_Str();
                    auto texture =
                        scene->GetAttachmentManager<Texture>()
                            ->CreateAttachment({path, ai_texture->mWidth, 1,
                                                std::move(data), true});

                    if (texture.has_value()) {
                        scene->texture_map()[path] = texture.value();
                    } else {
                        LOGW(
                            "Loading texture \"%s\" failed with error: %s",
                            path.c_str());  //, TODO compiler errors with
                                            // texture (also
                                            // texture.has_value())
                                            // texture.error().message().c_str());
                    }
                } else {
                    std::vector<uint8_t> data;
                    data.resize(4 * width * height);
                    memcpy(data.data(), image_data, data.size());

                    std::string path = ai_texture->mFilename.C_Str();
                    auto texture = scene->GetAttachmentManager<Texture>()
                                       ->CreateAttachment(
                                           {path, static_cast<uint32_t>(width),
                                            static_cast<uint32_t>(height),
                                            std::move(data), false});

                    if (texture.has_value()) {
                        scene->texture_map()[path] = texture.value();
                    } else {
                        LOGW(
                            "Loading texture \"%s\" failed with error: %s",
                            path.c_str());  //,
                                            // texture.error().message().c_str());
                    }
                }

                stbi_image_free(image_data);
            } else {
                // Uncompressed texture
                std::vector<uint8_t> data;
                data.resize(4 * ai_texture->mWidth * ai_texture->mHeight);
                memcpy(data.data(), ai_texture->pcData, data.size());

                std::string path = ai_texture->mFilename.C_Str();
                auto texture =
                    scene->GetAttachmentManager<Texture>()->CreateAttachment(
                        {0, 0},
                        {ai_texture->mFilename.C_Str(), ai_texture->mWidth,
                         ai_texture->mHeight, std::move(data), false});

                if (texture.has_value()) {
                    scene->texture_map()[path] = texture.value();
                } else {
                    LOGW("Loading texture \"%s\" failed with error: %s",
                         path.c_str());  //, texture.error().message().c_str());
                }
            }
        }
    }

    if (ai_scene->HasMaterials()) {
        for (size_t i = 0; i < ai_scene->mNumMaterials; i++) {
            aiMaterial *ai_material = ai_scene->mMaterials[i];
            Material material{ai_material->GetName().C_Str()};

            const std::vector<std::pair<aiTextureType, TextureType>>
                texture_types{
                    {aiTextureType_DIFFUSE, TextureType::Diffuse},
                    {aiTextureType_SPECULAR, TextureType::Specular},
                    {aiTextureType_AMBIENT, TextureType::Ambient},
                    {aiTextureType_EMISSIVE, TextureType::Emissive},
                    {aiTextureType_HEIGHT, TextureType::HeightMap},
                    {aiTextureType_NORMALS, TextureType::NormalMap},
                    {aiTextureType_SHININESS, TextureType::Shininess},
                    {aiTextureType_OPACITY, TextureType::Opacity},
                    {aiTextureType_DISPLACEMENT, TextureType::Displacement},
                    {aiTextureType_LIGHTMAP, TextureType::LightMap},
                    {aiTextureType_REFLECTION, TextureType::Reflection},
                    {aiTextureType_UNKNOWN, TextureType::MetallicRoughness}};

            const std::map<aiTextureMapMode, TextureWrappingMode>
                texture_wrap_modes{
                    {aiTextureMapMode_Wrap, TextureWrappingMode::Repeat},
                    {aiTextureMapMode_Clamp, TextureWrappingMode::ClampToEdge},
                    {aiTextureMapMode_Mirror,
                     TextureWrappingMode::MirroredRepeat},
                    {aiTextureMapMode_Decal, TextureWrappingMode::Decal}};

            for (const auto &texture_type : texture_types) {
                for (size_t j = 0;
                     j < ai_material->GetTextureCount(texture_type.first);
                     j++) {
                    aiString path;
                    aiTextureMapping mapping;
                    unsigned int uvindex;
                    float blend;
                    aiTextureOp op;
                    aiTextureMapMode mapmode[3];
                    ai_material->GetTexture(texture_type.first, j, &path,
                                            &mapping, &uvindex, &blend, &op,
                                            mapmode);

                    if (path.length == 0) {
                        LOGW("No path specified for texture, skipping.");
                        continue;
                    }

                    AttachmentIndex<Texture> texture_index;
                    auto result = scene->texture_map().find(path.C_Str());
                    if (result != scene->texture_map().end()) {
                        texture_index = result->second;
                    } else {
                        int width, height, n;
                        auto full_path = base_path + path.C_Str();
                        auto image_data = stbi_load(full_path.c_str(), &width,
                                                    &height, &n, 4);

                        if (!image_data) {
                            LOGW(
                                "Decompressing \"%s\" failed with error: "
                                "%s",
                                full_path.c_str(), stbi_failure_reason());
                            continue;
                        }

                        std::vector<uint8_t> data;
                        data.resize(4 * width * height);
                        memcpy(data.data(), image_data, data.size());

                        auto texture =
                            scene->GetAttachmentManager<Texture>()
                                ->CreateAttachment(
                                    {path.C_Str(), static_cast<uint32_t>(width),
                                     static_cast<uint32_t>(height),
                                     std::move(data), false});

                        if (texture.has_value()) {
                            scene->texture_map()[path.C_Str()] =
                                texture.value();
                            texture_index = texture.value();
                            stbi_image_free(image_data);
                        } else {
                            LOGW(
                                "Loading texture \"%s\" failed with error: "
                                "%s",
                                path.C_Str());
                            // texture.error().message().c_str());

                            stbi_image_free(image_data);
                            continue;
                        }
                    }

                    TextureBinding tex_binding = {texture_index, uvindex,
                                                  blend};
                    if (mapmode) {
                        for (size_t k = 0; k < 3; k++) {
                            auto m = texture_wrap_modes.find(mapmode[k]);
                            if (m != texture_wrap_modes.end()) {
                                tex_binding.wrapping[k] = m->second;
                            }
                        }
                    }

                    material.textures[texture_type.second].push_back(
                        std::move(tex_binding));
                }
            }
        }
    }

    return std::move(scene);
}

}  // namespace goma
