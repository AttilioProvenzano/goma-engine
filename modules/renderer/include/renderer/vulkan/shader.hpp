#pragma once

#include "common/include.hpp"
#include "common/vulkan.hpp"

namespace goma {

struct ShaderInput {
    std::string name;
    uint32_t location;
    uint32_t vecsize;
};
using ShaderInputs = std::vector<ShaderInput>;

struct ShaderDesc {
    std::string name;
    VkShaderStageFlagBits stage;
    std::string source;
    std::string preamble;
};

class Shader {
  public:
    Shader(const ShaderDesc&);

    const std::string& GetName();
    VkShaderStageFlagBits GetStage();
    const std::string& GetSource();
    const std::string& GetPreamble();

    void ClearSource();
    void ClearPreamble();

    void SetHandle(VkShaderModule);
    VkShaderModule GetHandle();

    void SetInputs(ShaderInputs);
    const ShaderInputs& GetInputs();

  private:
    ShaderDesc desc_;

    ShaderInputs inputs_ = {};

    struct {
        VkShaderModule shader = VK_NULL_HANDLE;
    } api_handles_;
};

}  // namespace goma
