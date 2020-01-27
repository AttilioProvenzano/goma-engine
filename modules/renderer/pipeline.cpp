#include "renderer/vulkan/pipeline.hpp"

namespace goma {

Pipeline::Pipeline(const PipelineDesc& pipeline_desc) : desc_(pipeline_desc) {}

void Pipeline::SetHandle(VkPipeline pipeline) {
    api_handles_.pipeline = pipeline;
}
VkPipeline Pipeline::GetHandle() { return api_handles_.pipeline; }

}  // namespace goma
