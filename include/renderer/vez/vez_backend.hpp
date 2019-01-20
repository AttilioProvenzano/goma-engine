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
    result<VezFramebuffer> CreateFramebuffer(VkExtent2D extent);
    result<VulkanImage> CreateImage(VezContext::ImageHash hash,
                                    VezImageCreateInfo image_info,
                                    void* initial_contents = nullptr);

  private:
    VezContext context_;

    VezContext::ShaderHash GetShaderHash(const char* source,
                                         const char* entry_point);
    VezContext::PipelineHash GetGraphicsPipelineHash(VkShaderModule vs,
                                                     VkShaderModule fs);
};

}  // namespace goma
