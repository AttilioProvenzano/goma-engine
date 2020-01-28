#pragma once

#include "common/include.hpp"
#include "common/vulkan.hpp"

namespace goma {

class Device;
class Buffer;
class Image;

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

    result<void> Begin();
    void End();
    void NextFrame();
    // void ResourceBarrier(BarrierDescription);

  protected:
    Device& device_;
    VkCommandBuffer active_cmd_buf_ = VK_NULL_HANDLE;

  private:
    uint8_t current_frame_ = 0;

    static constexpr int kFrameCount = 3;
    std::vector<CommandBufferManager> cmd_buf_managers_;

    std::vector<VkCommandBuffer> submission_queue_;
};

class GraphicsContext : public Context {
  public:
    GraphicsContext(Device& device);
    ~GraphicsContext();

    result<void> GraphicsContext::BindFramebuffer(FramebufferDesc&);
    void SetVertexBuffer(Buffer&, VkDeviceSize offset = 0);
    void SetIndexBuffer(Buffer&, VkDeviceSize offset = 0,
                        VkIndexType index_type = VK_INDEX_TYPE_UINT32);
    void Draw();

    // void BeginParallel();
    /*
void SetPipeline(Pipeline);
void SetVertexBuffer(Buffer);
void SetIndexBuffer(Buffer);
void Draw(...);
*/
  private:
    std::vector<VkRenderPass> render_passes_;
};

class ComputeContext : public Context {
    // void SetPipeline(Pipeline);
    void Dispatch();
};

class UploadContext : public Context {
    // void UploadBuffer(Buffer, Data);
    // void UploadTexture(Texture, Data);
};

}  // namespace goma
