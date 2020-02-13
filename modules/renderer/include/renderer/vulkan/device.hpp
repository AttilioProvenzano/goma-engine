#pragma once

#include "common/include.hpp"
#include "common/vulkan.hpp"
#include "renderer/buffer.hpp"
#include "renderer/image.hpp"
#include "renderer/pipeline.hpp"
#include "renderer/shader.hpp"

namespace goma {

struct FramebufferDesc;
class Context;
class Platform;

struct Receipt {
    size_t submission_id;
    VkDevice device = VK_NULL_HANDLE;
};
using ReceiptPtr = std::unique_ptr<Receipt>;

class Device {
  public:
    struct Config {
        enum class FbColorSpace {
            Linear,
            Srgb
        } fb_color_space = FbColorSpace::Srgb;
    };

    Device(const Config& = {});
    ~Device();

    result<void> InitWindow(Platform&);

    VkDevice GetHandle();
    uint32_t GetQueueFamilyIndex();

    result<Buffer*> CreateBuffer(const BufferDesc&);
    result<void*> MapBuffer(Buffer&);
    void UnmapBuffer(Buffer&);

    result<Image*> AcquireSwapchainImage();
    result<Image*> CreateImage(const ImageDesc&);

    result<Shader*> CreateShader(ShaderDesc);
    result<Pipeline*> CreatePipeline(PipelineDesc, FramebufferDesc&);

    result<ReceiptPtr> Submit(Context&);
    result<void> WaitOnWork(ReceiptPtr&&);
    result<void> Present();

    /*
void ProcessWindowChanges(Platform&);
Image* CreateImage(ImageDescription);
*/

  private:
    result<void> Init();
    VkSemaphore GetSemaphore();
    VkFence GetFence();

    Config config_;
    uint32_t queue_family_index_ = -1;

    struct {
        VkInstance instance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
        VkPhysicalDevice physical_device = VK_NULL_HANDLE;
        VkPhysicalDeviceFeatures features = {};
        VkPhysicalDeviceProperties properties = {};
        VkDevice device = VK_NULL_HANDLE;
        VkQueue queue = VK_NULL_HANDLE;

        VkSurfaceKHR surface = VK_NULL_HANDLE;
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;

        VmaAllocator allocator = VK_NULL_HANDLE;
        VkPipelineCache pipeline_cache = VK_NULL_HANDLE;
        std::vector<VkPipelineLayout> pipeline_layouts;
        std::vector<VkRenderPass> render_passes;
    } api_handles_;

    std::vector<std::unique_ptr<Buffer>> buffers_;
    std::vector<std::unique_ptr<Image>> images_;
    std::vector<std::unique_ptr<Image>> swapchain_images_;
    std::vector<std::unique_ptr<Pipeline>> pipelines_;
    std::vector<std::unique_ptr<Shader>> shaders_;

    std::vector<VkSemaphore> recycled_semaphores_;

    std::unordered_map<size_t, VkSemaphore> acquisition_semaphores_;
    std::unordered_map<size_t, VkSemaphore> presentation_semaphores_;

    std::unordered_map<size_t, VkCommandBuffer> acquisition_cmd_bufs_;
    std::unordered_map<size_t, VkCommandBuffer> presentation_cmd_bufs_;
    VkCommandPool cmd_pool_;

    std::vector<VkFence> recycled_fences_;
    std::unordered_map<size_t, VkFence> submission_fences_;

    static const uint32_t kInvalidSwapchainIndex;
    size_t last_submission_id_ = 0;
    uint32_t swapchain_index_ = kInvalidSwapchainIndex;
};

}  // namespace goma
