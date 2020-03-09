#pragma once

#include "common/include.hpp"
#include "common/vulkan.hpp"

namespace goma {

struct ShaderInput {
    std::string name;
    uint32_t location;
    VkFormat format;
};
using ShaderInputs = std::vector<ShaderInput>;

struct ShaderBinding {
    std::string name;
    VkDescriptorType type;
};
using ShaderBindings = std::unordered_map<uint32_t, ShaderBinding>;

struct ShaderDesc {
    std::string name;
    VkShaderStageFlagBits stage;
    std::string source;
    std::string preamble;

    bool operator==(const ShaderDesc& rhs) const;
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

    void SetBindings(ShaderBindings);
    const ShaderBindings& GetBindings();

  private:
    ShaderDesc desc_;

    ShaderInputs inputs_ = {};
    ShaderBindings bindings_ = {};

    struct {
        VkShaderModule shader = VK_NULL_HANDLE;
    } api_handles_;
};

}  // namespace goma
