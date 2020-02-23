#include "renderer/vulkan/pipeline.hpp"

#include "renderer/vulkan/context.hpp"

bool operator==(const VkPipelineColorBlendAttachmentState& lhs,
                const VkPipelineColorBlendAttachmentState& rhs) {
    return !memcmp(&lhs, &rhs, sizeof(lhs));
}

bool operator==(const VkStencilOpState& lhs, const VkStencilOpState& rhs) {
    return !memcmp(&lhs, &rhs, sizeof(lhs));
}

namespace goma {

bool PipelineDesc::operator==(const goma::PipelineDesc& rhs) const {
    const auto& lhs = *this;
    return lhs.shaders == rhs.shaders &&
           IsCompatible(lhs.fb_desc, rhs.fb_desc) &&
           lhs.primitive_topology == rhs.primitive_topology &&
           lhs.cull_mode == rhs.cull_mode && lhs.front_face == rhs.front_face &&
           lhs.sample_count == rhs.sample_count &&
           lhs.depth_test == rhs.depth_test &&
           lhs.depth_write == rhs.depth_write &&
           lhs.depth_compare_op == rhs.depth_compare_op &&
           lhs.stencil_test == rhs.stencil_test &&
           lhs.stencil_front_op == rhs.stencil_front_op &&
           lhs.stencil_back_op == rhs.stencil_back_op &&
           lhs.color_blend == rhs.color_blend &&
           lhs.blend_attachments == rhs.blend_attachments &&
           lhs.suppress_fragment == rhs.suppress_fragment;
}

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
