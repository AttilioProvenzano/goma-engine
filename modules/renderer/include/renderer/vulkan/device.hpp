#pragma once

#include "common/include.hpp"
#include "common/vulkan.hpp"

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

    Device(const Config& config = {});
    result<void> InitWindow(Platform&);

    /*
void ProcessWindowChanges(Platform&);
Buffer* CreateBuffer(BufferDescription);
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
    } api_handles_;
};

}  // namespace goma
