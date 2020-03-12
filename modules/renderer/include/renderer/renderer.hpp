#pragma once

#include "rhi/device.hpp"

#include "common/include.hpp"
#include "common/hash.hpp"

namespace std {

template <>
struct hash<goma::ShaderDesc> {
    size_t operator()(const goma::ShaderDesc& desc) const {
        size_t seed = 0;

        // Hash either the path or the source, depending on what is present
        if (!desc.name.empty()) {
            goma::hash_combine(seed, goma::djb2_hash(desc.name.c_str()));
        } else {
            goma::hash_combine(seed, goma::djb2_hash(desc.source.c_str()));
        }

        goma::hash_combine(seed, goma::djb2_hash(desc.preamble.c_str()));
        goma::hash_combine(seed, desc.stage);

        return seed;
    };
};

}  // namespace std

namespace goma {

class Engine;
class GraphicsContext;
class Scene;

class Renderer {
  public:
    Renderer(Engine& engine);

    result<void> Render();

  private:
    Engine& engine_;
    Device device_;

    GraphicsContext graphics_ctx_;
    UploadContext upload_ctx_;

    const uint32_t kMaxFramesInFlight = 3;
    uint32_t frame_index_ = 0;
    uint32_t current_frame_ = 0;

    using ShaderMap = std::unordered_map<ShaderDesc, Shader*>;
    ShaderMap shader_map_;

    result<void> RenderMeshes(GraphicsContext& ctx, Scene& scene);
};

}  // namespace goma
