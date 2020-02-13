#pragma once

#include "common/include.hpp"
#include "common/vulkan.hpp"

namespace goma {

class Device;
class Buffer;
class Image;
class Pipeline;

class CommandBufferManager {
  public:
    CommandBufferManager(Device& device);
    ~CommandBufferManager();

    void Reset();
    VkCommandBuffer RequestPrimary(size_t thread_id = 0);
    VkCommandBuffer RequestSecondary(size_t thread_id = 0);

  private:
    Device& device_;

    struct Pool {
        VkCommandPool pool = VK_NULL_HANDLE;

        std::vector<VkCommandBuffer> primary;
        size_t active_primary_count = 0;

        std::vector<VkCommandBuffer> secondary;
        size_t active_secondary_count = 0;
    };

    Pool& FindOrCreatePool(size_t thread_id);
    std::unordered_map<size_t, Pool> pools_;
};

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

    VkRenderPass render_pass_ = VK_NULL_HANDLE;  // for internal use
};

class Context {
  public:
    Context(Device& device);
    virtual ~Context() = default;

    result<void> Begin();
    virtual void End();
    void NextFrame();
    // void ResourceBarrier(BarrierDescription);

    std::vector<VkCommandBuffer> PopQueuedCommands();

  protected:
    Device& device_;
    VkCommandBuffer active_cmd_buf_ = VK_NULL_HANDLE;
    uint8_t current_frame_ = 0;

    static constexpr int kFrameCount = 3;
    std::vector<CommandBufferManager> cmd_buf_managers_;

    std::vector<VkCommandBuffer> submission_queue_;
};

class GraphicsContext : public Context {
  public:
    GraphicsContext(Device& device);
    ~GraphicsContext();

    virtual void End() override;

    result<void> GraphicsContext::BindFramebuffer(FramebufferDesc&);
    void SetViewport(VkViewport viewport);
    void SetScissor(VkRect2D scissor);

    void BindVertexBuffer(Buffer&, VkDeviceSize offset = 0);
    void BindIndexBuffer(Buffer&, VkDeviceSize offset = 0,
                         VkIndexType index_type = VK_INDEX_TYPE_UINT32);
    void BindGraphicsPipeline(Pipeline&);
    void BindComputePipeline(Pipeline&);

    void Draw(uint32_t vertex_count, uint32_t instance_count = 1,
              uint32_t first_vertex = 0, uint32_t first_instance = 0);
    void DrawIndexed(uint32_t index_count, uint32_t instance_count = 1,
                     uint32_t first_index = 0, uint32_t vertex_offset = 0,
                     uint32_t first_instance = 0);

    // void BeginParallel();
  private:
    std::vector<VkRenderPass> render_passes_;
    FramebufferDesc current_fb_;
};

class ComputeContext : public Context {
    // void SetPipeline(Pipeline);
    void Dispatch();
};

struct BufferData {
    size_t size;
    const void* data;
    size_t offset = 0;
};

struct ImageData {
    const void* data;
};

class UploadContext : public Context {
  public:
    UploadContext(Device& device);
    ~UploadContext();

    result<void> UploadBuffer(Buffer&, BufferData);
    result<void> UploadImage(Image&, ImageData);

  private:
    std::unordered_map<size_t, std::vector<Buffer*>> staging_buffers_;
};

}  // namespace goma
