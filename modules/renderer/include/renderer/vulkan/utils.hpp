#pragma once

#include "common/include.hpp"
#include "common/vulkan.hpp"

namespace goma {

struct FramebufferDesc;

namespace utils {

result<VkRenderPass> CreateRenderPass(VkDevice device,
                                      const FramebufferDesc& desc);

struct FormatInfo {
    uint32_t size;
    uint32_t channel_count;
};

FormatInfo GetFormatInfo(VkFormat format);

}  // namespace utils
}  // namespace goma
