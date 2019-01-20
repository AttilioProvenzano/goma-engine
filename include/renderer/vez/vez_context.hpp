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
    ImageCache image_cache;

    struct PerFrame {
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> command_buffers;
    };
    std::vector<PerFrame> per_frame;
};

}  // namespace goma
