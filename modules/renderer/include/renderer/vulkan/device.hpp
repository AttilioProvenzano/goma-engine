#pragma once

#include "common/include.hpp"
#include "common/vulkan.hpp"
#include "renderer/buffer.hpp"

namespace goma {

class Platform;

class Device {
  public:
    struct Config {
        enum class FbColorSpace {
            Linear,
            Srgb
        } fb_color_space = FbColorSpace::Srgb;
    };

    Device(const Config& config = {});  // TODO: Teardown
    result<void> InitWindow(Platform& platform);

    VkDevice GetHandle();
    uint32_t GetQueueFamilyIndex();
    result<Buffer*> CreateBuffer(const BufferDesc& buffer_desc);

    /*
void ProcessWindowChanges(Platform&);
Texture* CreateTexture(TextureDescription);
Shader* CreateShader(ShaderDescription);
Pipeline* CreatePipeline(PipelineDescription);

Receipt SubmitWork(Context);
void WaitOnWork(Receipt);
void Present();
*/

  private:
    result<void> Init();

    Config config_;
    uint32_t queue_family_index_ = -1;

    struct {
        VkInstance instance;
        VkDebugReportCallbackEXT debug_callback;
        VkPhysicalDevice physical_device;
        VkPhysicalDeviceFeatures features;
        VkPhysicalDeviceProperties properties;
        VkDevice device;

        VkSurfaceKHR surface;
        VkSwapchainKHR swapchain;

        VmaAllocator allocator;
    } api_handles_;

    std::vector<std::unique_ptr<Buffer>> buffers_;
};

}  // namespace goma
