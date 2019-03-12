#pragma once

#include "renderer/backend.hpp"

#include <map>
#include <memory>

namespace goma {

class Engine;

class Renderer {
  public:
    Renderer(Engine* engine);

    result<void> Render();

  private:
    Engine* engine_ = nullptr;
    std::unique_ptr<Backend> backend_;

    std::shared_ptr<Pipeline> pipeline_;
    std::vector<Framebuffer> framebuffers_;

    union VertexShaderPreambleDesc {
        struct {
            bool has_positions : 1;
            bool has_normals : 1;
            bool has_tangents : 1;
            bool has_bitangents : 1;
            bool has_colors : 1;
            bool has_uv0 : 1;
            bool has_uv1 : 1;
            bool has_uvw : 1;
        };

        uint32_t int_repr;
    };

    std::map<uint32_t, std::string> vs_preamble_map_;
    const char* GetVertexShaderPreamble(const VertexShaderPreambleDesc& desc);
    const char* GetVertexShaderPreamble(const Mesh& mesh);
};

}  // namespace goma
