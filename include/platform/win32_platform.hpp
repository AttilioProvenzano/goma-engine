#pragma once

#include "platform/platform.hpp"

#include "common/include.hpp"

struct GLFWwindow;

namespace goma {

class Win32Platform : public Platform {
  public:
    virtual ~Win32Platform() override;

    virtual uint32_t GetWidth() override;
    virtual uint32_t GetHeight() override;

    virtual InputState GetInputState() override;

    virtual result<void> InitWindow() override;
    virtual result<VkSurfaceKHR> CreateVulkanSurface(
        VkInstance instance) const override;

  private:
    GLFWwindow* window_ = nullptr;
};

}  // namespace goma
