#pragma once

#include "platform/platform.hpp"

struct GLFWwindow;

namespace goma {

class Win32Platform : public Platform {
  public:
    virtual ~Win32Platform();

    virtual result<void> InitWindow();
    virtual result<VkSurfaceKHR> CreateVulkanSurface(VkInstance instance) const;

  private:
    GLFWwindow* window_ = nullptr;
};

}  // namespace goma
