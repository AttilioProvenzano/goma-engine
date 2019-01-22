#pragma once

#define VK_NO_PROTOTYPES
#include "VEZ.h"

#include "renderer/handles.hpp"

#include <vector>
#include <map>
#include <set>

namespace goma {

struct VezContext {
    typedef std::vector<uint64_t> ShaderHash;
    typedef std::map<ShaderHash, VkShaderModule> ShaderCache;

    typedef std::vector<VkShaderModule> PipelineHash;
    typedef std ::map<PipelineHash, VezPipeline> PipelineCache;

    typedef std::vector<uint64_t> BufferHash;
    typedef std ::map<BufferHash, VkBuffer> BufferCache;

	typedef std::vector<uint64_t> ImageHash;
    typedef std::map<ImageHash, VulkanImage> ImageCache;

	typedef std::vector<uint64_t> FramebufferHash;
    typedef std::map<FramebufferHash, VezFramebuffer> FramebufferCache;

    VkInstance instance = VK_NULL_HANDLE;
    VkDebugReportCallbackEXT debug_callback = VK_NULL_HANDLE;

    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties properties = {};
    VkPhysicalDeviceFeatures features = {};

    VkDevice device = VK_NULL_HANDLE;

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSurfaceCapabilitiesKHR capabilities = {};

    VezSwapchain swapchain = VK_NULL_HANDLE;

    ShaderCache vertex_shader_cache;
    ShaderCache fragment_shader_cache;
    PipelineCache pipeline_cache;
    BufferCache buffer_cache;
    ImageCache fb_image_cache;
    ImageCache texture_cache;
    FramebufferCache framebuffer_cache;

    struct PerFrame {
        std::vector<VkCommandBuffer> command_buffers;
        std::vector<bool> command_buffer_active;
    };
    std::vector<PerFrame> per_frame;
    size_t current_frame = 0;
};

}  // namespace goma
