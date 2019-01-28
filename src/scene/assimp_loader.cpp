#include "scene/assimp_loader.hpp"

namespace goma {

result<std::unique_ptr<Scene>> AssimpLoader::ReadSceneFromFile(
    const char* file_path) {
    std::unique_ptr<Scene> scene = std::make_unique<Scene>();
    scene->CreateNode(scene->GetRootNode());
    return std::move(scene);
}

}  // namespace goma
