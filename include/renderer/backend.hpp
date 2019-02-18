#pragma once

#include "renderer/handles.hpp"
#include "platform/platform.hpp"

#include <outcome.hpp>
namespace outcome = OUTCOME_V2_NAMESPACE;
using outcome::result;
#include <vector>

namespace goma {

class Engine;

class Backend {
  public:
    Backend(Engine* engine = nullptr) : engine_(engine) {}
    virtual ~Backend() = default;

    virtual result<void> InitContext() = 0;
    virtual result<void> InitSurface(Platform* platform) = 0;
    virtual result<Pipeline> GetGraphicsPipeline(
        const char* vs_source, const char* fs_source,
        const char* vs_entry_point = "main",
        const char* fs_entry_point = "main") = 0;
    virtual result<VertexInputFormat> GetVertexInputFormat(
        const VertexInputFormatDesc& desc) = 0;

    virtual result<Image> CreateTexture(const char* name,
                                        TextureDesc texture_desc,
                                        void* initial_contents = nullptr) = 0;
    virtual result<Image> GetTexture(const char* name) = 0;
    virtual result<Framebuffer> CreateFramebuffer(size_t frame_index,
                                                  const char* name,
                                                  FramebufferDesc fb_desc) = 0;
    virtual result<Buffer> CreateVertexBuffer(
        const char* name, uint64_t size, bool gpu_stored = true,
        void* initial_contents = nullptr) = 0;
    virtual result<Buffer> GetVertexBuffer(const char* name) = 0;
    virtual result<Buffer> CreateIndexBuffer(
        const char* name, uint64_t size, bool gpu_stored = true,
        void* initial_contents = nullptr) = 0;
    virtual result<Buffer> GetIndexBuffer(const char* name) = 0;
    virtual result<void> UpdateBuffer(Buffer buffer, uint64_t offset,
                                      uint64_t size, void* contents) = 0;

    virtual result<void> SetupFrames(uint32_t frames) = 0;
    virtual result<size_t> StartFrame(uint32_t threads = 1) = 0;
    virtual result<void> StartRenderPass(Framebuffer fb,
                                         RenderPassDesc rp_desc) = 0;
    virtual result<void> BindUniformBuffer(Buffer buffer, uint64_t offset,
                                           uint64_t size, uint32_t binding,
                                           uint32_t array_index = 0) = 0;
    virtual result<void> BindTextures(
        const std::vector<Image>& images, uint32_t first_binding = 0,
        const SamplerDesc* sampler_override = nullptr) = 0;
    virtual result<void> BindVertexBuffers(
        const std::vector<Buffer>& vertex_buffers, uint32_t first_binding = 0,
        std::vector<size_t> offsets = {}) = 0;
    virtual result<void> BindIndexBuffer(Buffer index_buffer, size_t offset = 0,
                                         bool short_indices = false) = 0;
    virtual result<void> BindVertexInputFormat(
        VertexInputFormat vertex_input_format) = 0;
    virtual result<void> BindGraphicsPipeline(Pipeline pipeline) = 0;
    virtual result<void> Draw(uint32_t vertex_count,
                              uint32_t instance_count = 1,
                              uint32_t first_vertex = 0,
                              uint32_t first_instance = 0) = 0;
    virtual result<void> DrawIndexed(uint32_t index_count,
                                     uint32_t instance_count = 1,
                                     uint32_t first_index = 0,
                                     uint32_t vertex_offset = 0,
                                     uint32_t first_instance = 0) = 0;
    virtual result<void> FinishFrame() = 0;
    virtual result<void> PresentImage(
        const char* present_image_name = "color") = 0;

    virtual result<void> TeardownContext() = 0;

  protected:
    Engine* engine_ = nullptr;
};

}  // namespace goma
