#pragma once

#define VK_NO_PROTOTYPES
#include "VEZ.h"

#include "renderer/handles.hpp"

#include <vector>
#include <map>
#include <set>

namespace goma {

struct VezContext {
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugReportCallbackEXT debug_callback = VK_NULL_HANDLE;

    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties properties = {};
    VkPhysicalDeviceFeatures features = {};

    VkDevice device = VK_NULL_HANDLE;

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSurfaceCapabilitiesKHR capabilities = {};

    VezSwapchain swapchain = VK_NULL_HANDLE;

    std::map<PipelineHash, Pipeline> pipelines;

    struct PerFrame {
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> command_buffers;
    };
    std::vector<PerFrame> per_frame;
};

}  // namespace goma
