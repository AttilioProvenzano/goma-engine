#pragma once

#include "common/include.hpp"
#include "common/vulkan.hpp"

namespace goma {

class Image {
  public:
    Image();

    VkExtent3D GetSize() { return {800, 600, 1}; }
    VkFormat GetFormat() { return VK_FORMAT_UNDEFINED; }
    VkSampleCountFlagBits GetSampleCount() { return VK_SAMPLE_COUNT_1_BIT; }

    void SetAPIResource(VkImage) {}
    // TODO: probably bundle image and image view
    VkImage GetAPIResource() { return VK_NULL_HANDLE; }
    VkImageView GetViewHandle() { return VK_NULL_HANDLE; }
};

}  // namespace goma
