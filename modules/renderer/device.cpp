#include "renderer/vulkan/device.hpp"

#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>
#include <spirv_glsl.hpp>

#include "common/error_codes.hpp"
#include "platform/platform.hpp"
#include "renderer/vulkan/context.hpp"
#include "renderer/vulkan/utils.hpp"

namespace goma {

namespace {

const char* kPipelineCacheFilename = "pipeline_cache.data";

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

result<uint32_t> GetQueueFamilyIndex_(VkPhysicalDevice physical_device) {
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

    return static_cast<uint32_t>(selected_queue_family -
                                 queue_families.begin());
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
    queue_info.queueCount = static_cast<uint32_t>(queue_priorities.size());
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

result<VkPipelineCache> CreatePipelineCache(VkDevice device,
                                            const char* filename = nullptr) {
    std::vector<char> data;

    if (filename) {
        std::ifstream f(filename, std::ios_base::in | std::ios_base::binary);

        if (f.good()) {
            f.seekg(0, std::ios::end);
            auto size = static_cast<size_t>(f.tellg());

            data.resize(size);
            f.seekg(0);
            f.read(data.data(), size);
        }
    }

    VkPipelineCacheCreateInfo pipeline_cache_info = {
        VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};

    if (!data.empty()) {
        pipeline_cache_info.initialDataSize = data.size();
        pipeline_cache_info.pInitialData = data.data();
    }

    VkPipelineCache pipeline_cache = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineCache(device, &pipeline_cache_info, nullptr,
                                   &pipeline_cache));

    return pipeline_cache;
}

result<VmaAllocator> CreateAllocator(VkPhysicalDevice physical_device,
                                     VkDevice device) {
    VmaVulkanFunctions vk_functions = {vkGetPhysicalDeviceProperties,
                                       vkGetPhysicalDeviceMemoryProperties,
                                       vkAllocateMemory,
                                       vkFreeMemory,
                                       vkMapMemory,
                                       vkUnmapMemory,
                                       vkFlushMappedMemoryRanges,
                                       vkInvalidateMappedMemoryRanges,
                                       vkBindBufferMemory,
                                       vkBindImageMemory,
                                       vkGetBufferMemoryRequirements,
                                       vkGetImageMemoryRequirements,
                                       vkCreateBuffer,
                                       vkDestroyBuffer,
                                       vkCreateImage,
                                       vkDestroyImage,
                                       vkCmdCopyBuffer};

    VmaAllocatorCreateInfo allocator_info = {};
    allocator_info.physicalDevice = physical_device;
    allocator_info.device = device;
    allocator_info.frameInUseCount = 3;
    allocator_info.pVulkanFunctions = &vk_functions;

    VmaAllocator allocator = VK_NULL_HANDLE;
    VK_CHECK(vmaCreateAllocator(&allocator_info, &allocator));

    return allocator;
}

}  // namespace

Device::Device(const Device::Config& config) : config_(config) {
    auto res = Init();
    if (res.has_error()) {
        throw std::runtime_error(res.error().message());
    }
}

Device::~Device() {
    vkDeviceWaitIdle(api_handles_.device);

    /*
for (auto image : api_handles_.fb_image_cache) {
    vezDestroyImageView(api_handles_.device, image.second->vez.image_view);
    vezDestroyImage(api_handles_.device, image.second->vez.image);
}
    */

    for (auto& shader : shaders_) {
        vkDestroyShaderModule(api_handles_.device, shader->GetHandle(),
                              nullptr);
        shader->SetHandle(VK_NULL_HANDLE);
    }
    shaders_.clear();

    for (auto& buffer : buffers_) {
        if (buffer->GetHandle() != VK_NULL_HANDLE) {
            vmaDestroyBuffer(api_handles_.allocator, buffer->GetHandle(),
                             buffer->GetAllocation().allocation);
            buffer->SetHandle(VK_NULL_HANDLE);
        }
    }
    buffers_.clear();

    if (api_handles_.pipeline_cache) {
        size_t data_size;
        vkGetPipelineCacheData(api_handles_.device, api_handles_.pipeline_cache,
                               &data_size, nullptr);

        std::vector<char> data(data_size);
        vkGetPipelineCacheData(api_handles_.device, api_handles_.pipeline_cache,
                               &data_size, static_cast<void*>(data.data()));

        std::ofstream f(kPipelineCacheFilename,
                        std::ios_base::out | std::ios_base::binary);
        if (f.good()) {
            f.write(data.data(), data.size());
        }

        vkDestroyPipelineCache(api_handles_.device, api_handles_.pipeline_cache,
                               nullptr);
        api_handles_.pipeline_cache = VK_NULL_HANDLE;
    }

    for (auto& pipeline : pipelines_) {
        vkDestroyPipeline(api_handles_.device, pipeline->GetHandle(), nullptr);
        pipeline->SetHandle(VK_NULL_HANDLE);
    }
    pipelines_.clear();

    for (auto& pipeline_layout : api_handles_.pipeline_layouts) {
        vkDestroyPipelineLayout(api_handles_.device, pipeline_layout, nullptr);
    }
    api_handles_.pipeline_layouts.clear();

    for (auto& render_pass : api_handles_.render_passes) {
        vkDestroyRenderPass(api_handles_.device, render_pass, nullptr);
    }
    api_handles_.render_passes.clear();

    if (api_handles_.swapchain) {
        vkDestroySwapchainKHR(api_handles_.device, api_handles_.swapchain,
                              nullptr);
        api_handles_.swapchain = VK_NULL_HANDLE;
    }

    if (api_handles_.surface) {
        vkDestroySurfaceKHR(api_handles_.instance, api_handles_.surface,
                            nullptr);
        api_handles_.surface = VK_NULL_HANDLE;
    }

    if (api_handles_.allocator) {
        vmaDestroyAllocator(api_handles_.allocator);
        api_handles_.allocator = VK_NULL_HANDLE;
    }

    if (api_handles_.device) {
        vkDestroyDevice(api_handles_.device, nullptr);
        api_handles_.device = VK_NULL_HANDLE;
    }

    if (api_handles_.debug_callback) {
        vkDestroyDebugReportCallbackEXT(api_handles_.instance,
                                        api_handles_.debug_callback, nullptr);
        api_handles_.debug_callback = VK_NULL_HANDLE;
    }

    if (api_handles_.instance) {
        vkDestroyInstance(api_handles_.instance, nullptr);
        api_handles_.instance = VK_NULL_HANDLE;
    }
}

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

VkDevice Device::GetHandle() {
    assert(api_handles_.device != VK_NULL_HANDLE && "Device not initialized.");
    return api_handles_.device;
}

uint32_t Device::GetQueueFamilyIndex() {
    assert(queue_family_index_ != -1 && "Queue family index not initialized.");
    return queue_family_index_;
}

result<void> Device::Init() {
    VK_CHECK(volkInitialize());

    OUTCOME_TRY(instance, CreateInstance());
    api_handles_.instance = instance;

    OUTCOME_TRY(debug_callback, CreateDebugCallback(instance));
    api_handles_.debug_callback = debug_callback;

    OUTCOME_TRY(physical_device, CreatePhysicalDevice(instance));
    api_handles_.physical_device = physical_device;

    OUTCOME_TRY(queue_family_index, GetQueueFamilyIndex_(physical_device));
    queue_family_index_ = queue_family_index;

    OUTCOME_TRY(device, CreateDevice(physical_device, queue_family_index));
    api_handles_.device = device;

    OUTCOME_TRY(pipeline_cache,
                CreatePipelineCache(device, kPipelineCacheFilename));
    api_handles_.pipeline_cache = pipeline_cache;

    OUTCOME_TRY(allocator, CreateAllocator(physical_device, device));
    api_handles_.allocator = allocator;

    return outcome::success();
}

result<Buffer*> Device::CreateBuffer(const BufferDesc& buffer_desc) {
    VkBufferCreateInfo buffer_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_info.size = buffer_desc.size;
    buffer_info.usage = buffer_desc.usage;

    if (buffer_desc.storage == VMA_MEMORY_USAGE_GPU_ONLY) {
        buffer_info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }

    VmaAllocationCreateInfo allocation_create_info = {};
    allocation_create_info.usage = buffer_desc.storage;

    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo allocation_info = {};

    VK_CHECK(vmaCreateBuffer(api_handles_.allocator, &buffer_info,
                             &allocation_create_info, &buffer, &allocation,
                             &allocation_info));

    auto buffer_ptr = std::make_unique<Buffer>(buffer_desc);
    buffer_ptr->SetHandle(buffer);
    buffer_ptr->SetAllocation({allocation, allocation_info});

    buffers_.push_back(std::move(buffer_ptr));
    return buffers_.back().get();
}

result<void*> Device::MapBuffer(Buffer& buffer) {
    void* data = nullptr;
    VK_CHECK(vmaMapMemory(api_handles_.allocator,
                          buffer.GetAllocation().allocation, &data));

    return data;
}

void Device::UnmapBuffer(Buffer& buffer) {
    vmaUnmapMemory(api_handles_.allocator, buffer.GetAllocation().allocation);
}

result<Shader*> Device::CreateShader(ShaderDesc shader_desc) {
    static const std::unordered_map<VkShaderStageFlagBits, EShLanguage>
        stage_map = {{VK_SHADER_STAGE_VERTEX_BIT, EShLangVertex},
                     {VK_SHADER_STAGE_FRAGMENT_BIT, EShLangFragment}};

    glslang::InitializeProcess();
    glslang::TShader shader{stage_map.at(shader_desc.stage)};

    auto source = shader_desc.source.c_str();
    shader.setStrings(&source, 1);

    if (!shader_desc.preamble.empty()) {
        shader.setPreamble(shader_desc.preamble.c_str());
    }

    shader.setEnvInput(glslang::EShSourceGlsl, EShLangVertex,
                       glslang::EShClientVulkan, 100);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_1);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);

    TBuiltInResource builtin_resources = {};
    bool compilation_res =
        shader.parse(&builtin_resources, 0, false, EShMsgDefault);

    if (!compilation_res) {
        auto info_log = shader.getInfoLog();
        spdlog::info("Shader \"{}\" - compilation failed:\n{}",
                     shader_desc.name, info_log);

        return Error::ShaderCompilationFailed;
    }

    glslang::TProgram program;
    program.addShader(&shader);
    bool link_res = program.link(EShMsgDefault);

    if (!link_res) {
        auto info_log = program.getInfoLog();
        spdlog::info("Shader \"{}\" - linking failed:\n{}", shader_desc.name,
                     info_log);
    }

    std::vector<uint32_t> spirv;
    spv::SpvBuildLogger logger;
    glslang::SpvOptions spvOptions = {};
    GlslangToSpv(*program.getIntermediate(EShLangVertex), spirv, &logger,
                 &spvOptions);

    glslang::FinalizeProcess();

    VkShaderModuleCreateInfo shader_info = {
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    shader_info.codeSize = sizeof(spirv[0]) * spirv.size();
    shader_info.pCode = spirv.data();

    VkShaderModule shader_module = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(api_handles_.device, &shader_info, nullptr,
                                  &shader_module));

    auto shader_ptr = std::make_unique<Shader>(std::move(shader_desc));
    shader_ptr->SetHandle(shader_module);

    // Shader reflection
    spirv_cross::CompilerGLSL spirv_glsl{std::move(spirv)};
    auto resources = spirv_glsl.get_shader_resources();

    ShaderInputs inputs;
    for (const auto& stage_input : resources.stage_inputs) {
        ShaderInput input = {};
        input.name = std::move(stage_input.name);
        input.location =
            spirv_glsl.get_decoration(stage_input.id, spv::DecorationLocation);
        input.vecsize = spirv_glsl.get_type(stage_input.type_id).vecsize;

        inputs.push_back(input);
    };

    shader_ptr->SetInputs(std::move(inputs));

    shaders_.push_back(std::move(shader_ptr));
    return shaders_.back().get();
}

result<Pipeline*> Device::CreatePipeline(PipelineDesc pipeline_desc,
                                         FramebufferDesc& fb_desc) {
    VkGraphicsPipelineCreateInfo pipeline_info = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};

    if (fb_desc.render_pass_ == VK_NULL_HANDLE) {
        OUTCOME_TRY(render_pass,
                    utils::CreateRenderPass(api_handles_.device, fb_desc));
        fb_desc.render_pass_ = render_pass;
        api_handles_.render_passes.push_back(render_pass);
    }

    VkPipelineLayoutCreateInfo layout_info = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layout_info.setLayoutCount = 0;  // TODO!

    // TODO: move pipeline layout to shader (SPIRV-Cross?)
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(api_handles_.device, &layout_info, nullptr,
                                    &pipeline_layout));
    api_handles_.pipeline_layouts.push_back(pipeline_layout);

    std::vector<VkPipelineShaderStageCreateInfo> stages;

    for (const auto& shader : pipeline_desc.shaders) {
        VkPipelineShaderStageCreateInfo stage = {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        stage.stage = shader->GetStage();
        stage.module = shader->GetHandle();
        stage.pName = "main";  // TODO: Store in shader

        stages.push_back(stage);
    }

    VkPipelineVertexInputStateCreateInfo vtx_input_state = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

    static const std::vector<VkFormat> formats = {
        VK_FORMAT_UNDEFINED, VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT,
        VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT};

    uint32_t current_offset = 0;
    std::vector<VkVertexInputAttributeDescription> attributes;

    for (const auto& input : pipeline_desc.shaders[0]->GetInputs()) {
        VkVertexInputAttributeDescription desc = {};

        auto format_index = input.vecsize;

        if (format_index >= formats.size()) {
            spdlog::warn(
                "Shader resource \"{}\" has vector size {}, larger than the "
                "maximum supported ({}).",
                input.name, input.vecsize, formats.size() - 1);
            format_index = static_cast<uint32_t>(formats.size()) - 1;
        }

        desc.location = input.location;
        desc.format = formats[format_index];
        desc.binding = 0;
        desc.offset = current_offset;

        attributes.push_back(desc);
        current_offset += sizeof(float) * input.vecsize;
    }

    VkVertexInputBindingDescription binding = {};
    binding.binding = 0;
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    binding.stride = current_offset;

    vtx_input_state.vertexBindingDescriptionCount = 1;
    vtx_input_state.pVertexBindingDescriptions = &binding;
    vtx_input_state.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(attributes.size());
    vtx_input_state.pVertexAttributeDescriptions = attributes.data();

    VkPipelineViewportStateCreateInfo viewport_state = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};

    VkViewport viewport = {};
    VkRect2D scissor = {};

    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    VkPipelineInputAssemblyStateCreateInfo input_assembly_state = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    input_assembly_state.topology = pipeline_desc.primitive_topology;
    input_assembly_state.primitiveRestartEnable = VK_TRUE;

    VkPipelineRasterizationStateCreateInfo raster_state = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    raster_state.depthClampEnable = VK_FALSE;
    raster_state.rasterizerDiscardEnable = pipeline_desc.suppress_fragment;
    raster_state.polygonMode = VK_POLYGON_MODE_FILL;
    raster_state.cullMode = pipeline_desc.cull_mode;
    raster_state.frontFace = pipeline_desc.front_face;
    raster_state.depthBiasEnable = VK_FALSE;
    raster_state.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample_state = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample_state.rasterizationSamples = pipeline_desc.sample_count;
    multisample_state.sampleShadingEnable =
        pipeline_desc.sample_count > VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds_state = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds_state.depthTestEnable = pipeline_desc.depth_test;
    ds_state.depthWriteEnable = pipeline_desc.depth_write;
    ds_state.depthCompareOp = pipeline_desc.depth_compare_op;
    ds_state.stencilTestEnable = pipeline_desc.stencil_test;
    ds_state.front = pipeline_desc.stencil_front_op;
    ds_state.back = pipeline_desc.stencil_back_op;

    VkPipelineColorBlendStateCreateInfo blend_state = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};

    std::vector<VkPipelineColorBlendAttachmentState> blend_attachments;

    if (pipeline_desc.color_blend) {
        blend_state.attachmentCount =
            static_cast<uint32_t>(pipeline_desc.blend_attachments.size());
        blend_state.pAttachments = pipeline_desc.blend_attachments.data();
    } else {
        blend_attachments.resize(fb_desc.color_attachments.size(),
                                 VkPipelineColorBlendAttachmentState{VK_FALSE});
        blend_state.attachmentCount =
            static_cast<uint32_t>(blend_attachments.size());
        blend_state.pAttachments = blend_attachments.data();
    }

    static const std::vector<VkDynamicState> dyn_states = {
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
        VK_DYNAMIC_STATE_STENCIL_REFERENCE,
        VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
        VK_DYNAMIC_STATE_BLEND_CONSTANTS};

    VkPipelineDynamicStateCreateInfo dyn_state = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dyn_state.dynamicStateCount = static_cast<uint32_t>(dyn_states.size());
    dyn_state.pDynamicStates = dyn_states.data();

    pipeline_info.stageCount = static_cast<uint32_t>(stages.size());
    pipeline_info.pStages = stages.data();
    pipeline_info.pVertexInputState = &vtx_input_state;
    pipeline_info.pInputAssemblyState = &input_assembly_state;
    pipeline_info.pTessellationState = nullptr;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &raster_state;
    pipeline_info.pMultisampleState = &multisample_state;
    pipeline_info.pDepthStencilState = &ds_state;
    pipeline_info.pColorBlendState = &blend_state;
    pipeline_info.pDynamicState = &dyn_state;
    pipeline_info.layout = pipeline_layout;
    pipeline_info.renderPass = fb_desc.render_pass_;
    pipeline_info.subpass = 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    vkCreateGraphicsPipelines(api_handles_.device, api_handles_.pipeline_cache,
                              1, &pipeline_info, nullptr, &pipeline);

    auto pipeline_ptr = std::make_unique<Pipeline>(std::move(pipeline_desc));
    pipeline_ptr->SetHandle(pipeline);

    pipelines_.push_back(std::move(pipeline_ptr));
    return pipelines_.back().get();
}

}  // namespace goma
