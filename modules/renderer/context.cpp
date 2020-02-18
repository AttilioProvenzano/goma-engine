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

std::vector<VkCommandBuffer> Context::PopQueuedCommands() {
    assert(active_cmd_buf_ == VK_NULL_HANDLE &&
           "Context is in a recording state");

    auto ret = std::move(submission_queue_);
    submission_queue_.clear();
    return ret;
}

GraphicsContext::GraphicsContext(Device& device) : Context(device) {}

GraphicsContext::~GraphicsContext() {
    for (auto& framebuffer : framebuffers_) {
        vkDestroyFramebuffer(device_.GetHandle(), framebuffer, nullptr);
    }
    framebuffers_.clear();

    for (auto& render_pass : render_passes_) {
        vkDestroyRenderPass(device_.GetHandle(), render_pass, nullptr);
    }
    render_passes_.clear();
}

void GraphicsContext::End() {
    if (current_fb_.render_pass_) {
        vkCmdEndRenderPass(active_cmd_buf_);
    }
    current_fb_ = FramebufferDesc{};

    Context::End();
}

result<void> GraphicsContext::BindFramebuffer(FramebufferDesc& desc) {
    // TODO: Replace asserts with outcome::error
    assert(active_cmd_buf_ != VK_NULL_HANDLE &&
           "Context is not in a recording state");

    if (current_fb_.render_pass_) {
        // End the current render pass
        vkCmdEndRenderPass(active_cmd_buf_);
    }

    if (desc.render_pass_ == VK_NULL_HANDLE) {
        OUTCOME_TRY(render_pass,
                    utils::CreateRenderPass(device_.GetHandle(), desc));
        desc.render_pass_ = render_pass;
        render_passes_.push_back(render_pass);
    }
    current_fb_ = desc;

    std::vector<VkImageView> fb_attachments;
    VkExtent3D fb_size = {};

    for (const auto& c : desc.color_attachments) {
        assert(c.image && "Invalid image in color attachments");

        fb_attachments.push_back(c.image->GetView());

        if (c.resolve_to) {
            fb_attachments.push_back(c.resolve_to->GetView());
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
        fb_attachments.push_back(d.image->GetView());

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
    fb_info.attachmentCount = static_cast<uint32_t>(fb_attachments.size());
    fb_info.pAttachments = fb_attachments.data();
    fb_info.layers = 1;

    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VK_CHECK(vkCreateFramebuffer(device_.GetHandle(), &fb_info, nullptr,
                                 &framebuffer));
    framebuffers_.push_back(framebuffer);

    VkViewport viewport = {};
    viewport.width = static_cast<float>(fb_size.width);
    viewport.height = static_cast<float>(fb_size.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    SetViewport(std::move(viewport));

    VkRect2D scissor = {};
    scissor.extent = {fb_size.width, fb_size.height};
    SetScissor(std::move(scissor));

    // Note: if loadOp is LOAD_OP_LOAD, clearValue is ignored
    std::vector<VkClearValue> clear_values;
    std::vector<VkImageMemoryBarrier> image_barriers;

    for (const auto& attachment : desc.color_attachments) {
        clear_values.push_back(attachment.clear_value);

        VkImageMemoryBarrier image_barrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        image_barrier.srcAccessMask = 0;
        image_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        image_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        image_barrier.image = attachment.image->GetHandle();
        image_barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0,
                                          1};
        image_barriers.push_back(image_barrier);
    }

    if (desc.depth_attachment.image) {
        clear_values.push_back(desc.depth_attachment.clear_value);

        VkImageMemoryBarrier image_barrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        image_barrier.srcAccessMask = 0;
        image_barrier.dstAccessMask =
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        image_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_barrier.newLayout =
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        image_barrier.image = desc.depth_attachment.image->GetHandle();
        image_barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0,
                                          1};
        image_barriers.push_back(image_barrier);
    }

    vkCmdPipelineBarrier(active_cmd_buf_, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr,
                         static_cast<uint32_t>(image_barriers.size()),
                         image_barriers.data());

    VkRenderPassBeginInfo rp_begin_info = {
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rp_begin_info.renderArea = {VkOffset2D{}, {fb_size.width, fb_size.height}};
    rp_begin_info.renderPass = desc.render_pass_;
    rp_begin_info.framebuffer = framebuffer;
    rp_begin_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
    rp_begin_info.pClearValues = clear_values.data();

    vkCmdBeginRenderPass(active_cmd_buf_, &rp_begin_info,
                         VK_SUBPASS_CONTENTS_INLINE);

    return outcome::success();
}

void GraphicsContext::SetViewport(VkViewport viewport) {
    assert(active_cmd_buf_ != VK_NULL_HANDLE &&
           "Context is not in a recording state");

    vkCmdSetViewport(active_cmd_buf_, 0, 1, &viewport);
}

void GraphicsContext::SetScissor(VkRect2D scissor) {
    assert(active_cmd_buf_ != VK_NULL_HANDLE &&
           "Context is not in a recording state");

    vkCmdSetScissor(active_cmd_buf_, 0, 1, &scissor);
}

void GraphicsContext::BindVertexBuffer(Buffer& buffer, VkDeviceSize offset) {
    assert(active_cmd_buf_ != VK_NULL_HANDLE &&
           "Context is not in a recording state");

    VkBuffer handle = buffer.GetHandle();
    vkCmdBindVertexBuffers(active_cmd_buf_, 0, 1, &handle, &offset);
}

void GraphicsContext::BindIndexBuffer(Buffer& buffer, VkDeviceSize offset,
                                      VkIndexType index_type) {
    assert(active_cmd_buf_ != VK_NULL_HANDLE &&
           "Context is not in a recording state");

    VkBuffer handle = buffer.GetHandle();
    vkCmdBindIndexBuffer(active_cmd_buf_, handle, offset, index_type);
}

void GraphicsContext::BindDescriptorSet(const DescriptorSet& set) {
    for (const auto& descriptor : set) {
    }
}

void GraphicsContext::BindGraphicsPipeline(Pipeline& pipeline) {
    assert(active_cmd_buf_ != VK_NULL_HANDLE &&
           "Context is not in a recording state");

    VkPipeline handle = pipeline.GetHandle();
    vkCmdBindPipeline(active_cmd_buf_, VK_PIPELINE_BIND_POINT_GRAPHICS, handle);
}

void GraphicsContext::BindComputePipeline(Pipeline& pipeline) {
    assert(active_cmd_buf_ != VK_NULL_HANDLE &&
           "Context is not in a recording state");

    VkPipeline handle = pipeline.GetHandle();
    vkCmdBindPipeline(active_cmd_buf_, VK_PIPELINE_BIND_POINT_COMPUTE, handle);
}

void GraphicsContext::Draw(uint32_t vertex_count, uint32_t instance_count,
                           uint32_t first_vertex, uint32_t first_instance) {
    assert(active_cmd_buf_ != VK_NULL_HANDLE &&
           "Context is not in a recording state");

    vkCmdDraw(active_cmd_buf_, vertex_count, instance_count, first_vertex,
              first_instance);
}

void GraphicsContext::DrawIndexed(uint32_t index_count, uint32_t instance_count,
                                  uint32_t first_index, uint32_t vertex_offset,
                                  uint32_t first_instance) {
    assert(active_cmd_buf_ != VK_NULL_HANDLE &&
           "Context is not in a recording state");

    vkCmdDrawIndexed(active_cmd_buf_, index_count, instance_count, first_index,
                     vertex_offset, first_instance);
}

UploadContext::UploadContext(Device& device) : Context(device) {}

UploadContext::~UploadContext() {
    for (auto& staging_buffers_entry : staging_buffers_) {
        for (auto& staging_buffer : staging_buffers_entry.second) {
            // TODO: Ask the device to destroy the staging buffer
        }
    }
}

namespace {

result<void> CopyBufferData(Device& device, Buffer& buffer, BufferData data) {
    OUTCOME_TRY(mapped_data, device.MapBuffer(buffer));
    memcpy(static_cast<uint8_t*>(mapped_data) + data.offset, data.data,
           data.size);
    device.UnmapBuffer(buffer);

    return outcome::success();
}

}  // namespace

result<void> UploadContext::UploadBuffer(Buffer& buffer, BufferData data) {
    assert(active_cmd_buf_ != VK_NULL_HANDLE &&
           "Context is not in a recording state");

    if (buffer.GetStorage() == VMA_MEMORY_USAGE_GPU_ONLY) {
        // We need a staging buffer in this case
        BufferDesc staging_desc = {};
        staging_desc.size = data.size;
        staging_desc.storage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        staging_desc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        OUTCOME_TRY(staging_buffer, device_.CreateBuffer(staging_desc));
        OUTCOME_TRY(CopyBufferData(device_, *staging_buffer, data));

        VkBufferCopy region = {};
        region.dstOffset = data.offset;
        region.size = data.size;

        vkCmdCopyBuffer(active_cmd_buf_, staging_buffer->GetHandle(),
                        buffer.GetHandle(), 1, &region);
        staging_buffers_[current_frame_].push_back(staging_buffer);
    } else {
        // We can copy data directly
        OUTCOME_TRY(CopyBufferData(device_, buffer, data));
    }

    return outcome::success();
}

}  // namespace goma
