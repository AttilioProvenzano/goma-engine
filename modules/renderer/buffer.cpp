#include "renderer/vulkan/buffer.hpp"

namespace goma {

Buffer::Buffer(const BufferDesc& buffer_desc) : desc_(buffer_desc) {}

VkDeviceSize Buffer::GetSize() { return desc_.size; }
uint32_t Buffer::GetStride() { return desc_.stride; }
size_t Buffer::GetNumElements() { return desc_.num_elements; }

VkBufferUsageFlags Buffer::GetUsage() { return desc_.usage; }
VmaMemoryUsage Buffer::GetStorage() { return desc_.storage; }

void Buffer::SetHandle(VkBuffer buffer) { api_handles_.buffer = buffer; }
VkBuffer Buffer::GetHandle() { return api_handles_.buffer; }

void Buffer::SetAllocation(Allocation allocation) {
    api_handles_.allocation = allocation.allocation;
    api_handles_.allocation_info = allocation.allocation_info;
}
Buffer::Allocation Buffer::GetAllocation() {
    return {api_handles_.allocation, api_handles_.allocation_info};
}

}  // namespace goma
