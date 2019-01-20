#include "renderer/vez/vez_backend.hpp"

#include "engine.hpp"

#define VK_CHECK(fn)                                      \
    {                                                     \
        VkResult result = fn;                             \
        switch (result) {                                 \
            case VK_SUCCESS:                              \
                break;                                    \
            case VK_ERROR_INITIALIZATION_FAILED:          \
                return Error::VulkanInitializationFailed; \
            case VK_ERROR_OUT_OF_HOST_MEMORY:             \
                return Error::OutOfCPUMemory;             \
            case VK_ERROR_OUT_OF_DEVICE_MEMORY:           \
                return Error::OutOfGPUMemory;             \
            case VK_ERROR_LAYER_NOT_PRESENT:              \
                return Error::VulkanLayerNotPresent;      \
            case VK_ERROR_EXTENSION_NOT_PRESENT:          \
                return Error::VulkanExtensionNotPresent;  \
            default:                                      \
                return Error::GenericVulkanError;         \
        }                                                 \
    }

#define LOG(prefix, format, ...) printf(prefix format "\n", __VA_ARGS__)
#define LOGE(format, ...) LOG("** ERROR: ", format, __VA_ARGS__)
#define LOGW(format, ...) LOG("* Warning: ", format, __VA_ARGS__)
#define LOGI(format, ...) LOG("", format, __VA_ARGS__)

VKAPI_ATTR VkBool32 VKAPI_CALL DebugReportCallback(
    VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType,
    uint64_t object, size_t location, int32_t messageCode,
    const char* pLayerPrefix, const char* pMessage, void* pUserData) {
    if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
        LOGE("%s - %s", pLayerPrefix, pMessage);
    } else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
        LOGW("%s - %s", pLayerPrefix, pMessage);
    } else {
        LOGI("%s - %s", pLayerPrefix, pMessage);
    }

    return VK_FALSE;
}

namespace goma {

VezBackend::VezBackend(Engine* engine) : Backend(engine) {}

result<void> VezBackend::InitContext() {
    VK_CHECK(volkInitialize())

    OUTCOME_TRY(instance, CreateInstance());
    context_.instance = instance;

    OUTCOME_TRY(debug_callback, CreateDebugCallback(instance));
    context_.debug_callback = debug_callback;

    OUTCOME_TRY(p, CreatePhysicalDevice(instance));
    context_.physical_device = p.physical_device;
    context_.features = p.features;
    context_.properties = p.properties;

    OUTCOME_TRY(device, CreateDevice(p.physical_device));
    context_.device = device;

    OUTCOME_TRY(surface,
                engine_->platform()->CreateVulkanSurface(context_.instance));
    context_.surface = surface;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(p.physical_device, surface,
                                              &context_.capabilities);

    return outcome::success();
}

result<VkInstance> VezBackend::CreateInstance() {
    // TODO application name and version to be filled
    VezApplicationInfo appInfo = {};
    appInfo.pApplicationName = "Goma App";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
    appInfo.pEngineName = "Goma Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);

    uint32_t instance_layer_count;
    vezEnumerateInstanceLayerProperties(&instance_layer_count, nullptr);

    std::vector<VkLayerProperties> instance_layers(instance_layer_count);
    vezEnumerateInstanceLayerProperties(&instance_layer_count,
                                        instance_layers.data());

    std::vector<const char*> enabled_layers = {
        "VK_LAYER_LUNARG_standard_validation"};

    std::vector<const char*> instance_layer_names;
    for (auto& layer : instance_layers) {
        for (auto& enabled_layer : enabled_layers) {
            if (!strcmp(layer.layerName, enabled_layer)) {
                instance_layer_names.push_back(layer.layerName);
                break;
            }
        }
    }

    uint32_t instance_extension_count;
    vezEnumerateInstanceExtensionProperties(nullptr, &instance_extension_count,
                                            nullptr);

    std::vector<VkExtensionProperties> instance_extensions(
        instance_extension_count);
    vezEnumerateInstanceExtensionProperties(nullptr, &instance_extension_count,
                                            instance_extensions.data());

    std::vector<const char*> enabled_extensions = {
        "VK_KHR_surface", "VK_KHR_win32_surface", "VK_EXT_debug_report"};

    std::vector<const char*> instance_extension_names;
    for (auto& extension : instance_extensions) {
        for (auto& enabled_extension : enabled_extensions) {
            if (!strcmp(extension.extensionName, enabled_extension)) {
                instance_extension_names.push_back(extension.extensionName);
                break;
            }
        }
    }

    VezInstanceCreateInfo instance_info = {};
    instance_info.pApplicationInfo = &appInfo;
    instance_info.enabledLayerCount =
        static_cast<uint32_t>(instance_layer_names.size());
    instance_info.ppEnabledLayerNames = instance_layer_names.data();
    instance_info.enabledExtensionCount =
        static_cast<uint32_t>(instance_extension_names.size());
    instance_info.ppEnabledExtensionNames = instance_extension_names.data();

    VkInstance instance = VK_NULL_HANDLE;
    VK_CHECK(vezCreateInstance(&instance_info, &instance))
    volkLoadInstance(instance);
    return instance;
}

result<VkDebugReportCallbackEXT> VezBackend::CreateDebugCallback(
    VkInstance instance) {
    VkDebugReportCallbackCreateInfoEXT debug_callback_info = {};
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
                                            nullptr, &debug_callback))
    return debug_callback;
}

result<PhysicalDevice> VezBackend::CreatePhysicalDevice(VkInstance instance) {
    uint32_t physical_device_count;
    vezEnumeratePhysicalDevices(instance, &physical_device_count, nullptr);

    std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
    vezEnumeratePhysicalDevices(instance, &physical_device_count,
                                physical_devices.data());

    PhysicalDevice p = {physical_devices[0]};
    vezGetPhysicalDeviceProperties(p.physical_device, &p.properties);
    vezGetPhysicalDeviceFeatures(p.physical_device, &p.features);
    return p;
}

result<VkDevice> VezBackend::CreateDevice(VkPhysicalDevice physical_device) {
    uint32_t device_extension_count;
    vezEnumerateDeviceExtensionProperties(physical_device, nullptr,
                                          &device_extension_count, nullptr);

    std::vector<VkExtensionProperties> device_extensions(
        device_extension_count);
    vezEnumerateDeviceExtensionProperties(physical_device, nullptr,
                                          &device_extension_count,
                                          device_extensions.data());

    std::vector<const char*> enabled_extensions = {"VK_KHR_swapchain"};

    std::vector<const char*> device_extension_names;
    for (auto& extension : device_extensions) {
        for (auto& enabled_extension : enabled_extensions) {
            if (!strcmp(extension.extensionName, enabled_extension)) {
                device_extension_names.push_back(extension.extensionName);
                break;
            }
        }
    }

    VezDeviceCreateInfo device_info = {};
    device_info.enabledExtensionCount =
        static_cast<uint32_t>(device_extension_names.size());
    device_info.ppEnabledExtensionNames = device_extension_names.data();

    VkDevice device = VK_NULL_HANDLE;
    VK_CHECK(vezCreateDevice(physical_device, &device_info, &device))
    volkLoadDevice(device);
    return device;
}

result<VezSwapchain> CreateSwapchain(VkDevice device, VkSurfaceKHR surface) {
    VezSwapchainCreateInfo swapchain_info = {};
    swapchain_info.surface = surface;
    swapchain_info.tripleBuffer = VK_TRUE;
    swapchain_info.format = {VK_FORMAT_R8G8B8A8_UNORM,
                             VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

    VezSwapchain swapchain = VK_NULL_HANDLE;
    VK_CHECK(vezCreateSwapchain(device, &swapchain_info, &swapchain));
    return swapchain;
}

}  // namespace goma
