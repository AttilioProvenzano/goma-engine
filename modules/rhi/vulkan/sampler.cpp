#include "rhi/vulkan/sampler.hpp"

namespace goma {

Sampler::Sampler(const SamplerDesc& sampler_desc) : desc_(sampler_desc) {}

void Sampler::SetHandle(VkSampler sampler) { api_handles_.sampler = sampler; }
VkSampler Sampler::GetHandle() { return api_handles_.sampler; }

}  // namespace goma
