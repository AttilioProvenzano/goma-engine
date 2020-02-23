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

struct Descriptor {
    VkDescriptorType type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
    Buffer* buffer = nullptr;
    uint32_t buf_offset = 0;
    uint32_t buf_range = 0;
    Image* image = nullptr;

    Descriptor();
    Descriptor(Buffer& b);
    Descriptor(Buffer& b, uint32_t offset, uint32_t range);
    Descriptor(Image& i);
};

using DescriptorSet = std::unordered_map<uint32_t, Descriptor>;

class DescriptorSetManager {
  public:
    DescriptorSetManager(Device& device);
    ~DescriptorSetManager();

    void Reset();
    VkDescriptorSet RequestDescriptorSet(const Pipeline& pipeline,
                                         const DescriptorSet& set = {});

  private:
    Device& device_;

    struct Pool {
        VkDescriptorPool pool = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> sets;
    };

    struct PoolSet {
        std::vector<Pool> pools;
        size_t first_available_pool = 0;
        size_t first_available_set = 0;
    };

    size_t pool_size_ = 256;

    Pool* FindOrCreatePool(PoolSet& pool_set, const Pipeline& pipeline);

    std::unordered_map<VkDescriptorSetLayout, PoolSet> pool_sets_;
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
    virtual void NextFrame();
    // void ResourceBarrier(BarrierDescription);

    std::vector<VkCommandBuffer> PopQueuedCommands();

  protected:
    Device& device_;
    VkCommandBuffer active_cmd_buf_ = VK_NULL_HANDLE;
    uint8_t current_frame_ = 0;

    static constexpr int kFrameCount = 3;
    std::vector<CommandBufferManager> cmd_buf_managers_;
    std::vector<DescriptorSetManager> desc_set_managers_;

    std::vector<VkCommandBuffer> submission_queue_;
};

class GraphicsContext : public Context {
  public:
    GraphicsContext(Device& device);
    ~GraphicsContext();

    // void BeginParallel();
    virtual void End() override;
    virtual void NextFrame() override;

    result<void> GraphicsContext::BindFramebuffer(FramebufferDesc&);
    void SetViewport(VkViewport viewport);
    void SetScissor(VkRect2D scissor);

    void BindVertexBuffer(Buffer&, VkDeviceSize offset = 0);
    void BindIndexBuffer(Buffer&, VkDeviceSize offset = 0,
                         VkIndexType index_type = VK_INDEX_TYPE_UINT32);
    void BindDescriptorSet(const DescriptorSet& = {});

    void BindGraphicsPipeline(Pipeline&);
    void BindComputePipeline(Pipeline&);

    void Draw(uint32_t vertex_count, uint32_t instance_count = 1,
              uint32_t first_vertex = 0, uint32_t first_instance = 0);
    void DrawIndexed(uint32_t index_count, uint32_t instance_count = 1,
                     uint32_t first_index = 0, uint32_t vertex_offset = 0,
                     uint32_t first_instance = 0);

  private:
    std::vector<VkRenderPass> render_passes_;

    std::unordered_map<size_t, std::vector<VkFramebuffer>> framebuffer_map_;
    FramebufferDesc current_fb_;

    Pipeline* current_pipeline_ = nullptr;
};

class ComputeContext : public Context {
    void BindComputePipeline(Pipeline&);
    void Dispatch();
};

struct BufferData {
    size_t size;
    const void* data;
    size_t offset = 0;
};

struct ImageData {
    using MipLevel = uint32_t;
    std::map<MipLevel, const void*> mip_data;
};

struct ImageArrayData {
    using ArrayLayer = uint32_t;
    std::map<ArrayLayer, ImageData> array_data;
};

class UploadContext : public Context {
  public:
    UploadContext(Device& device);
    ~UploadContext();

    result<void> UploadBuffer(Buffer&, BufferData);
    result<void> UploadImage(Image&, ImageData);
    result<void> UploadImageArray(Image&, ImageArrayData);

    void GenerateMipmaps(Image&);

  private:
    std::unordered_map<size_t, std::vector<Buffer*>> staging_buffers_;
};

}  // namespace goma
