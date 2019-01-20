#pragma once

#include "platform/platform.hpp"

struct GLFWwindow;

namespace goma {

class Win32Platform : public Platform {
  public:
    virtual ~Win32Platform() override;

    virtual result<void> InitWindow() override;
    virtual result<VkSurfaceKHR> CreateVulkanSurface(
        VkInstance instance) const override;

  private:
    GLFWwindow* window_ = nullptr;
};

}  // namespace goma
