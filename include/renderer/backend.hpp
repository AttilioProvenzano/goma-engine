#pragma once

#include "renderer/handles.hpp"
#include "platform/platform.hpp"
#include "scene/attachments/mesh.hpp"

#include "common/include.hpp"
#include "common/error_codes.hpp"

namespace goma {

class Engine;

// TODO rework binding push constants
// (potentially just a memory range,
// but does it work in OpenGL?)
struct VertexUniforms {
    glm::mat4 mvp;
};

struct FragmentUniforms {};

enum class Buffering { Double, Triple };

typedef size_t FrameIndex;
typedef std::function<result<void>(RenderPassDesc, FramebufferDesc, FrameIndex)>
    RenderPassFn;

class Backend {
  public:
    struct Config {
        Buffering buffering = Buffering::Triple;
    };

    Backend(Engine* engine = nullptr, const Config& config = {})
        : engine_(engine), config_(config) {}
    virtual ~Backend() = default;

    virtual const RenderPlan& render_plan() { return *render_plan_.get(); }

    const Config& config() { return config_; }
    virtual result<void> SetBuffering(Buffering buffering) {
        return Error::ConfigNotSupported;
    }

    virtual result<void> InitContext() = 0;
    virtual result<void> InitSurface(Platform* platform) = 0;
    virtual result<std::shared_ptr<Pipeline>> GetGraphicsPipeline(
        const ShaderDesc& vert, const ShaderDesc& frag) = 0;
    virtual result<std::shared_ptr<VertexInputFormat>> GetVertexInputFormat(
        const VertexInputFormatDesc& desc) = 0;

    virtual result<std::shared_ptr<Image>> CreateTexture(
        const char* name, const TextureDesc& texture_desc,
        void* initial_contents = nullptr) = 0;
    virtual result<std::shared_ptr<Image>> CreateTexture(
        const char* name, const TextureDesc& texture_desc,
        const std::vector<void*>& initial_contents) = 0;
    virtual result<std::shared_ptr<Image>> GetTexture(const char* name) = 0;

    virtual result<std::shared_ptr<Buffer>> CreateUniformBuffer(
        const NodeIndex& node, const char* name, uint64_t size,
        bool gpu_stored = true, void* initial_contents = nullptr) = 0;
    virtual result<std::shared_ptr<Buffer>> GetUniformBuffer(
        const NodeIndex& node, const char* name) = 0;
    virtual result<std::shared_ptr<Buffer>> CreateUniformBuffer(
        const char* name, uint64_t size, bool gpu_stored = true,
        void* initial_contents = nullptr) = 0;
    virtual result<std::shared_ptr<Buffer>> GetUniformBuffer(
        const char* name) = 0;

    virtual result<std::shared_ptr<Buffer>> CreateVertexBuffer(
        const AttachmentIndex<Mesh>& mesh, const char* name, uint64_t size,
        bool gpu_stored = true, void* initial_contents = nullptr) = 0;
    virtual result<std::shared_ptr<Buffer>> GetVertexBuffer(
        const AttachmentIndex<Mesh>& mesh, const char* name) = 0;
    virtual result<std::shared_ptr<Buffer>> CreateIndexBuffer(
        const AttachmentIndex<Mesh>& mesh, const char* name, uint64_t size,
        bool gpu_stored = true, void* initial_contents = nullptr) = 0;
    virtual result<std::shared_ptr<Buffer>> GetIndexBuffer(
        const AttachmentIndex<Mesh>& mesh, const char* name) = 0;
    virtual result<void> UpdateBuffer(const Buffer& buffer, uint64_t offset,
                                      uint64_t size, void* contents) = 0;

    virtual result<void> SetRenderPlan(const RenderPlan& render_plan) = 0;
    virtual result<void> RenderFrame(std::vector<RenderPassFn> render_pass_fns,
                                     const char* present_image) = 0;

    virtual result<void> BindVertexUniforms(
        const VertexUniforms& vertex_uniforms) = 0;
    virtual result<void> BindFragmentUniforms(
        const FragmentUniforms& fragment_uniforms) = 0;
    virtual result<void> BindUniformBuffer(const Buffer& buffer,
                                           uint64_t offset, uint64_t size,
                                           uint32_t binding,
                                           uint32_t array_index = 0) = 0;
    virtual result<void> BindTexture(
        const Image& image, uint32_t binding = 0,
        const SamplerDesc* sampler_override = nullptr) = 0;
    virtual result<void> BindTextures(
        const std::vector<Image>& images, uint32_t first_binding = 0,
        const SamplerDesc* sampler_override = nullptr) = 0;
    virtual result<void> BindVertexBuffer(const Buffer& vertex_buffer,
                                          uint32_t binding = 0,
                                          size_t offset = 0) = 0;
    virtual result<void> BindVertexBuffers(
        const std::vector<Buffer>& vertex_buffers, uint32_t first_binding = 0,
        std::vector<size_t> offsets = {}) = 0;
    virtual result<void> BindIndexBuffer(const Buffer& index_buffer,
                                         size_t offset = 0,
                                         bool short_indices = false) = 0;

    virtual result<void> BindVertexInputFormat(
        VertexInputFormat vertex_input_format) = 0;
    virtual result<void> BindDepthStencilState(
        const DepthStencilState& state) = 0;
    virtual result<void> BindColorBlendState(const ColorBlendState& state) = 0;
    virtual result<void> BindMultisampleState(
        const MultisampleState& state) = 0;
    virtual result<void> BindInputAssemblyState(
        const InputAssemblyState& state) = 0;
    virtual result<void> BindRasterizationState(
        const RasterizationState& state) = 0;
    virtual result<void> BindViewportState(uint32_t viewport_count) = 0;

    virtual result<void> SetDepthBias(float constant_factor, float clamp,
                                      float slope_factor) = 0;
    virtual result<void> SetDepthBounds(float min, float max) = 0;
    virtual result<void> SetStencil(StencilFace face, uint32_t reference,
                                    uint32_t write_mask,
                                    uint32_t compare_mask) = 0;
    virtual result<void> SetBlendConstants(
        const std::array<float, 4>& blend_constants) = 0;
    virtual result<void> SetViewport(const std::vector<Viewport> viewports,
                                     uint32_t first_viewport = 0) = 0;
    virtual result<void> SetScissor(const std::vector<Scissor> scissors,
                                    uint32_t first_scissor = 0) = 0;

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

    virtual result<void> TeardownContext() = 0;

  protected:
    Engine* engine_ = nullptr;
    Config config_ = {};
    std::unique_ptr<RenderPlan> render_plan_;
};

}  // namespace goma
