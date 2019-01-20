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

    OUTCOME_TRY(swapchain, CreateSwapchain(surface));
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
                GetVertexShaderModule(vs_source, vs_entry_point));
    OUTCOME_TRY(fragment_shader,
                GetFragmentShaderModule(fs_source, fs_entry_point));

    auto hash = GetGraphicsPipelineHash(vertex_shader, fragment_shader);
    auto result = context_.pipeline_cache.find(hash);

    if (result != context_.pipeline_cache.end()) {
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
        context_.pipeline_cache[hash] = pipeline;
        return {pipeline};
    }
}

result<void> VezBackend::TeardownContext() {
    for (auto image : context_.image_cache) {
        vezDestroyImageView(context_.device, image.second.image_view);
        vezDestroyImage(context_.device, image.second.image);
    }

    for (auto buffer : context_.buffer_cache) {
        vezDestroyBuffer(context_.device, buffer.second);
	}

    for (auto pipeline : context_.pipeline_cache) {
        vezDestroyPipeline(context_.device, pipeline.second);
    }

    for (auto shader : context_.vertex_shader_cache) {
        vezDestroyShaderModule(context_.device, shader.second);
    }

    for (auto shader : context_.fragment_shader_cache) {
        vezDestroyShaderModule(context_.device, shader.second);
    }

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

result<VezSwapchain> VezBackend::CreateSwapchain(VkSurfaceKHR surface) {
    assert(context_.device &&
           "Context must be initialized before creating a swapchain");
    VkDevice device = context_.device;

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
    const char* source, const char* entry_point) {
    assert(context_.device &&
           "Context must be initialized before creating a shader");
    VkDevice device = context_.device;

    auto hash = GetShaderHash(source, entry_point);
    auto result = context_.vertex_shader_cache.find(hash);

    if (result != context_.vertex_shader_cache.end()) {
        return result->second;
    } else {
        VezShaderModuleCreateInfo shader_info = {};
        shader_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
        shader_info.codeSize = strlen(source);
        shader_info.pGLSLSource = source;
        shader_info.pEntryPoint = entry_point;

        VkShaderModule shader = VK_NULL_HANDLE;
        VK_CHECK(vezCreateShaderModule(device, &shader_info, &shader));
        context_.vertex_shader_cache[hash] = shader;
        return shader;
    }
}

result<VkShaderModule> VezBackend::GetFragmentShaderModule(
    const char* source, const char* entry_point) {
    assert(context_.device &&
           "Context must be initialized before creating a shader");
    VkDevice device = context_.device;

    auto hash = GetShaderHash(source, entry_point);
    auto result = context_.fragment_shader_cache.find(hash);

    if (result != context_.fragment_shader_cache.end()) {
        return result->second;
    } else {
        VezShaderModuleCreateInfo shader_info = {};
        shader_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
        shader_info.codeSize = strlen(source);
        shader_info.pGLSLSource = source;
        shader_info.pEntryPoint = entry_point;

        VkShaderModule shader = VK_NULL_HANDLE;
        VK_CHECK(vezCreateShaderModule(device, &shader_info, &shader));
        context_.fragment_shader_cache[hash] = shader;
        return shader;
    }
}

result<VkBuffer> VezBackend::CreateBuffer(VezContext::BufferHash hash,
                                          VkDeviceSize size,
                                          VezMemoryFlagsBits storage,
                                          VkBufferUsageFlags usage,
                                          void* initial_contents) {
    assert(context_.device &&
           "Context must be initialized before creating a buffer");
    VkDevice device = context_.device;

    auto result = context_.buffer_cache.find(hash);
    if (result != context_.buffer_cache.end()) {
        vezDestroyBuffer(device, result->second);
    }

    VezBufferCreateInfo buffer_info = {};
    buffer_info.size = size;
    buffer_info.usage = usage;

    VkBuffer buffer = VK_NULL_HANDLE;
    VK_CHECK(vezCreateBuffer(device, storage, &buffer_info, &buffer));

    if (initial_contents) {
        if (storage != VEZ_MEMORY_GPU_ONLY) {
            void* buffer_memory;
            VK_CHECK(vezMapBuffer(device, buffer, 0, size, &buffer_memory));
            memcpy(buffer_memory, initial_contents, size);
            vezUnmapBuffer(device, buffer);

            VezMappedBufferRange range = {};
            range.buffer = buffer;
            range.offset = 0;
            range.size = size;
            VK_CHECK(vezFlushMappedBufferRanges(device, 1, &range));
        } else {
            // GPU-only buffer can't be mapped, so we use V-EZ's utility
            // function to submit data
            VK_CHECK(
                vezBufferSubData(device, buffer, 0, size, initial_contents));
        }
    }

    context_.buffer_cache[hash] = buffer;
    return buffer;
}

result<VkBuffer> VezBackend::GetBuffer(VezContext::BufferHash hash) {
    auto result = context_.buffer_cache.find(hash);
    if (result != context_.buffer_cache.end()) {
        return result->second;
    }

    return Error::NotFound;
}

result<VulkanImage> VezBackend::CreateImage(VezContext::ImageHash hash,

                                            VezImageCreateInfo image_info,
                                            void* initial_contents) {
    assert(context_.device &&
           "Context must be initialized before creating an image");
    VkDevice device = context_.device;

    auto result = context_.image_cache.find(hash);
    if (result != context_.image_cache.end()) {
        vezDestroyImageView(device, result->second.image_view);
        vezDestroyImage(device, result->second.image);
    }

    VkImage image = VK_NULL_HANDLE;
    VK_CHECK(vezCreateImage(device, VEZ_MEMORY_GPU_ONLY, &image_info, &image));

    VezImageSubresourceRange range = {};
    range.layerCount = image_info.arrayLayers;
    range.levelCount = image_info.mipLevels;

    VezImageViewCreateInfo image_view_info = {};
    image_view_info.components = {
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    image_view_info.format = image_info.format;
    image_view_info.image = image;
    image_view_info.subresourceRange = range;
    image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;

    VkImageView image_view;
    VK_CHECK(vezCreateImageView(device, &image_view_info, &image_view));

    if (initial_contents) {
        VezImageSubresourceLayers layers = {};
        layers.baseArrayLayer = 0;
        layers.layerCount = image_info.arrayLayers;
        layers.mipLevel = 0;

        VezImageSubDataInfo sub_data_info = {};
        sub_data_info.dataRowLength = image_info.extent.width;
        sub_data_info.dataImageHeight = image_info.extent.height;
        sub_data_info.imageExtent = image_info.extent;
        sub_data_info.imageOffset = {0, 0, 0};
        sub_data_info.imageSubresource = layers;

        vezImageSubData(device, image, &sub_data_info, initial_contents);
    }

    VulkanImage vulkan_image = {image, image_view};
    context_.image_cache[hash] = vulkan_image;
    return vulkan_image;
}

result<VezFramebuffer> VezBackend::CreateFramebuffer(VkExtent2D extent) {
    assert(context_.device &&
           "Context must be initialized before creating a framebuffer");
    VkDevice device = context_.device;

    // TODO create images as attachments
    std::vector<VkImageView> attachments;

    VezFramebufferCreateInfo fb_info = {};
    fb_info.width = extent.width;
    fb_info.height = extent.height;
    fb_info.layers = 1;
    fb_info.attachmentCount = static_cast<uint32_t>(attachments.size());
    fb_info.pAttachments = attachments.data();

    VezFramebuffer framebuffer = VK_NULL_HANDLE;
    vezCreateFramebuffer(device, &fb_info, &framebuffer);
    return framebuffer;
}

VezContext::ShaderHash VezBackend::GetShaderHash(const char* source,
                                                 const char* entry_point) {
    return {sdbm_hash(source), sdbm_hash(entry_point)};
}

VezContext::PipelineHash VezBackend::GetGraphicsPipelineHash(
    VkShaderModule vs, VkShaderModule fs) {
    return {vs, fs};
}

}  // namespace goma
