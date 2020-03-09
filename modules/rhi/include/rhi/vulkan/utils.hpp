#pragma once

#include "common/include.hpp"
#include "common/vulkan.hpp"

namespace goma {

struct FramebufferDesc;

namespace utils {

result<VkRenderPass> CreateRenderPass(VkDevice device,
                                      const FramebufferDesc& desc);

}  // namespace utils
}  // namespace goma
