#include "renderer/vulkan/context.hpp"

#include "renderer/vulkan/device.hpp"
#include "renderer/vulkan/image.hpp"
#include "renderer/vulkan/utils.hpp"

namespace goma {

CommandBufferManager::CommandBufferManager(Device& device) : device_(device) {}

CommandBufferManager::~CommandBufferManager() {
    for (auto& pool : pools_) {
        vkDestroyCommandPool(device_.GetHandle(), pool.second.pool, nullptr);
    }
}

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

    return outcome::success();
}

void Context::End() {
    assert(active_cmd_buf_ != VK_NULL_HANDLE &&
           "Context is not in a recording state");

    vkEndCommandBuffer(active_cmd_buf_);
    submission_queue_.push_back(active_cmd_buf_);
    active_cmd_buf_ = VK_NULL_HANDLE;
}

GraphicsContext::GraphicsContext(Device& device) : Context(device) {}

GraphicsContext::~GraphicsContext() {
    for (auto& render_pass : render_passes_) {
        vkDestroyPipeline(device_.GetHandle(), render_pass, nullptr);
    }
    render_passes_.clear();
}

result<void> GraphicsContext::BindFramebuffer(FramebufferDesc& desc) {
    assert(active_cmd_buf_ != VK_NULL_HANDLE &&
           "Context is not in a recording state");

    if (desc.render_pass_ == VK_NULL_HANDLE) {
        OUTCOME_TRY(render_pass,
                    utils::CreateRenderPass(device_.GetHandle(), desc));
        desc.render_pass_ = render_pass;
        render_passes_.push_back(render_pass);
    }

    std::vector<VkImageView> fb_attachments;
    VkExtent3D fb_size = {};

    for (const auto& c : desc.color_attachments) {
        fb_attachments.push_back(c.image->GetViewHandle());

        if (c.resolve_to) {
            fb_attachments.push_back(c.resolve_to->GetViewHandle());
        }

        auto image_size = c.image->GetSize();
        if (fb_size.width == 0) {
            fb_size = image_size;
        } else if (fb_size.width != image_size.width ||
                   fb_size.height != image_size.height) {
            return Error::DimensionsNotMatching;
        }
    }

    auto& d = desc.depth_attachment;

    if (d.image) {
        fb_attachments.push_back(d.image->GetViewHandle());

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

    VkFramebufferCreateInfo fb_info = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fb_info.renderPass = desc.render_pass_;
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
    rp_begin_info.renderPass = desc.render_pass_;
    rp_begin_info.framebuffer = framebuffer;
    rp_begin_info.clearValueCount = 0;
    rp_begin_info.pClearValues = {};

    vkCmdBeginRenderPass(active_cmd_buf_, &rp_begin_info,
                         VK_SUBPASS_CONTENTS_INLINE);

    return outcome::success();
}

void GraphicsContext::SetVertexBuffer(Buffer& buffer, VkDeviceSize offset) {
    assert(active_cmd_buf_ != VK_NULL_HANDLE &&
           "Context is not in a recording state");

    VkBuffer handle = buffer.GetHandle();
    vkCmdBindVertexBuffers(active_cmd_buf_, 0, 1, &handle, &offset);
}

void GraphicsContext::SetIndexBuffer(Buffer& buffer, VkDeviceSize offset,
                                     VkIndexType index_type) {
    assert(active_cmd_buf_ != VK_NULL_HANDLE &&
           "Context is not in a recording state");

    VkBuffer handle = buffer.GetHandle();
    vkCmdBindIndexBuffer(active_cmd_buf_, handle, offset, index_type);
}

void GraphicsContext::Draw() {
    assert(active_cmd_buf_ != VK_NULL_HANDLE &&
           "Context is not in a recording state");

    vkCmdDraw(active_cmd_buf_, 3, 1, 0, 0);
}

}  // namespace goma
