#pragma once

#include "common/include.hpp"
#include "common/vulkan.hpp"

namespace goma {

struct ImageDesc {
    VkExtent3D size;
    VkFormat format;
    VkImageUsageFlags usage;
    uint32_t mip_levels = 1;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    VkImageViewType type = VK_IMAGE_VIEW_TYPE_2D;
    uint32_t array_layers = 1;
    VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;

    VmaMemoryUsage storage = VMA_MEMORY_USAGE_GPU_ONLY;

    static const ImageDesc ColorAttachmentDesc;
    static const ImageDesc DepthAttachmentDesc;
    static const ImageDesc TextureDesc;
};

class Image {
  public:
    Image(const ImageDesc&);

    VkExtent3D GetSize();
    VkFormat GetFormat();
    VkSampleCountFlagBits GetSampleCount();

    void SetHandle(VkImage);
    VkImage GetHandle();

    void SetView(VkImageView);
    VkImageView GetView();

    struct Allocation {
        VmaAllocation allocation;
        VmaAllocationInfo allocation_info;
    };

    void SetAllocation(Allocation);
    Allocation GetAllocation();

  private:
    ImageDesc desc_;

    struct {
        VkImage image = VK_NULL_HANDLE;
        VkImageView image_view = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VmaAllocationInfo allocation_info = {};
    } api_handles_;
};

}  // namespace goma
