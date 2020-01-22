#pragma once

#include "renderer/handles.hpp"
#include "platform/platform.hpp"
#include "scene/attachments/mesh.hpp"

#include "common/include.hpp"
#include "common/error_codes.hpp"

namespace goma {

class Engine;

enum class Buffering { Double, Triple };
enum class FramebufferColorSpace { Linear, Srgb };

using FrameIndex = size_t;
using PassFn = std::function<result<void>(FrameIndex, const RenderPassDesc*)>;

class Backend {
  public:
    struct Config {
        Buffering buffering{Buffering::Triple};
        FramebufferColorSpace fb_color_space{FramebufferColorSpace::Linear};
    };

    Backend(const Config& config = {}, const RenderPlan& render_plan = {})
        : config_(config), render_plan_(render_plan) {}
    virtual ~Backend() = default;

    const RenderPlan& render_plan() { return render_plan_; }
    const Config& config() { return config_; }

    virtual result<void> SetRenderPlan(RenderPlan render_plan) {
        render_plan_ = std::move(render_plan);
        return outcome::success();
    }

    virtual result<void> SetBuffering(Buffering) {
        return Error::ConfigNotSupported;
    }

    virtual result<void> SetFramebufferColorSpace(
        FramebufferColorSpace fb_color_space) {
        return Error::ConfigNotSupported;
    }

    virtual result<void> InitContext() = 0;
    virtual result<void> InitSurface(Platform& platform) = 0;
    virtual result<std::shared_ptr<Pipeline>> GetGraphicsPipeline(
        const ShaderDesc& vert, const ShaderDesc& frag = {}) = 0;
    virtual result<void> ClearShaderCache() = 0;
    virtual result<std::shared_ptr<VertexInputFormat>> GetVertexInputFormat(
        const VertexInputFormatDesc& desc) = 0;

    virtual result<std::shared_ptr<Image>> CreateTexture(
        const char* name, const TextureDesc& texture_desc,
        void* initial_contents = nullptr) = 0;
    virtual result<std::shared_ptr<Image>> CreateCubemap(
        const char* name, const TextureDesc& texture_desc,
        const CubemapContents& initial_contents) = 0;
    virtual result<std::shared_ptr<Image>> GetTexture(const char* name) = 0;
    virtual result<std::shared_ptr<Image>> GetRenderTarget(
        FrameIndex frame_id, const char* name) = 0;
    virtual Extent GetAbsoluteExtent(Extent extent) = 0;

    virtual result<std::shared_ptr<Buffer>> CreateUniformBuffer(
        BufferType type, const GenIndex& index, const char* name, uint64_t size,
        bool gpu_stored = true, void* initial_contents = nullptr) = 0;
    virtual result<std::shared_ptr<Buffer>> GetUniformBuffer(
        BufferType type, const GenIndex& index, const char* name) = 0;
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
                                      uint64_t size, const void* contents) = 0;

    virtual result<void> RenderFrame(std::vector<PassFn> pass_fns,
                                     const char* present_image) = 0;

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
    Config config_{};
    RenderPlan render_plan_{};
};

}  // namespace goma
