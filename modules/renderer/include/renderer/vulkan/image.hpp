#pragma once

#include "common/include.hpp"
#include "common/vulkan.hpp"

namespace goma {

class Image {
    Image(ImageDescription);

    std::array<uint32_t, 3> GetSize();

    void SetAPIResource(VkImage);
    VkImage GetAPIResource();

    ImageDescription mDesc;
    VkImage mAPIResource;
};

}  // namespace goma
