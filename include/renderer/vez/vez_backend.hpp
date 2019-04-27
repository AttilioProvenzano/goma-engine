#pragma once

#include "renderer/backend.hpp"
#include "renderer/vez/vez_context.hpp"

#include "common/include.hpp"
#include "common/vez.hpp"
#include "common/error_codes.hpp"

namespace goma {

class VezBackend : public Backend {
  public:
    VezBackend(const Config& config = {}, const RenderPlan& render_plan = {});
    virtual ~VezBackend() override;
    virtual result<void> SetRenderPlan(RenderPlan render_plan) override;
    virtual result<void> SetBuffering(Buffering buffering) override;

    virtual result<void> InitContext() override;
    virtual result<void> InitSurface(Platform& platform) override;
    virtual result<std::shared_ptr<Pipeline>> GetGraphicsPipeline(
        const ShaderDesc& vert, const ShaderDesc& frag = {}) override;
    virtual result<std::shared_ptr<VertexInputFormat>> GetVertexInputFormat(
        const VertexInputFormatDesc& desc) override;

    virtual result<std::shared_ptr<Image>> CreateTexture(
        const char* name, const TextureDesc& texture_desc,
        void* initial_contents = nullptr) override;
    virtual result<std::shared_ptr<Image>> CreateTexture(
        const char* name, const TextureDesc& texture_desc,
        const std::vector<void*>& initial_contents) override;
    virtual result<std::shared_ptr<Image>> GetTexture(
        const char* name) override;
    virtual result<std::shared_ptr<Image>> GetRenderTarget(
        FrameIndex frame_id, const char* name) override;

    virtual result<std::shared_ptr<Buffer>> CreateUniformBuffer(
        BufferType type, const GenIndex& index, const char* name, uint64_t size,
        bool gpu_stored = true, void* initial_contents = nullptr) override;
    virtual result<std::shared_ptr<Buffer>> GetUniformBuffer(
        BufferType type, const GenIndex& index, const char* name) override;
    virtual result<std::shared_ptr<Buffer>> CreateUniformBuffer(
        const char* name, uint64_t size, bool gpu_stored = true,
        void* initial_contents = nullptr) override;
    virtual result<std::shared_ptr<Buffer>> GetUniformBuffer(
        const char* name) override;

    virtual result<std::shared_ptr<Buffer>> CreateVertexBuffer(
        const AttachmentIndex<Mesh>& mesh, const char* name, uint64_t size,
        bool gpu_stored = true, void* initial_contents = nullptr) override;
    virtual result<std::shared_ptr<Buffer>> GetVertexBuffer(
        const AttachmentIndex<Mesh>& mesh, const char* name) override;
    virtual result<std::shared_ptr<Buffer>> CreateIndexBuffer(
        const AttachmentIndex<Mesh>& mesh, const char* name, uint64_t size,
        bool gpu_stored = true, void* initial_contents = nullptr) override;
    virtual result<std::shared_ptr<Buffer>> GetIndexBuffer(
        const AttachmentIndex<Mesh>& mesh, const char* name) override;
    virtual result<void> UpdateBuffer(const Buffer& buffer, uint64_t offset,
                                      uint64_t size, void* contents) override;

    virtual result<void> RenderFrame(std::vector<PassFn> pass_fns,
                                     const char* present_image) override;

    virtual result<void> BindUniformBuffer(const Buffer& buffer,
                                           uint64_t offset, uint64_t size,
                                           uint32_t binding,
                                           uint32_t array_index = 0) override;
    virtual result<void> BindTexture(
        const Image& image, uint32_t binding = 0,
        const SamplerDesc* sampler_override = nullptr) override;
    virtual result<void> BindTextures(
        const std::vector<Image>& images, uint32_t first_binding = 0,
        const SamplerDesc* sampler_override = nullptr) override;
    virtual result<void> BindVertexBuffer(const Buffer& vertex_buffer,
                                          uint32_t binding = 0,
                                          size_t offset = 0) override;
    virtual result<void> BindVertexBuffers(
        const std::vector<Buffer>& vertex_buffers, uint32_t first_binding = 0,
        std::vector<size_t> offsets = {}) override;
    virtual result<void> BindIndexBuffer(const Buffer& index_buffer,
                                         uint64_t offset = 0,
                                         bool short_indices = false) override;

    virtual result<void> BindVertexInputFormat(
        VertexInputFormat vertex_input_format) override;
    virtual result<void> BindDepthStencilState(
        const DepthStencilState& state) override;
    virtual result<void> BindColorBlendState(
        const ColorBlendState& state) override;
    virtual result<void> BindMultisampleState(
        const MultisampleState& state) override;
    virtual result<void> BindInputAssemblyState(
        const InputAssemblyState& state) override;
    virtual result<void> BindRasterizationState(
        const RasterizationState& state) override;
    virtual result<void> BindViewportState(uint32_t viewport_count) override;

    virtual result<void> SetDepthBias(float constant_factor, float clamp,
                                      float slope_factor) override;
    virtual result<void> SetDepthBounds(float min, float max) override;
    virtual result<void> SetStencil(StencilFace face, uint32_t reference,
                                    uint32_t write_mask,
                                    uint32_t compare_mask) override;
    virtual result<void> SetBlendConstants(
        const std::array<float, 4>& blend_constants) override;
    virtual result<void> SetViewport(const std::vector<Viewport> viewports,
                                     uint32_t first_viewport = 0) override;
    virtual result<void> SetScissor(const std::vector<Scissor> scissors,
                                    uint32_t first_scissor = 0) override;

    virtual result<void> BindGraphicsPipeline(Pipeline pipeline) override;
    virtual result<void> Draw(uint32_t vertex_count,
                              uint32_t instance_count = 1,
                              uint32_t first_vertex = 0,
                              uint32_t first_instance = 0) override;
    virtual result<void> DrawIndexed(uint32_t index_count,
                                     uint32_t instance_count = 1,
                                     uint32_t first_index = 0,
                                     uint32_t vertex_offset = 0,
                                     uint32_t first_instance = 0) override;

    virtual result<void> TeardownContext() override;

    struct PhysicalDevice {
        VkPhysicalDevice physical_device;
        VkPhysicalDeviceProperties properties;
        VkPhysicalDeviceFeatures features;
    };

    result<VkInstance> CreateInstance();
    result<VkDebugReportCallbackEXT> CreateDebugCallback(VkInstance instance);
    result<PhysicalDevice> CreatePhysicalDevice(VkInstance instance);
    result<VkDevice> CreateDevice(VkPhysicalDevice physical_device);
    result<VezSwapchain> CreateSwapchain(VkSurfaceKHR surface);
    result<VkShaderModule> GetVertexShaderModule(const ShaderDesc& vert);
    result<VkShaderModule> GetFragmentShaderModule(const ShaderDesc& frag);
    result<std::shared_ptr<Buffer>> CreateBuffer(
        VezContext::BufferHash hash, VkDeviceSize size,
        VezMemoryFlagsBits storage, VkBufferUsageFlags usage,
        void* initial_contents = nullptr);
    result<std::shared_ptr<Buffer>> GetBuffer(VezContext::BufferHash hash);
    result<VkSampler> GetSampler(const SamplerDesc& sampler_desc);

  private:
    VezContext context_{};
    bool rp_in_progress_{false};

    result<VulkanImage> CreateImage(VezContext::ImageHash hash,
                                    VezImageCreateInfo image_info,
                                    void* initial_contents = nullptr);
    result<void> GetSetupCommandBuffer();
    result<void> GetActiveCommandBuffer(uint32_t thread = 0);
    VkFormat GetVkFormat(Format format);
    Extent GetAbsoluteExtent(Extent extent);

    result<Framebuffer> CreateFramebuffer(FrameIndex frame_id, const char* name,
                                          RenderPassDesc fb_desc);
    result<Framebuffer> GetFramebuffer(FrameIndex frame_id, const char* name);
    result<std::shared_ptr<Image>> CreateRenderTarget(
        FrameIndex frame_id, const char* name,
        const ColorRenderTargetDesc& desc);
    result<std::shared_ptr<Image>> CreateRenderTarget(
        FrameIndex frame_id, const char* name,
        const DepthRenderTargetDesc& desc);

    result<size_t> StartFrame(uint32_t threads = 1);
    result<void> StartRenderPass(Framebuffer fb, RenderPassDesc rp_desc);
    result<void> FinishFrame();
    result<void> PresentImage(const char* present_image_name);

    VezContext::BufferHash GetBufferHash(const char* name);
    VezContext::BufferHash GetBufferHash(BufferType type, const GenIndex& index,
                                         const char* name);
    VezContext::ShaderHash GetShaderHash(const char* source,
                                         const char* preamble,
                                         const char* entry_point);
    VezContext::PipelineHash GetGraphicsPipelineHash(VkShaderModule vs,
                                                     VkShaderModule fs);
    VezContext::VertexInputFormatHash GetVertexInputFormatHash(
        const VertexInputFormatDesc& desc);
    VezContext::ImageHash GetRenderTargetHash(FrameIndex frame_id,
                                              const char* name);
    VezContext::ImageHash GetTextureHash(const char* name);
    VezContext::FramebufferHash GetFramebufferHash(FrameIndex frame_id,
                                                   const char* name);
    VezContext::SamplerHash GetSamplerHash(const SamplerDesc& sampler_desc);
};

}  // namespace goma
