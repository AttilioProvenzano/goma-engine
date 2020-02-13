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

VKAPI_ATTR VkBool32 VKAPI_CALL
DebugMessenger(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
               VkDebugUtilsMessageTypeFlagsEXT messageTypes,
               const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
               void* pUserData) {
    switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            spdlog::error("{} - {}", pCallbackData->pMessageIdName,
                          pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            spdlog::warn("{} - {}", pCallbackData->pMessageIdName,
                         pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            spdlog::info("{} - {}", pCallbackData->pMessageIdName,
                         pCallbackData->pMessage);
            break;
        default:
            spdlog::debug("{} - {}", pCallbackData->pMessageIdName,
                          pCallbackData->pMessage);
            break;
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
        "VK_LAYER_KHRONOS_validation",
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
        VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME};

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

result<VkDebugUtilsMessengerEXT> CreateDebugMessenger(VkInstance instance) {
    VkDebugUtilsMessengerCreateInfoEXT debug_messenger_info{};
    debug_messenger_info.sType =
        VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_messenger_info.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    debug_messenger_info.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    debug_messenger_info.pfnUserCallback = &DebugMessenger;

    VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDebugUtilsMessengerEXT(instance, &debug_messenger_info,
                                            nullptr, &debug_messenger));
    return debug_messenger;
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

using ImagePtr = std::unique_ptr<Image>;
result<std::vector<ImagePtr>> GetSwapchainImages(const Platform& platform,
                                                 VkDevice device,
                                                 VkSwapchainKHR swapchain,
                                                 VkFormat fb_format) {
    uint32_t image_count;
    vkGetSwapchainImagesKHR(device, swapchain, &image_count, nullptr);

    std::vector<VkImage> images(image_count);
    vkGetSwapchainImagesKHR(device, swapchain, &image_count, images.data());

    std::vector<ImagePtr> ret;
    for (auto image : images) {
        // TODO: abstract out the CreateImageView helper (used in CreateImage
        // too)

        auto image_desc = ImageDesc::ColorAttachmentDesc;
        image_desc.size = {platform.GetWidth(), platform.GetHeight(), 1};
        image_desc.format = fb_format;
        // TODO: other parameters

        VkImageAspectFlags aspect =
            image_desc.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                ? VK_IMAGE_ASPECT_DEPTH_BIT
                : VK_IMAGE_ASPECT_COLOR_BIT;

        VkImageViewCreateInfo image_view_info = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        image_view_info.image = image;
        image_view_info.format = image_desc.format;
        image_view_info.viewType = image_desc.type;
        image_view_info.subresourceRange = {aspect, 0, VK_REMAINING_MIP_LEVELS,
                                            0, VK_REMAINING_ARRAY_LAYERS};

        VkImageView image_view = VK_NULL_HANDLE;
        VK_CHECK(
            vkCreateImageView(device, &image_view_info, nullptr, &image_view));

        auto image_ptr = std::make_unique<Image>(image_desc);
        image_ptr->SetHandle(image);
        image_ptr->SetView(image_view);

        ret.push_back(std::move(image_ptr));
    }

    return ret;
}

}  // namespace

const uint32_t Device::kInvalidSwapchainIndex = UINT32_MAX;

Device::Device(const Device::Config& config) : config_(config) {
    auto res = Init();
    if (res.has_error()) {
        throw std::runtime_error(res.error().message());
    }
}

Device::~Device() {
    vkDeviceWaitIdle(api_handles_.device);

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

    for (auto& image : images_) {
        if (image->GetHandle() != VK_NULL_HANDLE) {
            vkDestroyImageView(api_handles_.device, image->GetView(), nullptr);
            vmaDestroyImage(api_handles_.allocator, image->GetHandle(),
                            image->GetAllocation().allocation);
            image->SetView(VK_NULL_HANDLE);
            image->SetHandle(VK_NULL_HANDLE);
        }
    }
    images_.clear();

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

    for (auto& semaphore : acquisition_semaphores_) {
        vkDestroySemaphore(api_handles_.device, semaphore.second, nullptr);
    }
    acquisition_semaphores_.clear();

    for (auto& semaphore : presentation_semaphores_) {
        vkDestroySemaphore(api_handles_.device, semaphore.second, nullptr);
    }
    presentation_semaphores_.clear();

    for (auto& semaphore : recycled_semaphores_) {
        vkDestroySemaphore(api_handles_.device, semaphore, nullptr);
    }
    recycled_semaphores_.clear();

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

    if (api_handles_.debug_messenger) {
        vkDestroyDebugUtilsMessengerEXT(api_handles_.instance,
                                        api_handles_.debug_messenger, nullptr);
        api_handles_.debug_messenger = VK_NULL_HANDLE;
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

    OUTCOME_TRY(swapchain_images,
                GetSwapchainImages(platform, api_handles_.device,
                                   api_handles_.swapchain, fb_format));
    swapchain_images_ = std::move(swapchain_images);

    VkCommandPoolCreateInfo cmd_pool_info{
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cmd_pool_info.queueFamilyIndex = GetQueueFamilyIndex();

    VK_CHECK(vkCreateCommandPool(api_handles_.device, &cmd_pool_info, nullptr,
                                 &cmd_pool_));

    std::vector<VkCommandBuffer> cmd_bufs(2 * swapchain_images_.size());

    VkCommandBufferAllocateInfo cmd_buf_info{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cmd_buf_info.commandPool = cmd_pool_;
    cmd_buf_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_buf_info.commandBufferCount = static_cast<uint32_t>(cmd_bufs.size());

    VK_CHECK(vkAllocateCommandBuffers(api_handles_.device, &cmd_buf_info,
                                      cmd_bufs.data()));

    for (size_t i = 0; i < swapchain_images_.size(); i++) {
        auto& image = swapchain_images_[i];

        {
            auto& acquisition_cmd_buf = cmd_bufs[2 * i];

            VkCommandBufferBeginInfo begin_info{
                VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            VK_CHECK(vkBeginCommandBuffer(acquisition_cmd_buf, &begin_info));

            VkImageMemoryBarrier image_barrier = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            image_barrier.srcAccessMask = 0;
            image_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            image_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            image_barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            image_barrier.image = image->GetHandle();
            image_barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1,
                                              0, 1};

            vkCmdPipelineBarrier(acquisition_cmd_buf,
                                 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0,
                                 nullptr, 1, &image_barrier);

            vkEndCommandBuffer(acquisition_cmd_buf);
            acquisition_cmd_bufs_[i] = acquisition_cmd_buf;
        }

        {
            auto& presentation_cmd_buf = cmd_bufs[2 * i + 1];

            VkCommandBufferBeginInfo begin_info{
                VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            VK_CHECK(vkBeginCommandBuffer(presentation_cmd_buf, &begin_info));

            VkImageMemoryBarrier image_barrier = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            image_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            image_barrier.dstAccessMask = 0;
            image_barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            image_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            image_barrier.image = image->GetHandle();
            image_barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1,
                                              0, 1};

            vkCmdPipelineBarrier(presentation_cmd_buf,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                 VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0,
                                 nullptr, 1, &image_barrier);

            vkEndCommandBuffer(presentation_cmd_buf);
            presentation_cmd_bufs_[i] = presentation_cmd_buf;
        }
    }

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

    OUTCOME_TRY(debug_messenger, CreateDebugMessenger(instance));
    api_handles_.debug_messenger = debug_messenger;

    OUTCOME_TRY(physical_device, CreatePhysicalDevice(instance));
    api_handles_.physical_device = physical_device;

    OUTCOME_TRY(queue_family_index, GetQueueFamilyIndex_(physical_device));
    queue_family_index_ = queue_family_index;

    OUTCOME_TRY(device, CreateDevice(physical_device, queue_family_index));
    api_handles_.device = device;

    vkGetDeviceQueue(device, queue_family_index_, 0, &api_handles_.queue);

    VkDebugUtilsObjectNameInfoEXT name_info = {
        VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    name_info.objectType = VK_OBJECT_TYPE_QUEUE;
    name_info.objectHandle = reinterpret_cast<uint64_t>(api_handles_.queue);
    name_info.pObjectName = "Graphics Queue";
    VK_CHECK(vkSetDebugUtilsObjectNameEXT(api_handles_.device, &name_info));

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

result<Image*> Device::AcquireSwapchainImage() {
    auto semaphore = GetSemaphore();

    VK_CHECK(vkAcquireNextImageKHR(api_handles_.device, api_handles_.swapchain,
                                   UINT64_MAX, semaphore, VK_NULL_HANDLE,
                                   &swapchain_index_));

    std::stringstream str;
    str << "Acquisition Semaphore #" << swapchain_index_;
    std::string name = str.str();

    VkDebugUtilsObjectNameInfoEXT name_info = {
        VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    name_info.objectType = VK_OBJECT_TYPE_SEMAPHORE;
    name_info.objectHandle = reinterpret_cast<uint64_t>(semaphore);
    name_info.pObjectName = name.c_str();
    VK_CHECK(vkSetDebugUtilsObjectNameEXT(api_handles_.device, &name_info));

    // Recycle the old acquisition semaphore, if present
    auto old_semaphore = acquisition_semaphores_.find(swapchain_index_);
    if (old_semaphore != acquisition_semaphores_.end()) {
        recycled_semaphores_.push_back(old_semaphore->second);
    }

    VkPipelineStageFlags stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &semaphore;
    submit.pWaitDstStageMask = &stage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &acquisition_cmd_bufs_[swapchain_index_];
    VK_CHECK(vkQueueSubmit(api_handles_.queue, 1, &submit, VK_NULL_HANDLE));

    acquisition_semaphores_[swapchain_index_] = semaphore;
    return swapchain_images_.at(swapchain_index_).get();
}

result<Image*> Device::CreateImage(const ImageDesc& image_desc) {
    static const std::unordered_map<VkImageViewType, VkImageType>
        image_types_map = {
            {VK_IMAGE_VIEW_TYPE_1D, VK_IMAGE_TYPE_1D},
            {VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_TYPE_2D},
            {VK_IMAGE_VIEW_TYPE_3D, VK_IMAGE_TYPE_3D},
            {VK_IMAGE_VIEW_TYPE_CUBE, VK_IMAGE_TYPE_2D},
            {VK_IMAGE_VIEW_TYPE_1D_ARRAY, VK_IMAGE_TYPE_1D},
            {VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_TYPE_2D},
            {VK_IMAGE_VIEW_TYPE_CUBE_ARRAY, VK_IMAGE_TYPE_2D},
        };

    VkImageCreateInfo image_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    image_info.extent = image_desc.size;
    image_info.format = image_desc.format;
    image_info.mipLevels = image_desc.mip_levels;
    image_info.arrayLayers = image_desc.array_layers;
    image_info.samples = image_desc.samples;
    image_info.tiling = image_desc.tiling;
    image_info.usage = image_desc.usage;
    image_info.imageType = image_types_map.at(image_desc.type);

    VmaAllocationCreateInfo allocation_create_info = {};
    allocation_create_info.usage = image_desc.storage;

    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo allocation_info = {};

    VK_CHECK(vmaCreateImage(api_handles_.allocator, &image_info,
                            &allocation_create_info, &image, &allocation,
                            &allocation_info));

    VkImageAspectFlags aspect =
        image_desc.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
            ? VK_IMAGE_ASPECT_DEPTH_BIT
            : VK_IMAGE_ASPECT_COLOR_BIT;

    VkImageViewCreateInfo image_view_info = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    image_view_info.image = image;
    image_view_info.format = image_desc.format;
    image_view_info.viewType = image_desc.type;
    image_view_info.subresourceRange = {aspect, 0, VK_REMAINING_MIP_LEVELS, 0,
                                        VK_REMAINING_ARRAY_LAYERS};

    VkImageView image_view = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImageView(api_handles_.device, &image_view_info, nullptr,
                               &image_view));

    auto image_ptr = std::make_unique<Image>(image_desc);
    image_ptr->SetHandle(image);
    image_ptr->SetAllocation({allocation, allocation_info});
    image_ptr->SetView(image_view);

    images_.push_back(std::move(image_ptr));
    return images_.back().get();
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

    VkDebugUtilsObjectNameInfoEXT name_info = {
        VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    name_info.objectType = VK_OBJECT_TYPE_PIPELINE;
    name_info.objectHandle = reinterpret_cast<uint64_t>(pipeline);
    name_info.pObjectName = "HelloTriangle";
    VK_CHECK(vkSetDebugUtilsObjectNameEXT(api_handles_.device, &name_info));

    auto pipeline_ptr = std::make_unique<Pipeline>(std::move(pipeline_desc));
    pipeline_ptr->SetHandle(pipeline);

    pipelines_.push_back(std::move(pipeline_ptr));
    return pipelines_.back().get();
}

VkSemaphore Device::GetSemaphore() {
    VkSemaphore ret = VK_NULL_HANDLE;

    if (!recycled_semaphores_.empty()) {
        ret = recycled_semaphores_.back();
        recycled_semaphores_.pop_back();
    } else {
        VkSemaphoreCreateInfo semaphore_info = {
            VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        vkCreateSemaphore(api_handles_.device, &semaphore_info, nullptr, &ret);
    }

    return ret;
}

VkFence Device::GetFence() {
    VkFence ret = VK_NULL_HANDLE;

    if (!recycled_fences_.empty()) {
        ret = recycled_fences_.back();
        recycled_fences_.pop_back();
    } else {
        VkFenceCreateInfo fence_info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        vkCreateFence(api_handles_.device, &fence_info, nullptr, &ret);
    }

    return ret;
}

result<ReceiptPtr> Device::Submit(Context& context) {
    auto cmd_bufs = context.PopQueuedCommands();

    VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = static_cast<uint32_t>(cmd_bufs.size());
    submit.pCommandBuffers = cmd_bufs.data();

    auto fence = GetFence();
    VK_CHECK(vkQueueSubmit(api_handles_.queue, 1, &submit, fence));

    auto submission_id = ++last_submission_id_;
    submission_fences_[submission_id] = fence;

    return std::make_unique<Receipt>(
        Receipt{submission_id, api_handles_.device});
}

result<void> Device::WaitOnWork(ReceiptPtr&& receipt) {
    if (receipt->device && receipt->device == api_handles_.device) {
        auto fence = submission_fences_[receipt->submission_id];
        if (fence) {
            VK_CHECK(vkWaitForFences(api_handles_.device, 1, &fence, VK_TRUE,
                                     UINT64_MAX));
            recycled_fences_.push_back(fence);
        }

        submission_fences_.erase(receipt->submission_id);
    }

    return outcome::success();
}

result<void> Device::Present() {
    assert(swapchain_index_ != kInvalidSwapchainIndex &&
           "An image must be acquired before calling Present()");

    if (presentation_semaphores_[swapchain_index_] == VK_NULL_HANDLE) {
        auto semaphore = GetSemaphore();

        std::stringstream str;
        str << "Presentation Semaphore #" << swapchain_index_;
        std::string name = str.str();

        VkDebugUtilsObjectNameInfoEXT name_info = {
            VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        name_info.objectType = VK_OBJECT_TYPE_SEMAPHORE;
        name_info.objectHandle = reinterpret_cast<uint64_t>(semaphore);
        name_info.pObjectName = name.c_str();
        VK_CHECK(vkSetDebugUtilsObjectNameEXT(api_handles_.device, &name_info));

        presentation_semaphores_[swapchain_index_] = semaphore;
    }

    VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &presentation_cmd_bufs_[swapchain_index_];
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &presentation_semaphores_[swapchain_index_];
    VK_CHECK(vkQueueSubmit(api_handles_.queue, 1, &submit, VK_NULL_HANDLE));

    VkPresentInfoKHR present_info = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &api_handles_.swapchain;
    present_info.pImageIndices = &swapchain_index_;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &presentation_semaphores_[swapchain_index_];

    VkResult result;
    present_info.pResults = &result;

    VK_CHECK(vkQueuePresentKHR(api_handles_.queue, &present_info));
    VK_CHECK(result);

    swapchain_index_ = kInvalidSwapchainIndex;
    return outcome::success();
}

}  // namespace goma
