#include "renderer/vez/vez_backend.hpp"

#include "engine.hpp"

#include <array>

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

uint64_t sdbm_hash(const char* str) {
    uint64_t hash = 0;
    int c;

    while (c = *str++) hash = c + (hash << 6) + (hash << 16) - hash;

    return hash;
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

VezBackend::~VezBackend() { TeardownContext(); }

result<void> VezBackend::InitContext() {
    VK_CHECK(volkInitialize());

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

    return outcome::success();
}

result<void> VezBackend::InitSurface(Platform* platform) {
    assert(platform && "Platform must not be null");
    assert(context_.instance && "Context must be initialized");
    assert(context_.physical_device && "Context must be initialized");
    assert(context_.device && "Context must be initialized");

    OUTCOME_TRY(surface, platform->CreateVulkanSurface(context_.instance));
    context_.surface = surface;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context_.physical_device, surface,
                                              &context_.capabilities);

    OUTCOME_TRY(swapchain, CreateSwapchain(context_.device, surface));
    context_.swapchain = swapchain;

    return outcome::success();
}

result<Pipeline> VezBackend::GetGraphicsPipeline(const char* vs_source,
                                                 const char* fs_source,
                                                 const char* vs_entry_point,
                                                 const char* fs_entry_point) {
    assert(context_.device &&
           "Context must be initialized before creating a pipeline");

    VkDevice device = context_.device;
    OUTCOME_TRY(vertex_shader,
                GetVertexShaderModule(device, vs_source, vs_entry_point));
    OUTCOME_TRY(fragment_shader,
                GetFragmentShaderModule(device, fs_source, fs_entry_point));

    PipelineHash hash = GetGraphicsPipelineHash(vertex_shader, fragment_shader);
    auto result = pipeline_cache_.find(hash);

    if (result != pipeline_cache_.end()) {
        return result->second;
    } else {
        std::array<VezPipelineShaderStageCreateInfo, 2> shader_stages = {
            {{}, {}}};
        shader_stages[0].module = vertex_shader;
        shader_stages[1].module = fragment_shader;

        VezGraphicsPipelineCreateInfo pipeline_info;
        pipeline_info.stageCount = static_cast<uint32_t>(shader_stages.size());
        pipeline_info.pStages = shader_stages.data();

        VezPipeline pipeline = VK_NULL_HANDLE;
        VK_CHECK(vezCreateGraphicsPipeline(device, &pipeline_info, &pipeline));
        pipeline_cache_[hash] = pipeline;
        return {pipeline};
    }
}

result<void> VezBackend::TeardownContext() {
    for (auto pipeline : pipeline_cache_) {
        vezDestroyPipeline(context_.device, pipeline.second);
    }
    pipeline_cache_.clear();

    for (auto shader : vertex_shader_cache_) {
        vezDestroyShaderModule(context_.device, shader.second);
    }
    vertex_shader_cache_.clear();

    for (auto shader : fragment_shader_cache_) {
        vezDestroyShaderModule(context_.device, shader.second);
    }
    fragment_shader_cache_.clear();

    if (context_.swapchain) {
        vezDestroySwapchain(context_.device, context_.swapchain);
    }

    if (context_.surface) {
        vkDestroySurfaceKHR(context_.instance, context_.surface, nullptr);
    }

    if (context_.device) {
        vezDestroyDevice(context_.device);
    }

    if (context_.debug_callback) {
        vkDestroyDebugReportCallbackEXT(context_.instance,
                                        context_.debug_callback, nullptr);
    }

    if (context_.instance) {
        vezDestroyInstance(context_.instance);
    }

    context_ = VezContext();
    return outcome::success();
}

result<VkInstance> VezBackend::CreateInstance() {
    // TODO application name and version to be filled
    VezApplicationInfo appInfo = {};
    appInfo.pApplicationName = "Goma App";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
    appInfo.pEngineName = "Goma Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);

    std::vector<const char*> enabled_layers = {
        "VK_LAYER_LUNARG_standard_validation"};

    uint32_t instance_layer_count;
    vezEnumerateInstanceLayerProperties(&instance_layer_count, nullptr);

    std::vector<VkLayerProperties> instance_layers(instance_layer_count);
    vezEnumerateInstanceLayerProperties(&instance_layer_count,
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

    std::vector<const char*> enabled_extensions = {
        "VK_KHR_surface", "VK_KHR_win32_surface", "VK_EXT_debug_report"};

#ifndef BYPASS_EXTENSION_ENUMERATION
    uint32_t instance_extension_count;
    vezEnumerateInstanceExtensionProperties(nullptr, &instance_extension_count,
                                            nullptr);

    std::vector<VkExtensionProperties> instance_extensions(
        instance_extension_count);
    vezEnumerateInstanceExtensionProperties(nullptr, &instance_extension_count,
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
#else
    std::vector<const char*> instance_extension_names =
        std::move(enabled_extensions);
#endif

    VezInstanceCreateInfo instance_info = {};
    instance_info.pApplicationInfo = &appInfo;
    instance_info.enabledLayerCount =
        static_cast<uint32_t>(instance_layer_names.size());
    instance_info.ppEnabledLayerNames = instance_layer_names.data();
    instance_info.enabledExtensionCount =
        static_cast<uint32_t>(instance_extension_names.size());
    instance_info.ppEnabledExtensionNames = instance_extension_names.data();

    VkInstance instance = VK_NULL_HANDLE;
    VK_CHECK(vezCreateInstance(&instance_info, &instance));
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
                                            nullptr, &debug_callback));
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
    std::vector<const char*> enabled_extensions = {"VK_KHR_swapchain"};

#ifndef BYPASS_EXTENSION_ENUMERATION
    uint32_t device_extension_count;
    vezEnumerateDeviceExtensionProperties(physical_device, nullptr,
                                          &device_extension_count, nullptr);

    std::vector<VkExtensionProperties> device_extensions(
        device_extension_count);
    vezEnumerateDeviceExtensionProperties(physical_device, nullptr,
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
#else
    std::vector<const char*> device_extension_names =
        std::move(enabled_extensions);
#endif

    VezDeviceCreateInfo device_info = {};
    device_info.enabledExtensionCount =
        static_cast<uint32_t>(device_extension_names.size());
    device_info.ppEnabledExtensionNames = device_extension_names.data();

    VkDevice device = VK_NULL_HANDLE;
    VK_CHECK(vezCreateDevice(physical_device, &device_info, &device));
    volkLoadDevice(device);
    return device;
}

result<VezSwapchain> VezBackend::CreateSwapchain(VkDevice device,
                                                 VkSurfaceKHR surface) {
    VezSwapchainCreateInfo swapchain_info = {};
    swapchain_info.surface = surface;
    swapchain_info.tripleBuffer = VK_TRUE;
    swapchain_info.format = {VK_FORMAT_R8G8B8A8_UNORM,
                             VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

    VezSwapchain swapchain = VK_NULL_HANDLE;
    VK_CHECK(vezCreateSwapchain(device, &swapchain_info, &swapchain));
    return swapchain;
}

result<VkShaderModule> VezBackend::GetVertexShaderModule(
    VkDevice device, const char* source, const char* entry_point) {
    ShaderHash hash = GetShaderHash(source, entry_point);
    auto result = vertex_shader_cache_.find(hash);

    if (result != vertex_shader_cache_.end()) {
        return result->second;
    } else {
        VezShaderModuleCreateInfo shader_info = {};
        shader_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
        shader_info.codeSize = strlen(source);
        shader_info.pGLSLSource = source;
        shader_info.pEntryPoint = entry_point;

        VkShaderModule shader = VK_NULL_HANDLE;
        VK_CHECK(vezCreateShaderModule(device, &shader_info, &shader));
        vertex_shader_cache_[hash] = shader;
        return shader;
    }
}

result<VkShaderModule> VezBackend::GetFragmentShaderModule(
    VkDevice device, const char* source, const char* entry_point) {
    ShaderHash hash = GetShaderHash(source, entry_point);
    auto result = fragment_shader_cache_.find(hash);

    if (result != fragment_shader_cache_.end()) {
        return result->second;
    } else {
        VezShaderModuleCreateInfo shader_info = {};
        shader_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
        shader_info.codeSize = strlen(source);
        shader_info.pGLSLSource = source;
        shader_info.pEntryPoint = entry_point;

        VkShaderModule shader = VK_NULL_HANDLE;
        VK_CHECK(vezCreateShaderModule(device, &shader_info, &shader));
        fragment_shader_cache_[hash] = shader;
        return shader;
    }
}

VezBackend::ShaderHash VezBackend::GetShaderHash(const char* source,
                                                 const char* entry_point) {
    return {sdbm_hash(source), sdbm_hash(entry_point)};
}

VezBackend::PipelineHash VezBackend::GetGraphicsPipelineHash(
    VkShaderModule vs, VkShaderModule fs) {
    return {vs, fs};
}

}  // namespace goma
