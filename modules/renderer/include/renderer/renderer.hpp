#pragma once

#include "common/include.hpp"
#include "common/hash.hpp"
#include "renderer/rendering_pipeline.hpp"
#include "rhi/device.hpp"

namespace goma {

class Engine;

class Renderer {
  public:
    Renderer(Engine& engine);
    ~Renderer();

    result<void> Render();

    Engine& engine() { return engine_; }
    Device& device() { return device_; }

    uint32_t max_frames_in_flight() { return kMaxFramesInFlight; }
    uint32_t frame_index() { return frame_index_; }
    uint32_t current_frame() { return current_frame_; }

    ctpl::thread_pool& thread_pool() { return thread_pool_; }

  private:
    Engine& engine_;
    Device device_;
    std::unique_ptr<RenderingPipeline> rp_;

    GraphicsContext graphics_ctx_;
    UploadContext upload_ctx_;

    const uint32_t kMaxFramesInFlight = 3;
    uint32_t frame_index_ = 0;
    uint32_t current_frame_ = 0;

    bool needs_upload_ = true;

    using FrameReceipts = std::vector<ReceiptPtr>;
    std::vector<FrameReceipts> frame_receipts_;

    const int kNumThreads = 8;
    ctpl::thread_pool thread_pool_;
};

}  // namespace goma
