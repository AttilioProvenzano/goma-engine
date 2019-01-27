#include "scene/assimp_loader.hpp"

namespace goma {

result<std::future<std::unique_ptr<Scene>>> AssimpLoader::ReadSceneFromFile(
    const char* file_path) {
    return std::async([] {
        std::unique_ptr<Scene> scene = std::make_unique<Scene>();
        scene->CreateNode(scene->GetRootNode());
        return std::move(scene);
    });
}

}  // namespace goma
