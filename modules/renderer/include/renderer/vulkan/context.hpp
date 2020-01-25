#pragma once

#include "common/include.hpp"
#include "common/vulkan.hpp"

namespace goma {

class Context {
    void Begin();
    void End();
    void ResourceBarrier(BarrierDescription);
    (...)

        CommandBuffer mCommandBuffer;
};

class GraphicsContext : public Context {
    void SetPipeline(Pipeline);
    void SetVertexBuffer(Buffer);
    void SetIndexBuffer(Buffer);
    void Draw(...);
    (...)
};

class ComputeContext : public Context {
    void SetPipeline(Pipeline);
    void Dispatch(...);
    (...)
};

class UploadContext : public Context {
    void UploadBuffer(Buffer, Data);
    void UploadTexture(Texture, Data);
    (...)
};

}  // namespace goma