#pragma once

#include <spirv_glsl.hpp>

#include "common/include.hpp"
#include "common/vulkan.hpp"

namespace goma {

struct ShaderDesc {
    std::string name;
    VkShaderStageFlagBits stage;
    std::string source;
    std::string preamble;
    spirv_cross::ShaderResources resources = {};
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

    void SetResources(spirv_cross::ShaderResources);
    const spirv_cross::ShaderResources& GetResources();

  private:
    ShaderDesc desc_;

    struct {
        VkShaderModule shader = VK_NULL_HANDLE;
    } api_handles_;
};

}  // namespace goma
