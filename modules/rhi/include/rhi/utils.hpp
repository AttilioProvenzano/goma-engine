#pragma once

#include "common/include.hpp"
#include "common/vulkan.hpp"

namespace goma {

namespace utils {

uint32_t ComputeMipLevels(uint32_t width, uint32_t height);

struct FormatInfo {
    uint32_t size;
    uint32_t channel_count;
};

FormatInfo GetFormatInfo(VkFormat format);

}  // namespace utils
}  // namespace goma
