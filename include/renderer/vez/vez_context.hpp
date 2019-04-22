#pragma once

#include "renderer/handles.hpp"

#include "common/include.hpp"
#include "common/vez.hpp"

namespace goma {

struct VezContext {
    typedef std::vector<uint64_t> ShaderHash;
    typedef std::map<ShaderHash, VkShaderModule> ShaderCache;

    typedef std::vector<VkShaderModule> PipelineHash;
    typedef std::map<PipelineHash, std::shared_ptr<Pipeline>> PipelineCache;

    typedef std::vector<uint64_t> VertexInputFormatHash;
    typedef std::map<VertexInputFormatHash, std::shared_ptr<VertexInputFormat>>
        VertexInputFormatCache;

    typedef std::vector<uint64_t> BufferHash;
    typedef std::map<BufferHash, std::shared_ptr<Buffer>> BufferCache;

    typedef std::vector<uint64_t> ImageHash;
    typedef std::map<ImageHash, std::shared_ptr<Image>> ImageCache;

    typedef std::vector<uint64_t> SamplerHash;
    typedef std::map<SamplerHash, VkSampler> SamplerCache;

    typedef std::vector<uint64_t> FramebufferHash;
    typedef std::map<FramebufferHash, VezFramebuffer> FramebufferCache;

    VkInstance instance{VK_NULL_HANDLE};
    VkDebugReportCallbackEXT debug_callback{VK_NULL_HANDLE};

    VkPhysicalDevice physical_device{VK_NULL_HANDLE};
    VkPhysicalDeviceProperties properties{};
    VkPhysicalDeviceFeatures features{};

    VkDevice device{VK_NULL_HANDLE};

    VkSurfaceKHR surface{VK_NULL_HANDLE};
    VkSurfaceCapabilitiesKHR capabilities{};

    VezSwapchain swapchain{VK_NULL_HANDLE};
    VkSurfaceFormatKHR swapchain_format{};

    ShaderCache vertex_shader_cache{};
    ShaderCache fragment_shader_cache{};
    PipelineCache pipeline_cache{};
    VertexInputFormatCache vertex_input_format_cache{};
    BufferCache buffer_cache{};
    ImageCache fb_image_cache{};
    ImageCache texture_cache{};
    SamplerCache sampler_cache{};
    FramebufferCache framebuffer_cache{};

    struct PerFrame {
        std::vector<VkCommandBuffer> command_buffers{};
        std::vector<bool> command_buffer_active{};

        VkCommandBuffer setup_command_buffer{VK_NULL_HANDLE};
        bool setup_command_buffer_active{false};

        VkSemaphore submission_semaphore{VK_NULL_HANDLE};
        VkSemaphore setup_semaphore{VK_NULL_HANDLE};
        VkSemaphore presentation_semaphore{VK_NULL_HANDLE};
        VkFence submission_fence{VK_NULL_HANDLE};
        VkFence setup_fence{VK_NULL_HANDLE};
    };
    std::vector<PerFrame> per_frame{};
    size_t current_frame{0};
};

}  // namespace goma
