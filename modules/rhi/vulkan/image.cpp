#include "rhi/vulkan/image.hpp"

namespace goma {

const ImageDesc ImageDesc::ColorAttachmentDesc = {
    {0, 0, 1},
    VK_FORMAT_R8G8B8A8_UNORM,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT};
const ImageDesc ImageDesc::DepthAttachmentDesc = {
    {0, 0, 1},
    VK_FORMAT_D32_SFLOAT_S8_UINT,
    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT};
const ImageDesc ImageDesc::TextureDesc = {{0, 0, 1},
                                          VK_FORMAT_R8G8B8A8_SRGB,
                                          VK_IMAGE_USAGE_SAMPLED_BIT |
                                              VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                              VK_IMAGE_USAGE_TRANSFER_DST_BIT};
const ImageDesc ImageDesc::LinearTextureDesc = {
    {0, 0, 1},
    VK_FORMAT_R8G8B8A8_UNORM,
    VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT};

Image::Image(const ImageDesc& image_desc) : desc_(image_desc) {}

VkExtent3D Image::GetSize() { return desc_.size; }
VkFormat Image::GetFormat() { return desc_.format; }
VkSampleCountFlagBits Image::GetSampleCount() { return desc_.samples; }
uint32_t Image::GetLayerCount() { return desc_.array_layers; }
uint32_t Image::GetMipLevels() { return desc_.mip_levels; }

void Image::SetHandle(VkImage image) { api_handles_.image = image; }
VkImage Image::GetHandle() { return api_handles_.image; }

void Image::SetView(VkImageView image_view) {
    api_handles_.image_view = image_view;
}
VkImageView Image::GetView() { return api_handles_.image_view; }

void Image::SetAllocation(Allocation allocation) {
    api_handles_.allocation = allocation.allocation;
    api_handles_.allocation_info = allocation.allocation_info;
}
Image::Allocation Image::GetAllocation() {
    return {api_handles_.allocation, api_handles_.allocation_info};
}

}  // namespace goma
