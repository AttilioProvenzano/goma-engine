#pragma once

#define VK_NO_PROTOTYPES
#include "VEZ.h"
#include "volk.h"

#include <outcome.hpp>
namespace outcome = OUTCOME_V2_NAMESPACE;
using outcome::result;

namespace goma {

class Platform {
  public:
    virtual ~Platform() = default;

    virtual result<void> InitWindow() = 0;
    virtual result<VkSurfaceKHR> CreateVulkanSurface(
        VkInstance instance) const = 0;
};

}  // namespace goma
