#pragma once

#include "common/include.hpp"
#include "common/vez.hpp"

namespace goma {

enum class KeyInput { W, A, S, D, Up, Down, Left, Right };
struct InputState {
    std::set<KeyInput> keypresses;
};

class Platform {
  public:
    virtual ~Platform() = default;

    virtual uint32_t GetWidth() = 0;
    virtual uint32_t GetHeight() = 0;

    virtual InputState GetInputState() = 0;

    virtual result<void> InitWindow() = 0;
    virtual result<VkSurfaceKHR> CreateVulkanSurface(
        VkInstance instance) const = 0;
};

}  // namespace goma
