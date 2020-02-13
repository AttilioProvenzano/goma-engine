#pragma once

#include "common/include.hpp"
#include "common/vulkan.hpp"

namespace goma {

class Shader;

struct PipelineDesc {
    std::vector<Shader*> shaders;

    VkPrimitiveTopology primitive_topology =
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkCullModeFlagBits cull_mode = VK_CULL_MODE_NONE;
    VkFrontFace front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;

    bool depth_test = false;
    bool depth_write = false;
    VkCompareOp depth_compare_op = VK_COMPARE_OP_LESS;

    bool stencil_test = false;
    VkStencilOpState stencil_front_op = {};
    VkStencilOpState stencil_back_op = {};

    bool color_blend = false;
    std::vector<VkPipelineColorBlendAttachmentState> blend_attachments = {};

    bool suppress_fragment = false;
};

class Pipeline {
  public:
    Pipeline(const PipelineDesc&);

    void SetHandle(VkPipeline);
    VkPipeline GetHandle();

  private:
    PipelineDesc desc_;

    struct {
        VkPipeline pipeline = VK_NULL_HANDLE;
    } api_handles_;
};

}  // namespace goma
