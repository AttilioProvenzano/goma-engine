#pragma once

#include "common/include.hpp"
#include "common/hash.hpp"
#include "common/gen_vector.hpp"
#include "rhi/device.hpp"

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
    }
};

}  // namespace std

namespace goma {

class GraphicsContext;
class Image;
class Renderer;

struct Mesh;

class RenderingPipeline {
  public:
    using BindingMap = std::map<std::string, Image*>;
    using BindingKeys = std::vector<std::string>;
    using GfxCtx = GraphicsContext;

    RenderingPipeline(Renderer& renderer, BindingKeys outputs,
                      BindingKeys inputs = {})
        : renderer_(renderer),
          output_interface_(outputs),
          input_interface_(inputs) {}

    virtual ~RenderingPipeline() = default;

    virtual result<void> run(GfxCtx& ctx, BindingMap outputs,
                             BindingMap inputs = {}) = 0;

    BindingKeys output_interface() { return output_interface_; }
    BindingKeys input_interface() { return input_interface_; }

  protected:
    Renderer& renderer_;

    BindingKeys output_interface_;
    BindingKeys input_interface_;

    using ShaderMap = std::unordered_map<ShaderDesc, Shader*>;
    ShaderMap shader_map_;

    using MeshIter = gen_vector<Mesh>::iterator;
    result<void> render_meshes(GfxCtx& ctx, MeshIter first, MeshIter last);
};

}  // namespace goma
