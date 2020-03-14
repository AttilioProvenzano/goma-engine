#pragma once

#include "platform/platform.hpp"

#include "common/include.hpp"

struct GLFWwindow;

namespace goma {

class Win32Platform : public Platform {
  public:
    virtual ~Win32Platform() override;

    virtual result<void> MainLoop(MainLoopFn inner_loop) override;

    virtual uint32_t GetWidth() const override;
    virtual uint32_t GetHeight() const override;

    virtual InputState GetInputState() const override;

    virtual result<void> InitWindow(int width = 1280,
                                    int height = 800) override;

    virtual result<VkSurfaceKHR> CreateVulkanSurface(
        VkInstance instance) const override;

    virtual void Sleep(uint32_t microseconds) override;

    static result<std::string> ReadFile(const std::string& filename,
                                        bool binary);

    static result<void> WriteFile(const std::string& filename, size_t size,
                                  const char* data, bool binary);

  private:
    GLFWwindow* window_{nullptr};
};

}  // namespace goma
