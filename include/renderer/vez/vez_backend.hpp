#pragma once

#include "renderer/backend.hpp"
#include "renderer/vez/vez_context.hpp"
#include "common/error_codes.hpp"

#define VK_NO_PROTOTYPES
#include "VEZ.h"
#include "volk.h"

namespace goma {

struct PhysicalDevice {
    VkPhysicalDevice physical_device;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
};

class VezBackend : public Backend {
  public:
    VezBackend(Engine* engine);
    virtual ~VezBackend() = default;

    virtual result<void> InitContext();

    result<VkInstance> CreateInstance();
    result<VkDebugReportCallbackEXT> CreateDebugCallback(VkInstance instance);
    result<PhysicalDevice> CreatePhysicalDevice(VkInstance instance);
    result<VkDevice> CreateDevice(VkPhysicalDevice physical_device);
    result<VezSwapchain> CreateSwapchain(VkDevice device,
                                         VkSurfaceKHR surface){};

  private:
    VezContext context_;
};

}  // namespace goma
