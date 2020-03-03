#pragma once

#include "common/include.hpp"
#include "common/vulkan.hpp"

namespace goma {

struct FramebufferDesc;

namespace utils {

uint32_t ComputeMipLevels(uint32_t width, uint32_t height);

result<VkRenderPass> CreateRenderPass(VkDevice device,
                                      const FramebufferDesc& desc);

struct FormatInfo {
    uint32_t size;
    uint32_t channel_count;
};

FormatInfo GetFormatInfo(VkFormat format);

}  // namespace utils
}  // namespace goma
