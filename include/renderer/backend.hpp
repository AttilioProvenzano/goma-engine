#pragma once

#include "renderer/handles.hpp"
#include "platform/platform.hpp"

#include <outcome.hpp>
namespace outcome = OUTCOME_V2_NAMESPACE;
using outcome::result;
#include <vector>

namespace goma {

class Engine;

class Backend {
  public:
    Backend(Engine* engine = nullptr) : engine_(engine) {}
    virtual ~Backend() = default;

    virtual result<void> InitContext() = 0;
    virtual result<void> InitSurface(Platform* platform) = 0;
    virtual result<Pipeline> GetGraphicsPipeline(
        const char* vs_source, const char* fs_source,
        const char* vs_entry_point = "main",
        const char* fs_entry_point = "main") = 0;

    virtual result<Image> CreateTexture(const char* name,
                                        TextureDesc texture_desc,
                                        void* initial_contents = nullptr) = 0;
    virtual result<Image> GetTexture(const char* name) = 0;
    virtual result<Framebuffer> CreateFramebuffer(uint32_t frame_index,
                                                  const char* name,
                                                  FramebufferDesc fb_desc) = 0;

    virtual result<void> TeardownContext() = 0;

  protected:
    Engine* engine_ = nullptr;
};

}  // namespace goma
