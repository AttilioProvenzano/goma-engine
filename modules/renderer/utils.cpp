#include "renderer/vulkan/utils.hpp"

#include "renderer/vulkan/context.hpp"
#include "renderer/vulkan/image.hpp"

namespace goma {
namespace utils {

uint32_t ComputeMipLevels(uint32_t width, uint32_t height) {
    auto mip_levels = 1U;
    auto min_wh = std::min(width, height);
    while (min_wh >> mip_levels) {
        mip_levels++;
    }
    return mip_levels;
}

result<VkRenderPass> CreateRenderPass(VkDevice device,
                                      const FramebufferDesc& desc) {
    std::vector<VkAttachmentDescription> attachments;
    std::vector<VkAttachmentReference> color_attachments;
    std::vector<VkAttachmentReference> resolve_attachments;
    std::vector<VkClearValue> clear_values;

    for (const auto& c : desc.color_attachments) {
        assert(c.image && "Invalid image in color attachments");

        VkAttachmentDescription attachment = {};
        attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachment.format = c.image->GetFormat();
        attachment.loadOp = c.load_op;
        attachment.storeOp = c.store_op;
        attachment.samples = c.image->GetSampleCount();

        attachments.push_back(attachment);
        clear_values.push_back(c.clear_value);

        VkAttachmentReference attachment_ref = {};
        attachment_ref.attachment =
            static_cast<uint32_t>(attachments.size()) - 1;
        attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        color_attachments.push_back(attachment_ref);

        if (c.resolve_to) {
            VkAttachmentDescription attachment = {};
            attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachment.format = c.resolve_to->GetFormat();
            attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachment.samples = c.resolve_to->GetSampleCount();

            attachments.push_back(attachment);
            clear_values.push_back(c.clear_value);

            VkAttachmentReference attachment_ref = {};
            attachment_ref.attachment =
                static_cast<uint32_t>(attachments.size()) - 1;
            attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            resolve_attachments.push_back(attachment_ref);
        }
    }

    VkAttachmentReference depth_attachment;
    auto& d = desc.depth_attachment;

    if (d.image) {
        VkAttachmentDescription attachment = {};
        attachment.initialLayout =
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachment.finalLayout =
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachment.format = d.image->GetFormat();
        attachment.loadOp = d.load_op;
        attachment.storeOp = d.store_op;
        attachment.samples = d.image->GetSampleCount();

        attachments.push_back(attachment);
        clear_values.push_back(d.clear_value);

        depth_attachment.attachment =
            static_cast<uint32_t>(attachments.size()) - 1;
        depth_attachment.layout =
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount =
        static_cast<uint32_t>(color_attachments.size());
    subpass.pColorAttachments = color_attachments.data();

    if (d.image) {
        subpass.pDepthStencilAttachment = &depth_attachment;
    }

    if (!resolve_attachments.empty()) {
        subpass.pResolveAttachments = resolve_attachments.data();
    }

    VkRenderPassCreateInfo rp_info = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rp_info.attachmentCount = static_cast<uint32_t>(attachments.size());
    rp_info.pAttachments = attachments.data();
    rp_info.subpassCount = 1;
    rp_info.pSubpasses = &subpass;

    VkRenderPass render_pass = VK_NULL_HANDLE;
    VK_CHECK(vkCreateRenderPass(device, &rp_info, nullptr, &render_pass));

    return render_pass;
}

FormatInfo GetFormatInfo(VkFormat format) {
    static const std::map<VkFormat, FormatInfo> vk_format_info = {
        {VK_FORMAT_UNDEFINED, {0, 0}},
        {VK_FORMAT_R4G4_UNORM_PACK8, {1, 2}},
        {VK_FORMAT_R4G4B4A4_UNORM_PACK16, {2, 4}},
        {VK_FORMAT_B4G4R4A4_UNORM_PACK16, {2, 4}},
        {VK_FORMAT_R5G6B5_UNORM_PACK16, {2, 3}},
        {VK_FORMAT_B5G6R5_UNORM_PACK16, {2, 3}},
        {VK_FORMAT_R5G5B5A1_UNORM_PACK16, {2, 4}},
        {VK_FORMAT_B5G5R5A1_UNORM_PACK16, {2, 4}},
        {VK_FORMAT_A1R5G5B5_UNORM_PACK16, {2, 4}},
        {VK_FORMAT_R8_UNORM, {1, 1}},
        {VK_FORMAT_R8_SNORM, {1, 1}},
        {VK_FORMAT_R8_USCALED, {1, 1}},
        {VK_FORMAT_R8_SSCALED, {1, 1}},
        {VK_FORMAT_R8_UINT, {1, 1}},
        {VK_FORMAT_R8_SINT, {1, 1}},
        {VK_FORMAT_R8_SRGB, {1, 1}},
        {VK_FORMAT_R8G8_UNORM, {2, 2}},
        {VK_FORMAT_R8G8_SNORM, {2, 2}},
        {VK_FORMAT_R8G8_USCALED, {2, 2}},
        {VK_FORMAT_R8G8_SSCALED, {2, 2}},
        {VK_FORMAT_R8G8_UINT, {2, 2}},
        {VK_FORMAT_R8G8_SINT, {2, 2}},
        {VK_FORMAT_R8G8_SRGB, {2, 2}},
        {VK_FORMAT_R8G8B8_UNORM, {3, 3}},
        {VK_FORMAT_R8G8B8_SNORM, {3, 3}},
        {VK_FORMAT_R8G8B8_USCALED, {3, 3}},
        {VK_FORMAT_R8G8B8_SSCALED, {3, 3}},
        {VK_FORMAT_R8G8B8_UINT, {3, 3}},
        {VK_FORMAT_R8G8B8_SINT, {3, 3}},
        {VK_FORMAT_R8G8B8_SRGB, {3, 3}},
        {VK_FORMAT_B8G8R8_UNORM, {3, 3}},
        {VK_FORMAT_B8G8R8_SNORM, {3, 3}},
        {VK_FORMAT_B8G8R8_USCALED, {3, 3}},
        {VK_FORMAT_B8G8R8_SSCALED, {3, 3}},
        {VK_FORMAT_B8G8R8_UINT, {3, 3}},
        {VK_FORMAT_B8G8R8_SINT, {3, 3}},
        {VK_FORMAT_B8G8R8_SRGB, {3, 3}},
        {VK_FORMAT_R8G8B8A8_UNORM, {4, 4}},
        {VK_FORMAT_R8G8B8A8_SNORM, {4, 4}},
        {VK_FORMAT_R8G8B8A8_USCALED, {4, 4}},
        {VK_FORMAT_R8G8B8A8_SSCALED, {4, 4}},
        {VK_FORMAT_R8G8B8A8_UINT, {4, 4}},
        {VK_FORMAT_R8G8B8A8_SINT, {4, 4}},
        {VK_FORMAT_R8G8B8A8_SRGB, {4, 4}},
        {VK_FORMAT_B8G8R8A8_UNORM, {4, 4}},
        {VK_FORMAT_B8G8R8A8_SNORM, {4, 4}},
        {VK_FORMAT_B8G8R8A8_USCALED, {4, 4}},
        {VK_FORMAT_B8G8R8A8_SSCALED, {4, 4}},
        {VK_FORMAT_B8G8R8A8_UINT, {4, 4}},
        {VK_FORMAT_B8G8R8A8_SINT, {4, 4}},
        {VK_FORMAT_B8G8R8A8_SRGB, {4, 4}},
        {VK_FORMAT_A8B8G8R8_UNORM_PACK32, {4, 4}},
        {VK_FORMAT_A8B8G8R8_SNORM_PACK32, {4, 4}},
        {VK_FORMAT_A8B8G8R8_USCALED_PACK32, {4, 4}},
        {VK_FORMAT_A8B8G8R8_SSCALED_PACK32, {4, 4}},
        {VK_FORMAT_A8B8G8R8_UINT_PACK32, {4, 4}},
        {VK_FORMAT_A8B8G8R8_SINT_PACK32, {4, 4}},
        {VK_FORMAT_A8B8G8R8_SRGB_PACK32, {4, 4}},
        {VK_FORMAT_A2R10G10B10_UNORM_PACK32, {4, 4}},
        {VK_FORMAT_A2R10G10B10_SNORM_PACK32, {4, 4}},
        {VK_FORMAT_A2R10G10B10_USCALED_PACK32, {4, 4}},
        {VK_FORMAT_A2R10G10B10_SSCALED_PACK32, {4, 4}},
        {VK_FORMAT_A2R10G10B10_UINT_PACK32, {4, 4}},
        {VK_FORMAT_A2R10G10B10_SINT_PACK32, {4, 4}},
        {VK_FORMAT_A2B10G10R10_UNORM_PACK32, {4, 4}},
        {VK_FORMAT_A2B10G10R10_SNORM_PACK32, {4, 4}},
        {VK_FORMAT_A2B10G10R10_USCALED_PACK32, {4, 4}},
        {VK_FORMAT_A2B10G10R10_SSCALED_PACK32, {4, 4}},
        {VK_FORMAT_A2B10G10R10_UINT_PACK32, {4, 4}},
        {VK_FORMAT_A2B10G10R10_SINT_PACK32, {4, 4}},
        {VK_FORMAT_R16_UNORM, {2, 1}},
        {VK_FORMAT_R16_SNORM, {2, 1}},
        {VK_FORMAT_R16_USCALED, {2, 1}},
        {VK_FORMAT_R16_SSCALED, {2, 1}},
        {VK_FORMAT_R16_UINT, {2, 1}},
        {VK_FORMAT_R16_SINT, {2, 1}},
        {VK_FORMAT_R16_SFLOAT, {2, 1}},
        {VK_FORMAT_R16G16_UNORM, {4, 2}},
        {VK_FORMAT_R16G16_SNORM, {4, 2}},
        {VK_FORMAT_R16G16_USCALED, {4, 2}},
        {VK_FORMAT_R16G16_SSCALED, {4, 2}},
        {VK_FORMAT_R16G16_UINT, {4, 2}},
        {VK_FORMAT_R16G16_SINT, {4, 2}},
        {VK_FORMAT_R16G16_SFLOAT, {4, 2}},
        {VK_FORMAT_R16G16B16_UNORM, {6, 3}},
        {VK_FORMAT_R16G16B16_SNORM, {6, 3}},
        {VK_FORMAT_R16G16B16_USCALED, {6, 3}},
        {VK_FORMAT_R16G16B16_SSCALED, {6, 3}},
        {VK_FORMAT_R16G16B16_UINT, {6, 3}},
        {VK_FORMAT_R16G16B16_SINT, {6, 3}},
        {VK_FORMAT_R16G16B16_SFLOAT, {6, 3}},
        {VK_FORMAT_R16G16B16A16_UNORM, {8, 4}},
        {VK_FORMAT_R16G16B16A16_SNORM, {8, 4}},
        {VK_FORMAT_R16G16B16A16_USCALED, {8, 4}},
        {VK_FORMAT_R16G16B16A16_SSCALED, {8, 4}},
        {VK_FORMAT_R16G16B16A16_UINT, {8, 4}},
        {VK_FORMAT_R16G16B16A16_SINT, {8, 4}},
        {VK_FORMAT_R16G16B16A16_SFLOAT, {8, 4}},
        {VK_FORMAT_R32_UINT, {4, 1}},
        {VK_FORMAT_R32_SINT, {4, 1}},
        {VK_FORMAT_R32_SFLOAT, {4, 1}},
        {VK_FORMAT_R32G32_UINT, {8, 2}},
        {VK_FORMAT_R32G32_SINT, {8, 2}},
        {VK_FORMAT_R32G32_SFLOAT, {8, 2}},
        {VK_FORMAT_R32G32B32_UINT, {12, 3}},
        {VK_FORMAT_R32G32B32_SINT, {12, 3}},
        {VK_FORMAT_R32G32B32_SFLOAT, {12, 3}},
        {VK_FORMAT_R32G32B32A32_UINT, {16, 4}},
        {VK_FORMAT_R32G32B32A32_SINT, {16, 4}},
        {VK_FORMAT_R32G32B32A32_SFLOAT, {16, 4}},
        {VK_FORMAT_R64_UINT, {8, 1}},
        {VK_FORMAT_R64_SINT, {8, 1}},
        {VK_FORMAT_R64_SFLOAT, {8, 1}},
        {VK_FORMAT_R64G64_UINT, {16, 2}},
        {VK_FORMAT_R64G64_SINT, {16, 2}},
        {VK_FORMAT_R64G64_SFLOAT, {16, 2}},
        {VK_FORMAT_R64G64B64_UINT, {24, 3}},
        {VK_FORMAT_R64G64B64_SINT, {24, 3}},
        {VK_FORMAT_R64G64B64_SFLOAT, {24, 3}},
        {VK_FORMAT_R64G64B64A64_UINT, {32, 4}},
        {VK_FORMAT_R64G64B64A64_SINT, {32, 4}},
        {VK_FORMAT_R64G64B64A64_SFLOAT, {32, 4}},
        {VK_FORMAT_B10G11R11_UFLOAT_PACK32, {4, 3}},
        {VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, {4, 3}},
        {VK_FORMAT_D16_UNORM, {2, 1}},
        {VK_FORMAT_X8_D24_UNORM_PACK32, {4, 1}},
        {VK_FORMAT_D32_SFLOAT, {4, 1}},
        {VK_FORMAT_S8_UINT, {1, 1}},
        {VK_FORMAT_D16_UNORM_S8_UINT, {3, 2}},
        {VK_FORMAT_D24_UNORM_S8_UINT, {4, 2}},
        {VK_FORMAT_D32_SFLOAT_S8_UINT, {8, 2}},
        {VK_FORMAT_BC1_RGB_UNORM_BLOCK, {8, 4}},
        {VK_FORMAT_BC1_RGB_SRGB_BLOCK, {8, 4}},
        {VK_FORMAT_BC1_RGBA_UNORM_BLOCK, {8, 4}},
        {VK_FORMAT_BC1_RGBA_SRGB_BLOCK, {8, 4}},
        {VK_FORMAT_BC2_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_BC2_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_BC3_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_BC3_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_BC4_UNORM_BLOCK, {8, 4}},
        {VK_FORMAT_BC4_SNORM_BLOCK, {8, 4}},
        {VK_FORMAT_BC5_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_BC5_SNORM_BLOCK, {16, 4}},
        {VK_FORMAT_BC6H_UFLOAT_BLOCK, {16, 4}},
        {VK_FORMAT_BC6H_SFLOAT_BLOCK, {16, 4}},
        {VK_FORMAT_BC7_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_BC7_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK, {8, 3}},
        {VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK, {8, 3}},
        {VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK, {8, 4}},
        {VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK, {8, 4}},
        {VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_EAC_R11_UNORM_BLOCK, {8, 1}},
        {VK_FORMAT_EAC_R11_SNORM_BLOCK, {8, 1}},
        {VK_FORMAT_EAC_R11G11_UNORM_BLOCK, {16, 2}},
        {VK_FORMAT_EAC_R11G11_SNORM_BLOCK, {16, 2}},
        {VK_FORMAT_ASTC_4x4_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_4x4_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_5x4_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_5x4_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_5x5_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_5x5_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_6x5_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_6x5_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_6x6_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_6x6_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_8x5_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_8x5_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_8x6_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_8x6_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_8x8_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_8x8_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_10x5_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_10x5_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_10x6_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_10x6_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_10x8_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_10x8_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_10x10_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_10x10_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_12x10_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_12x10_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_12x12_UNORM_BLOCK, {16, 4}},
        {VK_FORMAT_ASTC_12x12_SRGB_BLOCK, {16, 4}},
        {VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG, {8, 4}},
        {VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG, {8, 4}},
        {VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG, {8, 4}},
        {VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG, {8, 4}},
        {VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG, {8, 4}},
        {VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG, {8, 4}},
        {VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG, {8, 4}},
        {VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG, {8, 4}},
    };

    return vk_format_info.at(format);
}

}  // namespace utils
}  // namespace goma
