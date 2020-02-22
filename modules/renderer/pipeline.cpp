#include "renderer/vulkan/pipeline.hpp"

namespace goma {

Pipeline::Pipeline(const PipelineDesc& pipeline_desc) : desc_(pipeline_desc) {}

void Pipeline::SetHandle(VkPipeline pipeline) {
    api_handles_.pipeline = pipeline;
}
VkPipeline Pipeline::GetHandle() const { return api_handles_.pipeline; }

void Pipeline::SetLayout(VkPipelineLayout layout) {
    api_handles_.layout = layout;
}
VkPipelineLayout Pipeline::GetLayout() const { return api_handles_.layout; }

void Pipeline::SetDescriptorSetLayout(VkDescriptorSetLayout dsl) {
    api_handles_.desc_set_layout = dsl;
}
VkDescriptorSetLayout Pipeline::GetDescriptorSetLayout() const {
    return api_handles_.desc_set_layout;
}

void Pipeline::SetBindings(PipelineBindings bindings) {
    api_handles_.bindings = std::move(bindings);
}
const PipelineBindings& Pipeline::GetBindings() const {
    return api_handles_.bindings;
}

}  // namespace goma
