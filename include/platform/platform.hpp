#pragma once

#include "input/input.hpp"

#include "common/include.hpp"
#include "common/vez.hpp"

namespace goma {

typedef std::function<bool(void)> MainLoopFn;

class Platform {
  public:
    virtual ~Platform() = default;

    virtual result<void> MainLoop(MainLoopFn inner_loop) = 0;

    virtual uint32_t GetWidth() const = 0;
    virtual uint32_t GetHeight() const = 0;

    virtual InputState GetInputState() const = 0;

    virtual result<void> InitWindow() = 0;
    virtual result<VkSurfaceKHR> CreateVulkanSurface(
        VkInstance instance) const = 0;

    virtual void Sleep(uint32_t microseconds) = 0;
};

}  // namespace goma
