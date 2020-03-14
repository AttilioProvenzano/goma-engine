#pragma once

#include "common/include.hpp"
#include "common/vulkan.hpp"

namespace goma {

enum class FormatCompression {
    Uncompressed,
    ASTC,
    BC,
    ETC2,
    PVRTC,
};

struct FormatInfo {
    uint32_t size;
    uint32_t channel_count;
};

namespace utils {

uint32_t ComputeMipLevels(uint32_t width, uint32_t height);

FormatInfo GetFormatInfo(VkFormat format);

VkExtent3D GetFormatBlockSize(VkFormat format);

FormatCompression GetFormatCompression(VkFormat format);

}  // namespace utils
}  // namespace goma
