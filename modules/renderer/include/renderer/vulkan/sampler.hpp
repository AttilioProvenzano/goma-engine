#pragma once

#include "common/include.hpp"
#include "common/vulkan.hpp"

namespace goma {

struct SamplerDesc {
    VkSamplerAddressMode address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkFilter mag_filter = VK_FILTER_LINEAR;
    VkFilter min_filter : VK_FILTER_LINEAR;
    float max_anisotropy = 1.0f;
    VkBorderColor border_color = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    float min_lod = 0.0f;
    float max_lod = std::numeric_limits<float>::max();
};

class Sampler {
  public:
    Sampler(const SamplerDesc&);

    void SetHandle(VkSampler);
    VkSampler GetHandle();

  private:
    SamplerDesc desc_;

    struct {
        VkSampler sampler = VK_NULL_HANDLE;
    } api_handles_;
};

}  // namma