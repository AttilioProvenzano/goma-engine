#pragma once

#include "common/include.hpp"
#include "common/vulkan.hpp"

namespace goma {

struct BufferDesc {
    VkDeviceSize size;
    size_t num_elements;
    uint32_t stride;

    VkBufferUsageFlags usage;
    VmaMemoryUsage storage;
};

class Buffer {
  public:
    Buffer(const BufferDesc&);

    VkDeviceSize GetSize();
    uint32_t GetStride();
    size_t GetNumElements();

    VkBufferUsageFlags GetUsage();
    VmaMemoryUsage GetStorage();

    void SetHandle(VkBuffer);
    VkBuffer GetHandle();

    struct Allocation {
        VmaAllocation allocation;
        VmaAllocationInfo allocation_info;
    };

    void SetAllocation(Allocation);
    Allocation GetAllocation();

  private:
    BufferDesc desc_;

    struct {
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VmaAllocationInfo allocation_info = {};
    } api_handles_;
};

}  // namespace goma
