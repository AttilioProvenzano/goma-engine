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

struct PhysicalDevice {
    VkPhysicalDevice physical_device;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
};

class VezBackend : public Backend {
  public:
    VezBackend(Engine* engine = nullptr);
    virtual ~VezBackend() override;

    virtual result<void> InitContext() override;
    virtual result<void> InitSurface(Platform* platform) override;
    virtual result<Pipeline> GetGraphicsPipeline(
        const char* vs_source, const char* fs_source,
        const char* vs_entry_point = "main",
        const char* fs_entry_point = "main") override;

    virtual result<Image> CreateTexture(
        const char* name, TextureDesc texture_desc,
        void* initial_contents = nullptr) override;
    virtual result<Image> GetTexture(const char* name) override;
    virtual result<Framebuffer> CreateFramebuffer(
        uint32_t frame_index, const char* name,
        FramebufferDesc fb_desc) override;

    virtual result<void> SetupFrames(uint32_t frames) override;
    virtual result<size_t> StartFrame(uint32_t threads = 1) override;
    virtual result<void> StartRenderPass(Framebuffer fb,
                                         RenderPassDesc rp_desc) override;
    virtual result<void> BindTextures(const std::vector<Image>& images,
                                      uint32_t first_binding = 0) override;
    virtual result<void> BindVertexBuffers(
        const std::vector<Buffer>& vertex_buffers, uint32_t first_binding = 0,
        std::vector<size_t> offsets = {}) override;
    virtual result<void> BindIndexBuffer(Buffer index_buffer, size_t offset = 0,
                                         bool short_indices = false) override;
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
    virtual result<void> FinishFrame(std::string present_image_name) override;

    virtual result<void> TeardownContext() override;

    result<VkInstance> CreateInstance();
    result<VkDebugReportCallbackEXT> CreateDebugCallback(VkInstance instance);
    result<PhysicalDevice> CreatePhysicalDevice(VkInstance instance);
    result<VkDevice> CreateDevice(VkPhysicalDevice physical_device);
    result<VezSwapchain> CreateSwapchain(VkSurfaceKHR surface);
    result<VkShaderModule> GetVertexShaderModule(const char* source,
                                                 const char* entry_point);
    result<VkShaderModule> GetFragmentShaderModule(const char* source,
                                                   const char* entry_point);
    result<VkBuffer> CreateBuffer(VezContext::BufferHash hash,
                                  VkDeviceSize size, VezMemoryFlagsBits storage,
                                  VkBufferUsageFlags usage,
                                  void* initial_contents = nullptr);
    result<VkBuffer> GetBuffer(VezContext::BufferHash hash);
    result<VulkanImage> CreateFramebufferImage(VezContext::ImageHash hash,
                                               VezImageCreateInfo image_info);
    result<VulkanImage> GetFramebufferImage(VezContext::ImageHash hash);

  private:
    VezContext context_;

    result<VulkanImage> CreateImage(VezContext::ImageHash hash,
                                    VezImageCreateInfo image_info,
                                    void* initial_contents = nullptr);
    result<void> GetActiveCommandBuffer(uint32_t thread = 0);
    VkFormat GetVkFormat(Format format);

    VezContext::ShaderHash GetShaderHash(const char* source,
                                         const char* entry_point);
    VezContext::PipelineHash GetGraphicsPipelineHash(VkShaderModule vs,
                                                     VkShaderModule fs);
    VezContext::ImageHash GetImageHash(const char* name);
    VezContext::FramebufferHash GetFramebufferHash(uint32_t frame_index,
                                                   const char* name);
};

}  // namespace goma
