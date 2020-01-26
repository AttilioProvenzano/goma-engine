#pragma once

#include "common/include.hpp"
#include "common/vulkan.hpp"

namespace goma {

class Device;

class CommandBufferManager {
  public:
    CommandBufferManager(Device& device);

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
    result<void> GraphicsContext::BindFramebuffer()
    // void BeginParallel();
    /*
void SetPipeline(Pipeline);
void SetVertexBuffer(Buffer);
void SetIndexBuffer(Buffer);
void Draw(...);
*/
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
