#include "renderer/vulkan/device.hpp"

#include "common/error_codes.hpp"
#include "platform/platform.hpp"

#define VK_CHECK(fn)                                                         \
    {                                                                        \
        VkResult _r = fn;                                                    \
        if (_r != VK_SUCCESS) {                                              \
            spdlog::error(                                                   \
                "{}, line {}: In function {}, a Vulkan error occurred when " \
                "running {}.",                                               \
                __FILE__, __LINE__, __func__, #fn);                          \
        };                                                                   \
        switch (_r) {                                                        \
            case VK_SUCCESS:                                                 \
                break;                                                       \
            case VK_ERROR_INITIALIZATION_FAILED:                             \
                return Error::VulkanInitializationFailed;                    \
            case VK_ERROR_OUT_OF_HOST_MEMORY:                                \
                return Error::OutOfCPUMemory;                                \
            case VK_ERROR_OUT_OF_DEVICE_MEMORY:                              \
                return Error::OutOfGPUMemory;                                \
            case VK_ERROR_LAYER_NOT_PRESENT:                                 \
                return Error::VulkanLayerNotPresent;                         \
            case VK_ERROR_EXTENSION_NOT_PRESENT:                             \
                return Error::VulkanExtensionNotPresent;                     \
            default:                                                         \
                return Error::GenericVulkanError;                            \
        }                                                                    \
    }

namespace goma {

namespace {

VKAPI_ATTR VkBool32 VKAPI_CALL DebugReportCallback(
    VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT, uint64_t, size_t,
    int32_t, const char* pLayerPrefix, const char* pMessage, void*) {
    if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
        spdlog::error("{} - {}", pLayerPrefix, pMessage);
    } else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
        spdlog::warn("{} - {}", pLayerPrefix, pMessage);
    } else {
        spdlog::info("{} - {}", pLayerPrefix, pMessage);
    }

    return VK_FALSE;
}

result<VkInstance> CreateInstance() {
    VkApplicationInfo appInfo{};
    appInfo.pApplicationName = "Goma App";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
    appInfo.pEngineName = "Goma Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);

    std::vector<const char*> enabled_layers{
        "VK_LAYER_LUNARG_standard_validation",
        "VK_LAYER_LUNARG_monitor",
    };

    uint32_t instance_layer_count;
    vkEnumerateInstanceLayerProperties(&instance_layer_count, nullptr);

    std::vector<VkLayerProperties> instance_layers(instance_layer_count);
    vkEnumerateInstanceLayerProperties(&instance_layer_count,
                                       instance_layers.data());

    std::vector<const char*> instance_layer_names;
    for (auto& enabled_layer : enabled_layers) {
        for (auto& layer : instance_layers) {
            if (!strcmp(layer.layerName, enabled_layer)) {
                instance_layer_names.push_back(layer.layerName);
                break;
            }
        }
    }

    std::vector<const char*> enabled_extensions{
        "VK_KHR_surface", "VK_KHR_win32_surface", "VK_EXT_debug_report"};

    uint32_t instance_extension_count;
    vkEnumerateInstanceExtensionProperties(nullptr, &instance_extension_count,
                                           nullptr);

    std::vector<VkExtensionProperties> instance_extensions(
        instance_extension_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &instance_extension_count,
                                           instance_extensions.data());

    std::vector<const char*> instance_extension_names;
    for (auto& enabled_extension : enabled_extensions) {
        for (auto& extension : instance_extensions) {
            if (!strcmp(extension.extensionName, enabled_extension)) {
                instance_extension_names.push_back(extension.extensionName);
                break;
            }
        }
    }

    VkInstanceCreateInfo instance_info = {
        VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instance_info.pApplicationInfo = &appInfo;
    instance_info.enabledLayerCount =
        static_cast<uint32_t>(instance_layer_names.size());
    instance_info.ppEnabledLayerNames = instance_layer_names.data();
    instance_info.enabledExtensionCount =
        static_cast<uint32_t>(instance_extension_names.size());
    instance_info.ppEnabledExtensionNames = instance_extension_names.data();

    VkInstance instance = VK_NULL_HANDLE;
    VK_CHECK(vkCreateInstance(&instance_info, nullptr, &instance));
    volkLoadInstance(instance);
    return instance;
}

result<VkDebugReportCallbackEXT> CreateDebugCallback(VkInstance instance) {
    VkDebugReportCallbackCreateInfoEXT debug_callback_info{};
    debug_callback_info.sType =
        VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    debug_callback_info.flags =
        // VK_DEBUG_REPORT_DEBUG_BIT_EXT |
        // VK_DEBUG_REPORT_INFORMATION_BIT_EXT |
        VK_DEBUG_REPORT_WARNING_BIT_EXT |
        VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
        VK_DEBUG_REPORT_ERROR_BIT_EXT;
    debug_callback_info.pfnCallback = &DebugReportCallback;

    VkDebugReportCallbackEXT debug_callback = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDebugReportCallbackEXT(instance, &debug_callback_info,
                                            nullptr, &debug_callback));
    return debug_callback;
}

result<VkPhysicalDevice> CreatePhysicalDevice(VkInstance instance) {
    uint32_t physical_device_count;
    vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr);

    std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
    vkEnumeratePhysicalDevices(instance, &physical_device_count,
                               physical_devices.data());

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physical_devices[0], &properties);

    spdlog::info("Physical device: {}, driver version: {}",
                 properties.deviceName, properties.driverVersion);

    return physical_devices[0];
}

result<uint32_t> GetQueueFamilyIndex(VkPhysicalDevice physical_device) {
    // TODO: create multiple queues, separate queues for transfer and
    // compute
    uint32_t queue_family_count;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device,
                                             &queue_family_count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device, &queue_family_count, queue_families.data());

    auto selected_queue_family = std::find_if(
        queue_families.begin(), queue_families.end(), [](const auto& q) {
            return q.queueFlags &
                   (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT |
                    VK_QUEUE_TRANSFER_BIT);
        });
    if (selected_queue_family == queue_families.end()) {
        return Error::QueueFamilyNotFound;
    }

    return selected_queue_family - queue_families.begin();
}

result<VkDevice> CreateDevice(VkPhysicalDevice physical_device,
                              uint32_t queue_family_index) {
    std::vector<const char*> enabled_extensions{"VK_KHR_swapchain"};

    uint32_t device_extension_count;
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr,
                                         &device_extension_count, nullptr);

    std::vector<VkExtensionProperties> device_extensions(
        device_extension_count);
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr,
                                         &device_extension_count,
                                         device_extensions.data());

    std::vector<const char*> device_extension_names;
    for (auto& enabled_extension : enabled_extensions) {
        for (auto& extension : device_extensions) {
            if (!strcmp(extension.extensionName, enabled_extension)) {
                device_extension_names.push_back(extension.extensionName);
                break;
            }
        }
    }

    VkPhysicalDeviceFeatures enabled_features;
    vkGetPhysicalDeviceFeatures(physical_device, &enabled_features);

    // Disable robust buffer access as it has a performance impact
    enabled_features.robustBufferAccess = VK_FALSE;

    uint32_t queue_count = 1;
    auto queue_priorities = std::vector<float>(queue_count, 1.0f);

    VkDeviceQueueCreateInfo queue_info = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queue_info.queueCount = queue_priorities.size();
    queue_info.queueFamilyIndex = queue_family_index;
    queue_info.pQueuePriorities = queue_priorities.data();

    VkDeviceCreateInfo device_info = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    device_info.pEnabledFeatures = &enabled_features;
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;
    device_info.enabledExtensionCount =
        static_cast<uint32_t>(device_extension_names.size());
    device_info.ppEnabledExtensionNames = device_extension_names.data();

    VkDevice device = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDevice(physical_device, &device_info, nullptr, &device));
    volkLoadDevice(device);
    return device;
}

result<VkSurfaceKHR> CreateSurface(Platform& platform, VkInstance instance) {
    OUTCOME_TRY(surface, platform.CreateVulkanSurface(instance));
    return surface;
}

result<VkSwapchainKHR> CreateSwapchain(
    Platform& platform, VkPhysicalDevice physical_device, VkDevice device,
    VkSurfaceKHR surface, VkFormat fb_format, uint32_t queue_family_index,
    VkSwapchainKHR old_swapchain = VK_NULL_HANDLE) {
    // Check that the physical device supports rendering to the given surface
    VkBool32 supported;
    vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, queue_family_index,
                                         surface, &supported);
    if (!supported) {
        return Error::SurfaceNotSupported;
    }

    // Pick a supported format
    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface,
                                         &format_count, nullptr);

    std::vector<VkSurfaceFormatKHR> supported_formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        physical_device, surface, &format_count, supported_formats.data());

    auto selected_format =
        find_if(supported_formats.begin(), supported_formats.end(),
                [fb_format](const auto& sf) { return sf.format == fb_format; });

    if (selected_format == supported_formats.end()) {
        // If the requested format is not available, default to the first
        selected_format = supported_formats.begin();
        spdlog::warn(
            "Requested swapchain format not available, defaulting to {}.",
            selected_format->format);
    }

    // Pick a supported present mode
    uint32_t present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface,
                                              &present_mode_count, nullptr);

    std::vector<VkPresentModeKHR> present_modes(present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        physical_device, surface, &present_mode_count, present_modes.data());

    auto selected_present_mode =
        find(present_modes.begin(), present_modes.end(),
             VK_PRESENT_MODE_MAILBOX_KHR);

    if (selected_present_mode == present_modes.end()) {
        selected_present_mode = present_modes.begin();
    }

    // Check the number of images we are requesting
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface,
                                              &capabilities);

    uint32_t minImageCount = std::max(3U, capabilities.minImageCount);
    if (capabilities.maxImageCount > 0) {
        minImageCount = std::min(minImageCount, capabilities.maxImageCount);
    }

    VkSwapchainCreateInfoKHR swapchain_info = {
        VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    swapchain_info.surface = surface;
    swapchain_info.minImageCount = minImageCount;
    swapchain_info.imageFormat = selected_format->format;
    swapchain_info.imageColorSpace = selected_format->colorSpace;
    swapchain_info.imageExtent = {platform.GetWidth(), platform.GetHeight()};
    swapchain_info.imageArrayLayers = 1;
    swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_info.queueFamilyIndexCount = 1;
    swapchain_info.pQueueFamilyIndices = &queue_family_index;
    swapchain_info.preTransform =
        VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;  // TODO: Handle pre-transform
    swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_info.presentMode = *selected_present_mode;
    swapchain_info.clipped = VK_FALSE;
    swapchain_info.oldSwapchain = old_swapchain;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VK_CHECK(
        vkCreateSwapchainKHR(device, &swapchain_info, nullptr, &swapchain));

    return swapchain;
}

}  // namespace

Device::Device(const Device::Config& config) : config_(config) { Init(); }

result<void> Device::InitWindow(Platform& platform) {
    OUTCOME_TRY(surface, CreateSurface(platform, api_handles_.instance));
    api_handles_.surface = surface;

    VkFormat fb_format =
        (config_.fb_color_space == Config::FbColorSpace::Linear)
            ? VK_FORMAT_B8G8R8A8_UNORM
            : VK_FORMAT_B8G8R8A8_SRGB;

    OUTCOME_TRY(swapchain,
                CreateSwapchain(platform, api_handles_.physical_device,
                                api_handles_.device, api_handles_.surface,
                                fb_format, queue_family_index_));
    api_handles_.swapchain = swapchain;

    return outcome::success();
}

result<void> Device::Init() {
    VK_CHECK(volkInitialize());

    OUTCOME_TRY(instance, CreateInstance());
    api_handles_.instance = instance;

    OUTCOME_TRY(debug_callback, CreateDebugCallback(instance));
    api_handles_.debug_callback = debug_callback;

    OUTCOME_TRY(physical_device, CreatePhysicalDevice(instance));
    api_handles_.physical_device = physical_device;

    OUTCOME_TRY(queue_family_index, GetQueueFamilyIndex(physical_device));
    queue_family_index_ = queue_family_index;

    OUTCOME_TRY(device, CreateDevice(physical_device, queue_family_index));
    api_handles_.device = device;

    return outcome::success();
}

}  // namespace goma
