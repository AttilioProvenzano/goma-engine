#include "renderer/pipelines/forward_pipeline.hpp"

#include "engine/engine.hpp"
#include "renderer/renderer.hpp"

namespace goma {

result<void> ForwardPipeline::run(GfxCtx& ctx, BindingMap outputs,
                                  BindingMap inputs) {
    FramebufferDesc fb_desc;
    fb_desc.color_attachments = {{outputs.at("color")}};
    fb_desc.depth_attachment.image = outputs.at("depth");
    OUTCOME_TRY(ctx.BindFramebuffer(fb_desc));

    auto& meshes = renderer_.engine().scene()->meshes();
    OUTCOME_TRY(render_meshes(ctx, meshes.begin(), meshes.end()));

    return outcome::success();
}

}  // namespace goma
