#include "rhi/vulkan/utils.hpp"

#include "rhi/vulkan/context.hpp"
#include "rhi/vulkan/image.hpp"

namespace goma {
namespace utils {

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

}  // namespace utils
}  // namespace goma
