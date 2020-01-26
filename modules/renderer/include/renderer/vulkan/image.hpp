#pragma once

#include "common/include.hpp"
#include "common/vulkan.hpp"

namespace goma {

class Image {
  public:
    Image();

    VkExtent3D GetSize();
    VkFormat GetFormat();
    VkSampleCountFlagBits GetSampleCount();

    void SetAPIResource(VkImage);
    VkImage GetAPIResource();
    VkImageView GetViewHandle(); // TODO: probably bundle image and image view
};

}  // namespace goma
