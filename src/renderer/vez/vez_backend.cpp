#include "renderer/vez/vez_backend.hpp"

#include "engine.hpp"

#include "Core/ShaderModule.h"  // from V-EZ

#include <array>

#define VK_CHECK(fn)                                      \
    {                                                     \
        VkResult _r = fn;                                 \
        if (_r != VK_SUCCESS) {                           \
            LOGE("Vulkan error when running %s", #fn);    \
        };                                                \
        switch (_r) {                                     \
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

        VezGraphicsPipelineCreateInfo pipeline_info = {};
        pipeline_info.stageCount = static_cast<uint32_t>(shader_stages.size());
        pipeline_info.pStages = shader_stages.data();

        VezPipeline pipeline = VK_NULL_HANDLE;
        VK_CHECK(vezCreateGraphicsPipeline(device, &pipeline_info, &pipeline));
        context_.pipeline_cache[hash] = pipeline;
        return {pipeline};
    }
}

result<VertexInputFormat> VezBackend::GetVertexInputFormat(
    const VertexInputFormatDesc& desc) {
    assert(context_.device && "Context must be initialized");
    VkDevice device = context_.device;

    auto hash = GetVertexInputFormatHash(desc);

    auto result = context_.vertex_input_format_cache.find(hash);
    if (result != context_.vertex_input_format_cache.end()) {
        return {result->second};
    }

    std::vector<VkVertexInputBindingDescription> bindings;
    for (const auto& b : desc.bindings) {
        bindings.push_back({b.binding, b.stride,
                            b.per_instance ? VK_VERTEX_INPUT_RATE_INSTANCE
                                           : VK_VERTEX_INPUT_RATE_VERTEX});
    }

    std::vector<VkVertexInputAttributeDescription> attributes;
    for (const auto& a : desc.attributes) {
        attributes.push_back(
            {a.location, a.binding, GetVkFormat(a.format), a.offset});
    }

    VezVertexInputFormatCreateInfo input_info = {};
    input_info.vertexBindingDescriptionCount =
        static_cast<uint32_t>(bindings.size());
    input_info.pVertexBindingDescriptions = bindings.data();
    input_info.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(attributes.size());
    input_info.pVertexAttributeDescriptions = attributes.data();

    VezVertexInputFormat input_format;
    vezCreateVertexInputFormat(device, &input_info, &input_format);
    context_.vertex_input_format_cache[hash] = input_format;
    return {input_format};
};

result<Image> VezBackend::CreateTexture(const char* name,
                                        TextureDesc texture_desc,
                                        void* initial_contents) {
    auto hash = GetTextureHash(name);
    auto result = context_.texture_cache.find(hash);
    if (result != context_.texture_cache.end()) {
        vezDestroyImageView(context_.device, result->second.image_view);
        vezDestroyImage(context_.device, result->second.image);
    }

    VkFormat format = GetVkFormat(texture_desc.format);

    VezImageCreateInfo image_info = {};
    image_info.extent = {texture_desc.width, texture_desc.height, 1};
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.arrayLayers = texture_desc.array_layers;
    image_info.mipLevels = texture_desc.mip_levels;
    image_info.format = format;
    image_info.samples =
        static_cast<VkSampleCountFlagBits>(texture_desc.samples);
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage =
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    if (texture_desc.mip_levels > 1) {
        // We need TRANSFER_SRC to generate mipmaps
        image_info.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    }

    OUTCOME_TRY(vulkan_image, CreateImage(hash, image_info, initial_contents));
    OUTCOME_TRY(sampler, GetSampler(texture_desc.sampler));
    vulkan_image.sampler = sampler;

    context_.texture_cache[hash] = vulkan_image;
    return {vulkan_image};
}

result<Image> VezBackend::GetTexture(const char* name) {
    auto hash = GetTextureHash(name);
    auto result = context_.texture_cache.find(hash);
    if (result != context_.texture_cache.end()) {
        return {result->second};
    }

    return Error::NotFound;
}

result<Framebuffer> VezBackend::CreateFramebuffer(size_t frame_index,
                                                  const char* name,
                                                  FramebufferDesc fb_desc) {
    assert(context_.device &&
           "Context must be initialized before creating a framebuffer");
    VkDevice device = context_.device;

    auto hash = GetFramebufferHash(frame_index, name);
    auto result = context_.framebuffer_cache.find(hash);
    if (result != context_.framebuffer_cache.end()) {
        vezDestroyFramebuffer(context_.device, result->second);
    }

    std::vector<VkImageView> attachments;
    for (auto& image_desc : fb_desc.color_images) {
        OUTCOME_TRY(image,
                    CreateFramebufferImage(frame_index, image_desc,
                                           fb_desc.width, fb_desc.height));
        attachments.push_back(image.image_view);
    }

    if (fb_desc.depth_image.depth_type != DepthImageType::None) {
        OUTCOME_TRY(image,
                    CreateFramebufferImage(frame_index, fb_desc.depth_image,
                                           fb_desc.width, fb_desc.height));
        attachments.push_back(image.image_view);
    }

    VezFramebufferCreateInfo fb_info = {};
    fb_info.width = fb_desc.width;
    fb_info.height = fb_desc.height;
    fb_info.layers = 1;
    fb_info.attachmentCount = static_cast<uint32_t>(attachments.size());
    fb_info.pAttachments = attachments.data();

    VezFramebuffer framebuffer = VK_NULL_HANDLE;
    VK_CHECK(vezCreateFramebuffer(device, &fb_info, &framebuffer));
    context_.framebuffer_cache[hash] = framebuffer;
    return {framebuffer};
}

result<Buffer> VezBackend::CreateVertexBuffer(const char* name, uint64_t size,
                                              bool gpu_stored,
                                              void* initial_contents) {
    auto hash = GetBufferHash(name);
    OUTCOME_TRY(buffer, CreateBuffer(hash, static_cast<VkDeviceSize>(size),
                                     gpu_stored ? VEZ_MEMORY_GPU_ONLY
                                                : VEZ_MEMORY_CPU_TO_GPU,
                                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                         VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                     initial_contents));
    return {buffer};
}

result<Buffer> VezBackend::GetVertexBuffer(const char* name) {
    auto hash = GetBufferHash(name);
    auto result = context_.buffer_cache.find(hash);
    if (result != context_.buffer_cache.end()) {
        return {result->second};
    }

    return Error::NotFound;
}

result<Buffer> VezBackend::CreateIndexBuffer(const char* name, uint64_t size,
                                             bool gpu_stored,
                                             void* initial_contents) {
    auto hash = GetBufferHash(name);
    OUTCOME_TRY(buffer, CreateBuffer(hash, static_cast<VkDeviceSize>(size),
                                     gpu_stored ? VEZ_MEMORY_GPU_ONLY
                                                : VEZ_MEMORY_CPU_TO_GPU,
                                     VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                         VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                     initial_contents));
    return {buffer};
}

result<Buffer> VezBackend::GetIndexBuffer(const char* name) {
    auto hash = GetBufferHash(name);
    auto result = context_.buffer_cache.find(hash);
    if (result != context_.buffer_cache.end()) {
        return {result->second};
    }

    return Error::NotFound;
}

result<void> VezBackend::SetupFrames(uint32_t frames) {
    if (frames > context_.per_frame.size()) {
        context_.per_frame.resize(frames);
    }
    return outcome::success();
}

result<size_t> VezBackend::StartFrame(uint32_t threads) {
    assert(context_.device &&
           "Context must be initialized before starting a frame");
    VkDevice device = context_.device;

    if (context_.per_frame.empty()) {
        // Set up triple buffering by default
        SetupFrames(3);
    }

    if (threads == 0) {
        threads = 1;
    }

    auto& per_frame = context_.per_frame[context_.current_frame];
    auto command_buffer_count = per_frame.command_buffers.size();
    if (threads > command_buffer_count) {
        VkQueue graphics_queue = VK_NULL_HANDLE;
        vezGetDeviceGraphicsQueue(device, 0, &graphics_queue);

        VezCommandBufferAllocateInfo cmd_info = {};
        cmd_info.commandBufferCount =
            static_cast<uint32_t>(threads - command_buffer_count);
        cmd_info.queue = graphics_queue;

        per_frame.command_buffers.resize(threads);
        VK_CHECK(vezAllocateCommandBuffers(
            context_.device, &cmd_info,
            &per_frame.command_buffers[command_buffer_count]));
    }

    per_frame.command_buffer_active.resize(threads);
    for (auto& active : per_frame.command_buffer_active) {
        active = false;
    }

    return context_.current_frame;
}

result<void> VezBackend::StartRenderPass(Framebuffer fb,
                                         RenderPassDesc rp_desc) {
    assert(context_.device &&
           "Context must be initialized before starting a render pass");
    VkDevice device = context_.device;

    // Ensure there is a valid command buffer for the current thread
    OUTCOME_TRY(GetActiveCommandBuffer());

    if (rp_in_progress_) {
        vezCmdEndRenderPass();
    };
    rp_in_progress_ = false;

    size_t total_attachments = rp_desc.depth_attachment.active
                                   ? rp_desc.color_attachments.size() + 1
                                   : rp_desc.color_attachments.size();

    std::vector<VezAttachmentInfo> attach_infos;
    attach_infos.reserve(total_attachments);

    for (auto attachment : rp_desc.color_attachments) {
        attach_infos.push_back(
            {attachment.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR
                              : VK_ATTACHMENT_LOAD_OP_LOAD,
             attachment.store ? VK_ATTACHMENT_STORE_OP_STORE
                              : VK_ATTACHMENT_STORE_OP_DONT_CARE,
             {attachment.clear_color[0], attachment.clear_color[1],
              attachment.clear_color[2], attachment.clear_color[3]}});
    }

    if (rp_desc.depth_attachment.active) {
        VezAttachmentInfo depth_info = {
            rp_desc.depth_attachment.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR
                                           : VK_ATTACHMENT_LOAD_OP_LOAD,
            rp_desc.depth_attachment.store ? VK_ATTACHMENT_STORE_OP_STORE
                                           : VK_ATTACHMENT_STORE_OP_DONT_CARE};
        depth_info.clearValue.depthStencil = {
            rp_desc.depth_attachment.clear_depth,
            rp_desc.depth_attachment.clear_stencil};
        attach_infos.push_back(depth_info);
    }

    VezRenderPassBeginInfo rp_info = {};
    rp_info.framebuffer = fb.vez;
    rp_info.attachmentCount = static_cast<uint32_t>(attach_infos.size());
    rp_info.pAttachments = attach_infos.data();

    vezCmdBeginRenderPass(&rp_info);
    rp_in_progress_ = true;
    return outcome::success();
}

result<void> VezBackend::BindTextures(const std::vector<Image>& images,
                                      uint32_t first_binding,
                                      const SamplerDesc* sampler_override) {
    VkSampler sampler_ovr = VK_NULL_HANDLE;
    if (sampler_override != nullptr) {
        OUTCOME_TRY(s, GetSampler(*sampler_override));
        sampler_ovr = s;
    }

    for (const auto& image : images) {
        vezCmdBindImageView(image.vez.image_view,
                            sampler_override ? sampler_ovr : image.vez.sampler,
                            0, first_binding, 0);
    }
    return outcome::success();
}

result<void> VezBackend::BindVertexBuffers(
    const std::vector<Buffer>& vertex_buffers, uint32_t first_binding,
    std::vector<size_t> offsets) {
    std::vector<VkDeviceSize> vk_offsets;
    vk_offsets.reserve(vertex_buffers.size());

    for (const auto& o : offsets) {
        vk_offsets.push_back(static_cast<VkDeviceSize>(o));
    }
    vk_offsets.resize(vertex_buffers.size(), 0);

    std::vector<VkBuffer> buffers;
    buffers.reserve(vertex_buffers.size());

    for (const auto& vb : vertex_buffers) {
        buffers.push_back(vb.vez);
    }

    vezCmdBindVertexBuffers(first_binding,
                            static_cast<uint32_t>(buffers.size()),
                            buffers.data(), vk_offsets.data());
    return outcome::success();
}

result<void> VezBackend::BindIndexBuffer(Buffer index_buffer, size_t offset,
                                         bool short_indices) {
    vezCmdBindIndexBuffer(
        index_buffer.vez, static_cast<VkDeviceSize>(offset),
        short_indices ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
    return outcome::success();
}

result<void> VezBackend::BindGraphicsPipeline(Pipeline pipeline) {
    vezCmdBindPipeline(pipeline.vez);
    return outcome::success();
}

result<void> VezBackend::BindVertexInputFormat(
    VertexInputFormat vertex_input_format) {
    vezCmdSetVertexInputFormat(vertex_input_format.vez);
    return outcome::success();
}

result<void> VezBackend::Draw(uint32_t vertex_count, uint32_t instance_count,
                              uint32_t first_vertex, uint32_t first_instance) {
    vezCmdDraw(vertex_count, instance_count, first_vertex, first_instance);
    return outcome::success();
}

result<void> VezBackend::DrawIndexed(uint32_t index_count,
                                     uint32_t instance_count,
                                     uint32_t first_index,
                                     uint32_t vertex_offset,
                                     uint32_t first_instance) {
    vezCmdDrawIndexed(index_count, instance_count, first_index, vertex_offset,
                      first_instance);
    return outcome::success();
}

result<void> VezBackend::FinishFrame() {
    VkDevice device = context_.device;

    VkQueue graphics_queue = VK_NULL_HANDLE;
    vezGetDeviceGraphicsQueue(device, 0, &graphics_queue);

    if (rp_in_progress_) {
        vezCmdEndRenderPass();
    }
    rp_in_progress_ = false;

    vezEndCommandBuffer();

    auto& per_frame = context_.per_frame[context_.current_frame];

    // Submit the command buffer for the current thread
    VezSubmitInfo submit_info = {};
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &per_frame.command_buffers[thread_id_];
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &per_frame.submission_semaphore;

    VkFence fence = VK_NULL_HANDLE;
    VK_CHECK(vezQueueSubmit(graphics_queue, 1, &submit_info, &fence));
    return outcome::success();
}

result<void> VezBackend::PresentImage(const char* present_image_name) {
    VkDevice device = context_.device;

    OUTCOME_TRY(fb_image, GetFramebufferImage(context_.current_frame,
                                              present_image_name));
    VkImage present_image = fb_image.image;

    VkPipelineStageFlags wait_dst =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkResult res;

    VkQueue graphics_queue = VK_NULL_HANDLE;
    vezGetDeviceGraphicsQueue(device, 0, &graphics_queue);

    auto& per_frame = context_.per_frame[context_.current_frame];

    VezPresentInfo present_info = {};
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &per_frame.submission_semaphore;
    present_info.pWaitDstStageMask = &wait_dst;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &context_.swapchain;
    present_info.pImages = &present_image;
    present_info.pResults = &res;

    vezQueuePresent(graphics_queue, &present_info);
    VK_CHECK(res);

    context_.current_frame =
        (context_.current_frame + 1) % context_.per_frame.size();
    return outcome::success();
};

result<void> VezBackend::TeardownContext() {
    if (context_.device) {
        vezDeviceWaitIdle(context_.device);
    }

    for (auto& per_frame : context_.per_frame) {
        vezFreeCommandBuffers(
            context_.device,
            static_cast<uint32_t>(per_frame.command_buffers.size()),
            per_frame.command_buffers.data());
    }

    for (auto& vertex_input : context_.vertex_input_format_cache) {
        vezDestroyVertexInputFormat(context_.device, vertex_input.second);
    }

    for (auto framebuffer : context_.framebuffer_cache) {
        vezDestroyFramebuffer(context_.device, framebuffer.second);
    }

    for (auto image : context_.texture_cache) {
        vezDestroyImageView(context_.device, image.second.image_view);
        vezDestroyImage(context_.device, image.second.image);
    }

    for (auto image : context_.fb_image_cache) {
        vezDestroyImageView(context_.device, image.second.image_view);
        vezDestroyImage(context_.device, image.second.image);
    }

    for (auto sampler : context_.sampler_cache) {
        vezDestroySampler(context_.device, sampler.second);
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
    vezGetSwapchainSurfaceFormat(swapchain, &context_.swapchain_format);
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
        auto shader_compilation_result =
            vezCreateShaderModule(device, &shader_info, &shader);

        if (shader_compilation_result == VK_ERROR_INITIALIZATION_FAILED) {
            // The info log is stored in the V-EZ shader object
            auto vez_shader = reinterpret_cast<vez::ShaderModule*>(shader);
            LOGE("Vertex shader compilation failed:\n%s",
                 vez_shader->GetInfoLog().c_str());
        }
        VK_CHECK(shader_compilation_result);

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
        shader_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shader_info.codeSize = strlen(source);
        shader_info.pGLSLSource = source;
        shader_info.pEntryPoint = entry_point;

        VkShaderModule shader = VK_NULL_HANDLE;
        auto shader_compilation_result =
            vezCreateShaderModule(device, &shader_info, &shader);

        if (shader_compilation_result == VK_ERROR_INITIALIZATION_FAILED) {
            // The info log is stored in the V-EZ shader object
            auto vez_shader = reinterpret_cast<vez::ShaderModule*>(shader);
            LOGE("Fragment shader compilation failed:\n%s",
                 vez_shader->GetInfoLog().c_str());
        }
        VK_CHECK(shader_compilation_result);

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

result<VulkanImage> VezBackend::CreateFramebufferImage(
    size_t frame_index, const FramebufferColorImageDesc& image_desc,
    uint32_t width, uint32_t height) {
    auto hash = GetFramebufferImageHash(frame_index, image_desc.name.c_str());
    auto result = context_.fb_image_cache.find(hash);
    if (result != context_.fb_image_cache.end()) {
        vezDestroyImageView(context_.device, result->second.image_view);
        vezDestroyImage(context_.device, result->second.image);
    }

    VkFormat format = GetVkFormat(image_desc.format);

    VezImageCreateInfo image_info = {};
    image_info.extent = {width, height, 1};
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.arrayLayers = 1;
    image_info.mipLevels = 1;
    image_info.format = format;
    image_info.samples = static_cast<VkSampleCountFlagBits>(image_desc.samples);
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    OUTCOME_TRY(vulkan_image, CreateImage(hash, image_info));
    context_.fb_image_cache[hash] = vulkan_image;
    return vulkan_image;
}

result<VulkanImage> VezBackend::CreateFramebufferImage(
    size_t frame_index, const FramebufferDepthImageDesc& image_desc,
    uint32_t width, uint32_t height) {
    auto hash = GetFramebufferImageHash(frame_index, image_desc.name.c_str());
    auto result = context_.fb_image_cache.find(hash);
    if (result != context_.fb_image_cache.end()) {
        vezDestroyImageView(context_.device, result->second.image_view);
        vezDestroyImage(context_.device, result->second.image);
    }

    VkFormat format;
    switch (image_desc.depth_type) {
        case DepthImageType::Depth:
            format = VK_FORMAT_D32_SFLOAT;
            break;
        case DepthImageType::DepthStencil:
            format = VK_FORMAT_D32_SFLOAT_S8_UINT;
            break;
        default:
            format = VK_FORMAT_D32_SFLOAT_S8_UINT;
            break;
    }

    VezImageCreateInfo image_info = {};
    image_info.extent = {width, height, 1};
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.arrayLayers = 1;
    image_info.mipLevels = 1;
    image_info.format = format;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                       VK_IMAGE_USAGE_SAMPLED_BIT;

    OUTCOME_TRY(vulkan_image, CreateImage(hash, image_info));
    context_.fb_image_cache[hash] = vulkan_image;
    return vulkan_image;
}

result<VulkanImage> VezBackend::GetFramebufferImage(size_t frame_index,
                                                    const char* name) {
    auto hash = GetFramebufferImageHash(frame_index, name);
    auto result = context_.fb_image_cache.find(hash);
    if (result != context_.fb_image_cache.end()) {
        return result->second;
    }

    return Error::NotFound;
}

result<VkSampler> VezBackend::GetSampler(const SamplerDesc& sampler_desc) {
    auto hash = GetSamplerHash(sampler_desc);
    auto result = context_.sampler_cache.find(hash);

    if (result != context_.sampler_cache.end()) {
        return result->second;
    } else {
        VezSamplerCreateInfo sampler_info = {};

        VkSamplerAddressMode address_mode = {};
        switch (sampler_desc.addressing_mode) {
            case TextureWrappingMode::Repeat:
                address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                break;
            case TextureWrappingMode::MirroredRepeat:
                address_mode = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
                break;
            case TextureWrappingMode::ClampToEdge:
                address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                break;
            default:
                address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                break;
        }

        VkSamplerMipmapMode mipmap_mode = {};
        switch (sampler_desc.mipmap_mode) {
            case FilterType::Linear:
                mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
                break;
            case FilterType::Nearest:
            default:
                mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
                break;
        }

        VkFilter filter = {};
        switch (sampler_desc.filter_type) {
            case FilterType::Linear:
                filter = VK_FILTER_LINEAR;
                break;
            case FilterType::Nearest:
            default:
                filter = VK_FILTER_NEAREST;
                break;
        }

        sampler_info.addressModeU = address_mode;
        sampler_info.addressModeV = address_mode;
        sampler_info.addressModeW = address_mode;
        if (sampler_desc.anisotropy > 0) {
            sampler_info.anisotropyEnable = VK_TRUE;
            sampler_info.maxAnisotropy = sampler_desc.anisotropy;
        }
        sampler_info.minLod = sampler_desc.min_lod;
        sampler_info.maxLod = sampler_desc.max_lod;
        sampler_info.mipLodBias = sampler_desc.lod_bias;
        sampler_info.mipmapMode = mipmap_mode;
        sampler_info.minFilter = filter;
        sampler_info.magFilter = filter;

        VkSampler sampler = VK_NULL_HANDLE;
        vezCreateSampler(context_.device, &sampler_info, &sampler);
        context_.sampler_cache[hash] = sampler;
        return sampler;
    }
}

result<VulkanImage> VezBackend::CreateImage(VezContext::ImageHash hash,
                                            VezImageCreateInfo image_info,
                                            void* initial_contents) {
    assert(context_.device &&
           "Context must be initialized before creating an image");
    VkDevice device = context_.device;

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
        sub_data_info.imageExtent = image_info.extent;
        sub_data_info.imageOffset = {0, 0, 0};
        sub_data_info.imageSubresource = layers;

        vezImageSubData(device, image, &sub_data_info, initial_contents);
    }

    VulkanImage vulkan_image = {image, image_view};
    return vulkan_image;
}

result<void> VezBackend::GetActiveCommandBuffer(uint32_t thread) {
    auto& per_frame = context_.per_frame[context_.current_frame];

    if (!per_frame.command_buffer_active[thread]) {
        VK_CHECK(
            vezBeginCommandBuffer(per_frame.command_buffers[thread],
                                  VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT));
        per_frame.command_buffer_active[thread] = true;

        // TODO separate scissor and viewport (also more details)
        VkRect2D scissor = {{}, {800, 600}};
        vezCmdSetScissor(0, 1, &scissor);

        VkViewport viewport = {};
        viewport.width = 800;
        viewport.height = 600;
        vezCmdSetViewport(0, 1, &viewport);
    }

    return outcome::success();
}

VkFormat VezBackend::GetVkFormat(Format format) {
    switch (format) {
        case Format::UNormRGBA8:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case Format::UNormBGRA8:
            return VK_FORMAT_B8G8R8A8_UNORM;
        case Format::SFloatRGB32:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case Format::SFloatRG32:
            return VK_FORMAT_R32G32_SFLOAT;
        case Format::SFloatR32:
            return VK_FORMAT_R32_SFLOAT;
        case Format::SwapchainFormat:
        case Format::Undefined:
        default:
            // We default to the swapchain format even
            // if the format is undefined
            return context_.swapchain_format.format;
    }
}

VezContext::BufferHash VezBackend::GetBufferHash(const char* name) {
    return {sdbm_hash(name)};
}

VezContext::ShaderHash VezBackend::GetShaderHash(const char* source,
                                                 const char* entry_point) {
    return {sdbm_hash(source), sdbm_hash(entry_point)};
}

VezContext::PipelineHash VezBackend::GetGraphicsPipelineHash(
    VkShaderModule vs, VkShaderModule fs) {
    return {vs, fs};
}

VezContext::VertexInputFormatHash VezBackend::GetVertexInputFormatHash(
    const VertexInputFormatDesc& desc) {
    VezContext::VertexInputFormatHash hash;

    union BindingAttributeBitField {
        struct {
            uint32_t b_binding : 8;
            uint32_t b_stride : 16;
            bool b_per_instance : 1;
            uint32_t a_location : 8;
            uint32_t a_binding : 8;
            uint32_t a_offset : 16;
            Format a_format : 7;
        };

        uint64_t int_repr;
    };

    size_t max_size = std::max(desc.bindings.size(), desc.attributes.size());
    for (size_t i = 0; i < max_size; i++) {
        BindingAttributeBitField bit_field;
        if (i < desc.bindings.size()) {
            const auto& binding = desc.bindings[i];
            bit_field.b_binding = binding.binding;
            bit_field.b_stride = binding.stride;
            bit_field.b_per_instance = binding.per_instance;
        }

        if (i < desc.attributes.size()) {
            const auto& attribute = desc.attributes[i];
            bit_field.a_location = attribute.location;
            bit_field.a_binding = attribute.binding;
            bit_field.a_offset = attribute.offset;
            bit_field.a_format = attribute.format;
        }

        hash.push_back(bit_field.int_repr);
    }

    return hash;
}

VezContext::ImageHash VezBackend::GetTextureHash(const char* name) {
    return {sdbm_hash(name)};
}

VezContext::ImageHash VezBackend::GetFramebufferImageHash(size_t frame_index,
                                                          const char* name) {
    return {frame_index, sdbm_hash(name)};
}

VezContext::FramebufferHash VezBackend::GetFramebufferHash(size_t frame_index,
                                                           const char* name) {
    return {frame_index, sdbm_hash(name)};
}

VezContext::SamplerHash VezBackend::GetSamplerHash(
    const SamplerDesc& sampler_desc) {
    struct SamplerHashBitField {
        float min_lod;
        float max_lod;
        float lod_bias;
        float anisotropy;
        FilterType filter_type : 4;
        FilterType mipmap_mode : 4;
        TextureWrappingMode addressing_mode : 4;
    };

    SamplerHashBitField bit_field = {
        sampler_desc.min_lod,
        sampler_desc.max_lod,
        sampler_desc.lod_bias,
        sampler_desc.anisotropy,
        static_cast<int>(sampler_desc.filter_type),
        static_cast<int>(sampler_desc.mipmap_mode),
        static_cast<int>(sampler_desc.addressing_mode),
    };

    std::vector<uint64_t> hash(3);
    memcpy(hash.data(), &bit_field, sizeof(bit_field));

    return std::move(hash);
}

}  // namespace goma
