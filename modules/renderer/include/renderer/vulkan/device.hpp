#pragma once

#include "common/include.hpp"
#include "common/vulkan.hpp"
#include "renderer/buffer.hpp"
#include "renderer/pipeline.hpp"
#include "renderer/shader.hpp"

namespace goma {

struct FramebufferDesc;
class Platform;

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
    result<void*> MapBuffer(Buffer*);
    void UnmapBuffer(Buffer*);

    result<Shader*> CreateShader(ShaderDesc);
    result<Pipeline*> CreatePipeline(PipelineDesc, FramebufferDesc&);

    /*
void ProcessWindowChanges(Platform&);
Texture* CreateTexture(TextureDescription);

Receipt SubmitWork(Context);
void WaitOnWork(Receipt);
void Present();
*/

  private:
    result<void> Init();

    Config config_;
    uint32_t queue_family_index_ = -1;

    struct {
        VkInstance instance = VK_NULL_HANDLE;
        VkDebugReportCallbackEXT debug_callback = VK_NULL_HANDLE;
        VkPhysicalDevice physical_device = VK_NULL_HANDLE;
        VkPhysicalDeviceFeatures features = {};
        VkPhysicalDeviceProperties properties = {};
        VkDevice device = VK_NULL_HANDLE;

        VkSurfaceKHR surface = VK_NULL_HANDLE;
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;

        VmaAllocator allocator = VK_NULL_HANDLE;
        VkPipelineCache pipeline_cache = VK_NULL_HANDLE;
        std::vector<VkPipelineLayout> pipeline_layouts;
        std::vector<VkRenderPass> render_passes;
    } api_handles_;

    std::vector<std::unique_ptr<Buffer>> buffers_;
    std::vector<std::unique_ptr<Pipeline>> pipelines_;
    std::vector<std::unique_ptr<Shader>> shaders_;
};

}  // namespace goma
