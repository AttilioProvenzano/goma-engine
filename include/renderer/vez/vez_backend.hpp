#pragma once

#include "renderer/backend.hpp"
#include "renderer/vez/vez_context.hpp"
#include "common/error_codes.hpp"

#define VK_NO_PROTOTYPES
#include "VEZ.h"
#include "volk.h"

#include <vector>
#include <map>

namespace goma {

class VezBackend : public Backend {
  public:
    VezBackend(Engine* engine = nullptr);
    virtual ~VezBackend() override;

    virtual result<void> InitContext() override;
    virtual result<void> InitSurface(Platform* platform) override;
    virtual result<std::shared_ptr<Pipeline>> GetGraphicsPipeline(
        const char* vs_source, const char* fs_source,
        const char* vs_entry_point = "main",
        const char* fs_entry_point = "main") override;
    virtual result<std::shared_ptr<VertexInputFormat>> GetVertexInputFormat(
        const VertexInputFormatDesc& desc) override;

    virtual result<Image> CreateTexture(
        const char* name, TextureDesc texture_desc,
        void* initial_contents = nullptr) override;
    virtual result<Image> GetTexture(const char* name) override;
    virtual result<Framebuffer> CreateFramebuffer(
        size_t frame_index, const char* name, FramebufferDesc fb_desc) override;
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

    virtual result<void> SetupFrames(uint32_t frames) override;
    virtual result<size_t> StartFrame(uint32_t threads = 1) override;
    virtual result<void> StartRenderPass(Framebuffer fb,
                                         RenderPassDesc rp_desc) override;

    virtual result<void> BindVertexUniforms(
        const VertexUniforms& vertex_uniforms) override;
    virtual result<void> BindFragmentUniforms(
        const FragmentUniforms& fragment_uniforms) override;
    virtual result<void> BindUniformBuffer(const Buffer& buffer,
                                           uint64_t offset, uint64_t size,
                                           uint32_t binding,
                                           uint32_t array_index = 0) override;
    virtual result<void> BindTextures(
        const std::vector<Image>& images, uint32_t first_binding = 0,
        const SamplerDesc* sampler_override = nullptr) override;
    virtual result<void> BindVertexBuffers(
        const std::vector<Buffer>& vertex_buffers, uint32_t first_binding = 0,
        std::vector<size_t> offsets = {}) override;
    virtual result<void> BindIndexBuffer(const Buffer& index_buffer,
                                         uint64_t offset = 0,
                                         bool short_indices = false) override;
    virtual result<void> BindGraphicsPipeline(Pipeline pipeline) override;
    virtual result<void> BindVertexInputFormat(
        VertexInputFormat vertex_input_format) override;
    virtual result<void> Draw(uint32_t vertex_count,
                              uint32_t instance_count = 1,
                              uint32_t first_vertex = 0,
                              uint32_t first_instance = 0) override;
    virtual result<void> DrawIndexed(uint32_t index_count,
                                     uint32_t instance_count = 1,
                                     uint32_t first_index = 0,
                                     uint32_t vertex_offset = 0,
                                     uint32_t first_instance = 0) override;
    virtual result<void> FinishFrame() override;
    virtual result<void> PresentImage(
        const char* present_image_name = "color") override;

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
    result<VkShaderModule> GetVertexShaderModule(const char* source,
                                                 const char* entry_point);
    result<VkShaderModule> GetFragmentShaderModule(const char* source,
                                                   const char* entry_point);
    result<std::shared_ptr<Buffer>> CreateBuffer(
        VezContext::BufferHash hash, VkDeviceSize size,
        VezMemoryFlagsBits storage, VkBufferUsageFlags usage,
        void* initial_contents = nullptr);
    result<std::shared_ptr<Buffer>> GetBuffer(VezContext::BufferHash hash);
    result<VulkanImage> CreateFramebufferImage(
        size_t frame_index, const FramebufferColorImageDesc& desc,
        uint32_t width, uint32_t height);
    result<VulkanImage> CreateFramebufferImage(
        size_t frame_index, const FramebufferDepthImageDesc& desc,
        uint32_t width, uint32_t height);
    result<VulkanImage> GetFramebufferImage(size_t frame_index,
                                            const char* name);
    result<VkSampler> GetSampler(const SamplerDesc& sampler_desc);

  private:
    VezContext context_;
    bool rp_in_progress_ = false;
    uint32_t thread_id_ = 0;

    result<VulkanImage> CreateImage(VezContext::ImageHash hash,
                                    VezImageCreateInfo image_info,
                                    void* initial_contents = nullptr);
    result<void> GetActiveCommandBuffer(uint32_t thread = 0);
    VkFormat GetVkFormat(Format format);

    VezContext::BufferHash GetBufferHash(const char* name);
    VezContext::BufferHash GetMeshBufferHash(const AttachmentIndex<Mesh>& mesh,
                                             const char* name);
    VezContext::ShaderHash GetShaderHash(const char* source,
                                         const char* entry_point);
    VezContext::PipelineHash GetGraphicsPipelineHash(VkShaderModule vs,
                                                     VkShaderModule fs);
    VezContext::VertexInputFormatHash GetVertexInputFormatHash(
        const VertexInputFormatDesc& desc);
    VezContext::ImageHash GetFramebufferImageHash(size_t frame_index,
                                                  const char* name);
    VezContext::ImageHash GetTextureHash(const char* name);
    VezContext::FramebufferHash GetFramebufferHash(size_t frame_index,
                                                   const char* name);
    VezContext::SamplerHash GetSamplerHash(const SamplerDesc& sampler_desc);
};

}  // namespace goma
