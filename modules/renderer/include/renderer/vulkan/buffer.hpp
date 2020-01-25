#pragma once

#include "common/include.hpp"
#include "common/vulkan.hpp"

namespace goma {

class Buffer {
    Buffer(BufferDescription);

    uint32_t GetStride();
    uint32_t GetSize();
    uint32_t GetNumElements();

    void SetAPIResource(VkBuffer);
    VkBuffer GetAPIResource();

    BufferDescription mDesc;
    VkBuffer mAPIResource;
};

}  // namespace goma
