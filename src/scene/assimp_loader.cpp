#include "scene/loaders/assimp_loader.hpp"

#include "scene/attachments/texture.hpp"
#include "scene/attachments/material.hpp"
#include "scene/attachments/camera.hpp"
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
    const aiScene *ai_scene, const std::string &base_path) {
    std::unique_ptr<Scene> scene = std::make_unique<Scene>();

    std::map<std::string, NodeIndex> node_name_map;

    // Convert node structure
    if (ai_scene->mRootNode) {
        std::map<aiNode *, NodeIndex> node_map;

        aiNode *ai_root_node = ai_scene->mRootNode;
        aiVector3D pos, rot, scale;

        ai_root_node->mTransformation.Decompose(scale, rot, pos);
        auto root_transform = scene->GetTransform(scene->GetRootNode()).value();
        root_transform->position = {pos.x, pos.y, pos.z};
        root_transform->rotation = glm::quat(glm::vec3(rot.x, rot.y, rot.z));
        root_transform->scale = {scale.x, scale.y, scale.z};

        node_map[ai_root_node] = scene->GetRootNode();
        node_name_map[ai_root_node->mName.C_Str()] = scene->GetRootNode();

        // Traverse the node graph
        std::queue<aiNode *> open_nodes;
        std::set<aiNode *> closed_nodes;

        for (unsigned int i = 0; i < ai_root_node->mNumChildren; i++) {
            open_nodes.push(ai_root_node->mChildren[i]);
        }

        while (open_nodes.size() > 0) {
            aiNode *ai_node = open_nodes.front();
            open_nodes.pop();
            closed_nodes.insert(ai_node);

            NodeIndex parent = scene->GetRootNode();
            auto result = node_map.find(ai_node->mParent);
            if (result != node_map.end()) {
                parent = result->second;
            }

            ai_node->mTransformation.Decompose(scale, rot, pos);
            auto node_result = scene->CreateNode(
                parent, {{pos.x, pos.y, pos.z},
                         glm::quat(glm::vec3(rot.x, rot.y, rot.z)),
                         {scale.x, scale.y, scale.z}});

            if (node_result.has_value()) {
                auto node = node_result.value();
                node_map[ai_node] = node;
                node_name_map[ai_node->mName.C_Str()] = node;
            } else {
                const auto &error = node_result.error();
                LOGE("Node conversion failed for node \"%s\". Error: %s",
                     ai_node->mName.C_Str(), error.message().c_str());
            }

            // Add children to open_nodes
            for (unsigned int i = 0; i < ai_node->mNumChildren; i++) {
                auto child = ai_node->mChildren[i];
                // Guard against loops in the graph
                if (closed_nodes.find(child) == closed_nodes.end()) {
                    open_nodes.push(child);
                }
            }
        }
    }

    // Convert embedded textures (if any)
    if (ai_scene->HasTextures()) {
        for (size_t i = 0; i < ai_scene->mNumTextures; i++) {
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
                    auto texture = scene->CreateAttachment<Texture>(
                        {path, ai_texture->mWidth, 1, std::move(data), true});

                    if (texture.has_value()) {
                        auto t = texture.value();
                        scene->RegisterAttachment<Texture>(t, path);
                    } else {
                        const auto &error = texture.error();
                        LOGW("Loading texture \"%s\" failed with error: %s",
                             path.c_str(), error.message().c_str());
                    }
                } else {
                    std::vector<uint8_t> data;
                    data.resize(4 * width * height);
                    memcpy(data.data(), image_data, data.size());

                    std::string path = ai_texture->mFilename.C_Str();
                    auto texture = scene->CreateAttachment<Texture>(
                        {path, static_cast<uint32_t>(width),
                         static_cast<uint32_t>(height), std::move(data),
                         false});

                    if (texture.has_value()) {
                        auto t = texture.value();
                        scene->RegisterAttachment<Texture>(t, path);
                    } else {
                        const auto &error = texture.error();
                        LOGW("Loading texture \"%s\" failed with error: %s",
                             path.c_str(), error.message().c_str());
                    }
                }

                stbi_image_free(image_data);
            } else {
                // Uncompressed texture
                std::vector<uint8_t> data;
                data.resize(4 * ai_texture->mWidth * ai_texture->mHeight);
                memcpy(data.data(), ai_texture->pcData, data.size());

                std::string path = ai_texture->mFilename.C_Str();
                auto texture = scene->CreateAttachment<Texture>(
                    {ai_texture->mFilename.C_Str(), ai_texture->mWidth,
                     ai_texture->mHeight, std::move(data), false});

                if (texture.has_value()) {
                    auto t = texture.value();
                    scene->RegisterAttachment<Texture>(t, path);
                } else {
                    const auto &error = texture.error();
                    LOGW("Loading texture \"%s\" failed with error: %s",
                         path.c_str(), error.message().c_str());
                }
            }
        }
    }

    // Convert materials
    if (ai_scene->HasMaterials()) {
        for (size_t i = 0; i < ai_scene->mNumMaterials; i++) {
            aiMaterial *ai_material = ai_scene->mMaterials[i];
            Material material{ai_material->GetName().C_Str()};

            static const std::vector<std::pair<aiTextureType, TextureType>>
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

            for (const auto &texture_type : texture_types) {
                for (uint32_t j = 0;
                     j < ai_material->GetTextureCount(texture_type.first);
                     j++) {
                    auto tex_binding = LoadMaterialTexture(
                        scene.get(), ai_material, base_path, texture_type, j);

                    if (tex_binding.has_value()) {
                        material.textures[texture_type.second].push_back(
                            std::move(tex_binding.value()));
                    }
                }

                aiColor3D diffuse(0, 0, 0);
                aiColor3D specular(0, 0, 0);
                aiColor3D ambient(0, 0, 0);
                aiColor3D emissive(0, 0, 0);
                aiColor3D transparent(0, 0, 0);
                ai_material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
                ai_material->Get(AI_MATKEY_COLOR_SPECULAR, specular);
                ai_material->Get(AI_MATKEY_COLOR_AMBIENT, ambient);
                ai_material->Get(AI_MATKEY_COLOR_EMISSIVE, emissive);
                ai_material->Get(AI_MATKEY_COLOR_TRANSPARENT, transparent);
                material.diffuse_color = {diffuse.r, diffuse.g, diffuse.b};
                material.specular_color = {specular.r, specular.g, specular.b};
                material.ambient_color = {ambient.r, ambient.g, ambient.b};
                material.emissive_color = {emissive.r, emissive.g, emissive.b};
                material.transparent_color = {transparent.r, transparent.g,
                                              transparent.b};

                int two_sided = 0;
                float opacity = 1.0f;
                float shininess_exponent = 0.0f;
                float specular_strength = 1.0f;
                ai_material->Get(AI_MATKEY_TWOSIDED, two_sided);
                ai_material->Get(AI_MATKEY_OPACITY, opacity);
                ai_material->Get(AI_MATKEY_SHININESS, shininess_exponent);
                ai_material->Get(AI_MATKEY_SHININESS_STRENGTH,
                                 specular_strength);
                material.two_sided = (two_sided > 0);
                material.opacity = opacity;
                material.shininess_exponent = shininess_exponent;
                material.specular_strength = specular_strength;
            }

            auto material_result = scene->CreateAttachment(std::move(material));

            if (material_result.has_value()) {
                auto m = material_result.value();
                scene->RegisterAttachment<Material>(
                    m, ai_material->GetName().C_Str());
            } else {
                LOGW("Material creation failed for material \"%s\".",
                     ai_material->GetName().C_Str());
            }
        }
    }

    // Convert cameras
    if (ai_scene->HasCameras()) {
        for (size_t i = 0; i < ai_scene->mNumCameras; i++) {
            aiCamera *ai_camera = ai_scene->mCameras[i];

            NodeIndex node = scene->GetRootNode();
            auto result = node_name_map.find(ai_camera->mName.C_Str());
            if (result != node_name_map.end()) {
                node = result->second;
            }

            auto camera_result = scene->CreateAttachment<Camera>(
                node, {ai_camera->mName.C_Str(),
                       glm::degrees(ai_camera->mHorizontalFOV),
                       ai_camera->mClipPlaneNear,
                       ai_camera->mClipPlaneFar,
                       ai_camera->mAspect,
                       {ai_camera->mPosition.x, ai_camera->mPosition.y,
                        ai_camera->mPosition.z},
                       {ai_camera->mUp.x, ai_camera->mUp.y, ai_camera->mUp.z},
                       {ai_camera->mLookAt.x, ai_camera->mLookAt.y,
                        ai_camera->mLookAt.z}});

            if (camera_result.has_value()) {
                auto c = camera_result.value();
                scene->RegisterAttachment<Camera>(c, ai_camera->mName.C_Str());
            } else {
                LOGW("Camera creation failed for material \"%s\".",
                     ai_camera->mName.C_Str());
            }
        }
    }

    // Convert lights
    if (ai_scene->HasLights()) {
        for (size_t i = 0; i < ai_scene->mNumLights; i++) {
            aiLight *ai_light = ai_scene->mLights[i];
        }
    }

    return std::move(scene);
}

result<TextureBinding> AssimpLoader::LoadMaterialTexture(
    Scene *scene, const aiMaterial *ai_material, const std::string &base_path,
    const std::pair<aiTextureType, TextureType> &texture_type,
    uint32_t texture_index) {
    assert(scene && "Scene must be valid");
    assert(ai_material && "Assimp material must be valid");
    // Get texture information from the material
    aiString path;
    aiTextureMapping mapping = aiTextureMapping_UV;
    unsigned int uvindex = 0;
    float blend = 1.0f;
    aiTextureOp op = aiTextureOp_Add;
    aiTextureMapMode mapmode[3] = {aiTextureMapMode_Wrap, aiTextureMapMode_Wrap,
                                   aiTextureMapMode_Wrap};

    ai_material->GetTexture(texture_type.first, texture_index, &path, &mapping,
                            &uvindex, &blend, &op, mapmode);

    if (path.length == 0) {
        LOGW("No path specified for texture, skipping.");
        return Error::NotFound;
    }

    // Gather the texture (if already loaded) or create one
    AttachmentIndex<Texture> texture;
    auto result = scene->FindAttachment<Texture>(path.C_Str());
    if (result.has_value()) {
        texture = result.value().first;
    } else {
        // Create a texture using stb_image
        int width, height, n;
        auto full_path = base_path + path.C_Str();
        auto image_data = stbi_load(full_path.c_str(), &width, &height, &n, 4);

        if (!image_data) {
            LOGW(
                "Decompressing \"%s\" failed with error: "
                "%s",
                full_path.c_str(), stbi_failure_reason());
            return Error::DecompressionFailed;
        }

        std::vector<uint8_t> data;
        data.resize(4 * width * height);
        memcpy(data.data(), image_data, data.size());

        auto texture_result = scene->CreateAttachment<Texture>(
            {path.C_Str(), static_cast<uint32_t>(width),
             static_cast<uint32_t>(height), std::move(data), false});

        if (texture_result.has_value()) {
            texture = texture_result.value();
            scene->RegisterAttachment<Texture>(texture, path.C_Str());
            stbi_image_free(image_data);
        } else {
            const auto &error = texture_result.error();
            LOGW("Loading texture \"%s\" failed with error: %s", path.C_Str(),
                 error.message().c_str());

            stbi_image_free(image_data);
            return Error::LoadingFailed;
        }
    }

    static const std::map<aiTextureMapMode, TextureWrappingMode>
        texture_wrap_modes{
            {aiTextureMapMode_Wrap, TextureWrappingMode::Repeat},
            {aiTextureMapMode_Clamp, TextureWrappingMode::ClampToEdge},
            {aiTextureMapMode_Mirror, TextureWrappingMode::MirroredRepeat},
            {aiTextureMapMode_Decal, TextureWrappingMode::Decal}};

    TextureBinding tex_binding = {texture, uvindex, blend};
    if (mapmode) {
        for (size_t k = 0; k < 3; k++) {
            auto m = texture_wrap_modes.find(mapmode[k]);
            if (m != texture_wrap_modes.end()) {
                tex_binding.wrapping[k] = m->second;
            }
        }
    }

    return tex_binding;
}

}  // namespace goma
