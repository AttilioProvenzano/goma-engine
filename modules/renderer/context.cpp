#include "renderer/vulkan/context.hpp"

#include "renderer/vulkan/device.hpp"
#include "renderer/vulkan/image.hpp"

namespace goma {

CommandBufferManager::CommandBufferManager(Device& device) : device_(device) {}

void CommandBufferManager::Reset() {
    for (auto& pool : pools_) {
        pool.second.active_primary_count = 0;
        pool.second.active_secondary_count = 0;
    }
}

CommandBufferManager::Pool& CommandBufferManager::FindOrCreatePool(
    size_t thread_id) {
    auto res = pools_.find(thread_id);

    if (res == pools_.end()) {
        VkCommandPoolCreateInfo pool_info = {
            VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        pool_info.queueFamilyIndex = device_.GetQueueFamilyIndex();

        VkCommandPool pool = VK_NULL_HANDLE;
        vkCreateCommandPool(device_.GetHandle(), &pool_info, nullptr, &pool);

        auto new_pool = pools_.emplace(thread_id, Pool{pool});
        res = new_pool.first;
    }

    return res->second;
}

VkCommandBuffer CommandBufferManager::RequestPrimary(size_t thread_id) {
    auto& pool = FindOrCreatePool(thread_id);
    if (pool.active_primary_count < pool.primary.size()) {
        return pool.primary[pool.active_primary_count++];
    }

    VkCommandBufferAllocateInfo cmd_buf_info = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cmd_buf_info.commandBufferCount = 1;
    cmd_buf_info.commandPool = pool.pool;
    cmd_buf_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    VkCommandBuffer cmd_buf = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(device_.GetHandle(), &cmd_buf_info, &cmd_buf);

    pool.primary.push_back(cmd_buf);
    pool.active_primary_count++;

    return cmd_buf;
}

VkCommandBuffer CommandBufferManager::RequestSecondary(size_t thread_id) {
    auto& pool = FindOrCreatePool(thread_id);
    if (pool.active_secondary_count < pool.secondary.size()) {
        return pool.secondary[pool.active_secondary_count++];
    }

    VkCommandBufferAllocateInfo cmd_buf_info = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cmd_buf_info.commandBufferCount = 1;
    cmd_buf_info.commandPool = pool.pool;
    cmd_buf_info.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;

    VkCommandBuffer cmd_buf = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(device_.GetHandle(), &cmd_buf_info, &cmd_buf);

    pool.secondary.push_back(cmd_buf);
    pool.active_secondary_count++;

    return cmd_buf;
}

Context::Context(Device& device)
    : device_(device), cmd_buf_managers_(kFrameCount, {device}) {}

void Context::NextFrame() {
    current_frame_ = (current_frame_ + 1) % kFrameCount;
}

result<void> Context::Begin() {
    assert(active_cmd_buf_ == VK_NULL_HANDLE &&
           "Context is already in a recording state");

    auto& cmd_buf_manager = cmd_buf_managers_[current_frame_];
    auto cmd_buf = cmd_buf_manager.RequestPrimary();

    VkCommandBufferBeginInfo begin_info = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(cmd_buf, &begin_info));
    active_cmd_buf_ = cmd_buf;
}

void Context::End() {
    assert(active_cmd_buf_ != VK_NULL_HANDLE &&
           "Context is not in a recording state");

    vkEndCommandBuffer(active_cmd_buf_);
    submission_queue_.push_back(active_cmd_buf_);
    active_cmd_buf_ = VK_NULL_HANDLE;
}

struct FramebufferDesc {
    struct Attachment {
        Image* image = nullptr;
        VkAttachmentLoadOp load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        VkAttachmentStoreOp store_op = VK_ATTACHMENT_STORE_OP_STORE;
        VkClearValue clear_value = {0.0f, 0.0f, 0.0f, 1.0f};
        Image* resolve_to = nullptr;
    };

    std::vector<Attachment> color_attachments;
    Attachment depth_attachment;
};

result<void> GraphicsContext::BindFramebuffer(FramebufferDesc desc) {
    assert(active_cmd_buf_ != VK_NULL_HANDLE &&
           "Context is not in a recording state");

    std::vector<VkAttachmentDescription> attachments;
    std::vector<VkAttachmentReference> color_attachments;
    std::vector<VkAttachmentReference> resolve_attachments;
    std::vector<VkClearValue> clear_values;

    std::vector<VkImageView> fb_attachments;
    VkExtent3D fb_size = {};

    for (const auto& c : desc.color_attachments) {
        VkAttachmentDescription attachment = {};
        attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachment.format = c.image->GetFormat();
        attachment.loadOp = c.load_op;
        attachment.storeOp = c.store_op;
        attachment.samples = c.image->GetSampleCount();

        attachments.push_back(attachment);
        fb_attachments.push_back(c.image->GetViewHandle());
        clear_values.push_back(c.clear_value);

        VkAttachmentReference attachment_ref = {};
        attachment_ref.attachment = attachments.size() - 1;
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
            fb_attachments.push_back(c.resolve_to->GetViewHandle());
            clear_values.push_back(c.clear_value);

            VkAttachmentReference attachment_ref = {};
            attachment_ref.attachment = attachments.size() - 1;
            attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            resolve_attachments.push_back(attachment_ref);
        }

        auto image_size = c.image->GetSize();
        if (fb_size.width == 0) {
            fb_size = image_size;
        } else if (fb_size.width != image_size.width ||
                   fb_size.height != image_size.height) {
            return Error::DimensionsNotMatching;
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
        fb_attachments.push_back(d.image->GetViewHandle());
        clear_values.push_back(d.clear_value);

        depth_attachment.attachment = attachments.size() - 1;
        depth_attachment.layout =
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // Note: we still check fb_size for the depth image as
        // there could be a depth-only pass
        auto image_size = d.image->GetSize();
        if (fb_size.width == 0) {
            fb_size = image_size;
        } else if (fb_size.width != image_size.width ||
                   fb_size.height != image_size.height) {
            return Error::DimensionsNotMatching;
        }
    }

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = color_attachments.size();
    subpass.pColorAttachments = color_attachments.data();

    if (depth_image) {
        subpass.pDepthStencilAttachment = &depth_attachment;
    }

    if (!resolve_attachments.empty()) {
        subpass.pResolveAttachments = resolve_attachments.data();
    }

    VkRenderPassCreateInfo rp_info = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rp_info.attachmentCount = attachments.size();
    rp_info.pAttachments = attachments.data();
    rp_info.subpassCount = 1;
    rp_info.pSubpasses = &subpass;

    VkRenderPass render_pass = VK_NULL_HANDLE;
    VK_CHECK(vkCreateRenderPass(device_.GetHandle(), &rp_info, nullptr,
                                &render_pass));

    VkFramebufferCreateInfo fb_info = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fb_info.renderPass = render_pass;
    fb_info.width = fb_size.width;
    fb_info.height = fb_size.height;
    fb_info.attachmentCount = fb_attachments.size();
    fb_info.pAttachments = fb_attachments.data();
    fb_info.layers = 1;

    // TODO: Store them for cleanup
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VK_CHECK(vkCreateFramebuffer(device_.GetHandle(), &fb_info, nullptr,
                                 &framebuffer));

    VkRenderPassBeginInfo rp_begin_info = {
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rp_begin_info.renderArea = {VkOffset2D{}, {fb_size.width, fb_size.height}};
    rp_begin_info.renderPass = render_pass;
    rp_begin_info.framebuffer = framebuffer;
    rp_begin_info.clearValueCount = 0;
    rp_begin_info.pClearValues = {};

    vkCmdBeginRenderPass(active_cmd_buf_, &rp_begin_info,
                         VK_SUBPASS_CONTENTS_INLINE);
}

}  // namespace goma
