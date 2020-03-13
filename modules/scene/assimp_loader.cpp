#include "scene/loaders/assimp_loader.hpp"

#include "scene/attachments/texture.hpp"
#include "scene/attachments/material.hpp"
#include "scene/attachments/camera.hpp"
#include "scene/attachments/light.hpp"
#include "scene/attachments/mesh.hpp"
#include "scene/utils.hpp"

#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/pbrmaterial.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include <stb/stb_image.h>

#include "common/error_codes.hpp"

namespace goma {

result<std::unique_ptr<Scene>> AssimpLoader::ReadSceneFromFile(
    const char *file_path) {
    Assimp::Importer importer;
    auto ai_scene =
        importer.ReadFile(file_path, aiProcessPreset_TargetRealtime_Quality |
                                         aiProcess_TransformUVCoords);

    if (!ai_scene) {
        spdlog::error(importer.GetErrorString());
        return Error::SceneImportFailed;
    }

    std::string base_path = file_path;
    base_path = base_path.substr(0, base_path.find_last_of('/') + 1);
    return std::move(ConvertScene(ai_scene, std::move(base_path)));
}

namespace {

void ConvertNode(const aiNode &ai_node, Node &out_node,
                 gen_vector<Mesh> &out_meshes) {
    aiVector3D pos, rot, scale;
    ai_node.mTransformation.Decompose(scale, rot, pos);

    Transform t;
    t.position = {pos.x, pos.y, pos.z};
    t.rotation = glm::quat(glm::vec3(rot.x, rot.y, rot.z));
    t.scale = {scale.x, scale.y, scale.z};

    out_node.set_transform(std::move(t));

    std::for_each(ai_node.mMeshes, ai_node.mMeshes + ai_node.mNumMeshes,
                  [&out_node, &out_meshes](unsigned int mesh_id) {
                      if (out_meshes.is_valid({mesh_id, 0})) {
                          out_meshes.at({mesh_id, 0}).attach_to(out_node);
                      }
                  });

    std::for_each(ai_node.mChildren, ai_node.mChildren + ai_node.mNumChildren,
                  [&out_node, &out_meshes](aiNode *ai_child) {
                      auto &child =
                          out_node.add_child({ai_child->mName.C_Str()});
                      ConvertNode(*ai_child, child, out_meshes);
                  });
}

}  // namespace

result<std::unique_ptr<Scene>> AssimpLoader::ConvertScene(
    const aiScene *ai_scene, const std::string &base_path) {
    std::unique_ptr<Scene> scene = std::make_unique<Scene>();

    // Useful to avoid errors if the scene is already filled
    auto base_material_offset = scene->materials().size();

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
                        material.texture_bindings[texture_type.second]
                            .push_back(std::move(tex_binding.value()));
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
                float alpha_cutoff = 1.0f;
                float shininess_exponent = 0.0f;
                float specular_strength = 1.0f;
                float metallic_factor = 0.2f;
                float roughness_factor = 0.5f;

                ai_material->Get(AI_MATKEY_TWOSIDED, two_sided);
                ai_material->Get(AI_MATKEY_OPACITY, opacity);
                ai_material->Get(AI_MATKEY_GLTF_ALPHACUTOFF, alpha_cutoff);
                ai_material->Get(AI_MATKEY_SHININESS, shininess_exponent);
                ai_material->Get(AI_MATKEY_SHININESS_STRENGTH,
                                 specular_strength);
                ai_material->Get(
                    AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLIC_FACTOR,
                    metallic_factor);
                ai_material->Get(
                    AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_ROUGHNESS_FACTOR,
                    roughness_factor);

                material.two_sided = (two_sided > 0);
                material.opacity = opacity;
                material.alpha_cutoff = alpha_cutoff;
                material.shininess_exponent = shininess_exponent;
                material.specular_strength = specular_strength;
                material.metallic_factor = metallic_factor;
                material.roughness_factor = roughness_factor;
            }

            scene->materials().push_back(std::move(material));
        }
    }

    // Convert meshes
    if (ai_scene->HasMeshes()) {
        for (size_t i = 0; i < ai_scene->mNumMeshes; i++) {
            aiMesh *ai_mesh = ai_scene->mMeshes[i];
            Mesh mesh{ai_mesh->mName.C_Str()};

            // Create the mesh layout
            struct AttrAccessorInfo {
                std::function<bool(const aiMesh &)> has_any;
                std::function<ai_real *(const aiMesh &)> accessor;
                uint8_t vec_size;
                uint8_t stride;  // e.g. UVs have stride 3 but only the first 2
                                 // elements are relevant
            };
            using VertexAttrMap =
                const std::map<VertexAttribute, AttrAccessorInfo>;

            static const VertexAttrMap attr_map = {
                {VertexAttribute::Position,
                 {[](const auto &m) { return m.HasPositions(); },
                  [](const auto &m) { return &m.mVertices[0].x; }, 3, 3}},
                {VertexAttribute::Normal,
                 {[](const auto &m) { return m.HasNormals(); },
                  [](const auto &m) { return &m.mNormals[0].x; }, 3, 3}},
                {VertexAttribute::Tangent,
                 {[](const auto &m) { return m.HasTangentsAndBitangents(); },
                  [](const auto &m) { return &m.mTangents[0].x; }, 3, 3}},
                {VertexAttribute::Bitangent,
                 {[](const auto &m) { return m.HasTangentsAndBitangents(); },
                  [](const auto &m) { return &m.mBitangents[0].x; }, 3, 3}},
                {VertexAttribute::Color,
                 {[](const auto &m) { return m.HasVertexColors(0); },
                  [](const auto &m) { return &m.mColors[0][0].r; }, 4, 4}},
                {VertexAttribute::UV0,
                 {[](const auto &m) { return m.HasTextureCoords(0); },
                  [](const auto &m) { return &m.mTextureCoords[0][0].x; }, 2,
                  3}},
                {VertexAttribute::UV1,
                 {[](const auto &m) { return m.HasTextureCoords(1); },
                  [](const auto &m) { return &m.mTextureCoords[1][0].x; }, 2,
                  3}},
            };

            // Copy the keys (in-order) into a layout vector
            VertexLayout layout;
            layout.reserve(attr_map.size());
            std::transform(attr_map.begin(), attr_map.end(),
                           std::back_inserter(layout),
                           [](const auto &a) { return a.first; });

            // Filter out the attributes not present in the current mesh
            layout.erase(
                std::remove_if(layout.begin(), layout.end(),
                               [&ai_mesh](const auto &attr) {
                                   return !attr_map.at(attr).has_any(*ai_mesh);
                               }),
                layout.end());

            mesh.vertices.data.resize(ai_mesh->mNumVertices *
                                      utils::GetStride(layout));

            // Get the bounding box for the mesh
            AABB aabb;
            std::for_each(ai_mesh->mVertices,
                          ai_mesh->mVertices + ai_mesh->mNumVertices,
                          [&aabb](const auto &v) {
                              if (v.x < aabb.min.x) aabb.min.x = v.x;
                              if (v.x > aabb.max.x) aabb.max.x = v.x;
                              if (v.y < aabb.min.y) aabb.min.y = v.y;
                              if (v.y > aabb.max.y) aabb.max.y = v.y;
                              if (v.z < aabb.min.z) aabb.min.z = v.z;
                              if (v.z > aabb.max.z) aabb.max.z = v.z;
                          });
            mesh.aabb = std::make_unique<AABB>(std::move(aabb));

            // Copy vertex data into the buffer
            auto stride = utils::GetStride(layout);
            std::for_each(
                layout.begin(), layout.end(),
                [&mesh, &ai_mesh, &layout, stride](const auto &attr) {
                    auto offset = utils::GetOffset(layout, attr);
                    const auto &attr_info = attr_map.at(attr);
                    for (unsigned i = 0; i < ai_mesh->mNumVertices; i++) {
                        memcpy(
                            mesh.vertices.data.data() + i * stride + offset,
                            attr_info.accessor(*ai_mesh) + i * attr_info.stride,
                            attr_info.vec_size * sizeof(ai_real));
                    }
                });
            mesh.vertices.size = ai_mesh->mNumVertices;
            mesh.vertices.layout = std::move(layout);

            // Copy index data into the buffer
            if (ai_mesh->HasFaces()) {
                mesh.indices.reserve(ai_mesh->mNumFaces * 3);
                std::for_each(ai_mesh->mFaces,
                              ai_mesh->mFaces + ai_mesh->mNumFaces,
                              [&mesh](const auto &face) {
                                  std::copy(face.mIndices,
                                            face.mIndices + face.mNumIndices,
                                            std::back_inserter(mesh.indices));
                              });
            }

            // Using push_back() for materials guarantees that they are stored
            // in order; a solution using insert() would require keeping track
            // of the actual indices.
            mesh.material_id = {base_material_offset + ai_mesh->mMaterialIndex,
                                0};

            scene->meshes().push_back(std::move(mesh));
        }
    }

    // Convert node structure and meshes
    if (ai_scene->mRootNode) {
        // This will recursively convert all other nodes
        ConvertNode(*ai_scene->mRootNode, scene->root_node(), scene->meshes());
    }

    // Convert embedded textures (if any)
    if (ai_scene->HasTextures()) {
        for (size_t i = 0; i < ai_scene->mNumTextures; i++) {
            aiTexture *ai_texture = ai_scene->mTextures[i];

            if (ai_texture->mFilename.length == 0) {
                spdlog::warn("No filename specified for texture, skipping.");
                continue;
            }

            if (!ai_texture->mHeight) {
                int width, height, n;
                auto image_data = stbi_load_from_memory(
                    reinterpret_cast<const stbi_uc *>(ai_texture->pcData),
                    ai_texture->mWidth, &width, &height, &n, 4);

                if (!image_data) {
                    spdlog::warn("Decompressing \"{}\" failed with error: {}",
                                 ai_texture->mFilename.C_Str(),
                                 stbi_failure_reason());

                    // Decompression failed, store the compressed texture
                    std::vector<uint8_t> data;
                    data.resize(4 * ai_texture->mWidth * ai_texture->mHeight);
                    memcpy(data.data(), ai_texture->pcData, data.size());

                    scene->textures().push_back({ai_texture->mFilename.C_Str(),
                                                 ai_texture->mWidth, 1,
                                                 std::move(data), true});
                } else {
                    std::vector<uint8_t> data;
                    data.resize(4 * width * height);
                    memcpy(data.data(), image_data, data.size());

                    scene->textures().push_back({ai_texture->mFilename.C_Str(),
                                                 static_cast<uint32_t>(width),
                                                 static_cast<uint32_t>(height),
                                                 std::move(data), false});
                }

                stbi_image_free(image_data);
            } else {
                // Uncompressed texture
                std::vector<uint8_t> data;
                data.resize(4 * ai_texture->mWidth * ai_texture->mHeight);
                memcpy(data.data(), ai_texture->pcData, data.size());

                scene->textures().push_back(
                    {ai_texture->mFilename.C_Str(), ai_texture->mWidth,
                     ai_texture->mHeight, std::move(data), false});
            }
        }
    }

    // Convert cameras
    if (ai_scene->HasCameras()) {
        for (size_t i = 0; i < ai_scene->mNumCameras; i++) {
            aiCamera *ai_camera = ai_scene->mCameras[i];

            Camera camera{
                ai_camera->mName.C_Str(),
                glm::degrees(ai_camera->mHorizontalFOV),
                ai_camera->mClipPlaneNear,
                ai_camera->mClipPlaneFar,
                ai_camera->mAspect,
                {ai_camera->mPosition.x, ai_camera->mPosition.y,
                 ai_camera->mPosition.z},
                {ai_camera->mUp.x, ai_camera->mUp.y, ai_camera->mUp.z},
                {ai_camera->mLookAt.x, ai_camera->mLookAt.y,
                 ai_camera->mLookAt.z}};

            auto node = scene->find(ai_camera->mName.C_Str());
            if (node) {
                camera.attach_to(*node);
            }
            scene->cameras().push_back(std::move(camera));
        }
    }

    // Convert lights
    if (ai_scene->HasLights()) {
        for (size_t i = 0; i < ai_scene->mNumLights; i++) {
            aiLight *ai_light = ai_scene->mLights[i];

            static const std::map<aiLightSourceType, LightType> light_type_map =
                {
                    {aiLightSource_UNDEFINED, LightType::Directional},
                    {aiLightSource_DIRECTIONAL, LightType::Directional},
                    {aiLightSource_POINT, LightType::Point},
                    {aiLightSource_SPOT, LightType::Spot},
                    {aiLightSource_AMBIENT, LightType::Ambient},
                    {aiLightSource_AREA, LightType::Area},
                };

            Light light{
                ai_light->mName.C_Str(),
                light_type_map.at(ai_light->mType),
                {ai_light->mPosition.x, ai_light->mPosition.y,
                 ai_light->mPosition.z},
                {ai_light->mDirection.x, ai_light->mDirection.y,
                 ai_light->mDirection.z},
                {ai_light->mUp.x, ai_light->mUp.y, ai_light->mUp.z},
                1.0f,
                {ai_light->mColorDiffuse.r, ai_light->mColorDiffuse.g,
                 ai_light->mColorDiffuse.b},
                {ai_light->mColorSpecular.r, ai_light->mColorSpecular.g,
                 ai_light->mColorSpecular.b},
                {ai_light->mColorAmbient.r, ai_light->mColorAmbient.g,
                 ai_light->mColorAmbient.b},
                {ai_light->mAttenuationConstant, ai_light->mAttenuationLinear,
                 ai_light->mAttenuationQuadratic},
                glm::degrees(ai_light->mAngleInnerCone),
                glm::degrees(ai_light->mAngleOuterCone),
                {ai_light->mSize.x, ai_light->mSize.y}};

            auto node = scene->find(ai_light->mName.C_Str());
            if (node) {
                light.attach_to(*node);
            }
            scene->lights().push_back(std::move(light));
        }
    }

    return std::move(scene);
}  // namespace goma

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
        spdlog::warn("No path specified for texture, skipping.");
        return Error::NotFound;
    }

    // Create a texture using stb_image
    int width, height, n;
    auto full_path = base_path + path.C_Str();
    auto image_data = stbi_load(full_path.c_str(), &width, &height, &n, 4);

    if (!image_data) {
        spdlog::warn("Decompressing \"{}\" failed with error: {}", full_path,
                     stbi_failure_reason());
        return Error::DecompressionFailed;
    }

    std::vector<uint8_t> data;
    data.resize(4 * width * height);
    memcpy(data.data(), image_data, data.size());

    auto tex_id = scene->textures().push_back(
        Texture{path.C_Str(), static_cast<uint32_t>(width),
                static_cast<uint32_t>(height), std::move(data), false});

    static const std::map<aiTextureMapMode, VkSamplerAddressMode>
        texture_wrap_modes{
            {aiTextureMapMode_Wrap, VK_SAMPLER_ADDRESS_MODE_REPEAT},
            {aiTextureMapMode_Clamp, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE},
            {aiTextureMapMode_Mirror, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT},
            {aiTextureMapMode_Decal, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER}};

    TextureBinding tex_binding = {tex_id, texture_wrap_modes.at(mapmode[0]),
                                  uvindex, blend};
    return tex_binding;
}

}  // namespace goma
