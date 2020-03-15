#pragma once

#include "renderer/rendering_pipeline.hpp"

namespace goma {

class ForwardPipeline : public RenderingPipeline {
  public:
    ForwardPipeline(Renderer& renderer)
        : RenderingPipeline(renderer, {"color", "depth"}) {}

    virtual result<void> run(GfxCtx& ctx, BindingMap outputs,
                             BindingMap inputs) override;
};

}  // namespace goma
