#include "rhi/vulkan/shader.hpp"

namespace goma {

Shader::Shader(const ShaderDesc& shader_desc) : desc_(shader_desc) {}

const std::string& Shader::GetName() { return desc_.name; }
VkShaderStageFlagBits Shader::GetStage() { return desc_.stage; }

const std::string& Shader::GetSource() { return desc_.source; }
const std::string& Shader::GetPreamble() { return desc_.preamble; }

void Shader::ClearSource() { desc_.source.clear(); }
void Shader::ClearPreamble() { desc_.preamble.clear(); }

void Shader::SetHandle(VkShaderModule shader) { api_handles_.shader = shader; }
VkShaderModule Shader::GetHandle() { return api_handles_.shader; }

void Shader::SetInputs(ShaderInputs si) {
    inputs_ = std::move(si);
    std::sort(inputs_.begin(), inputs_.end(), [](const auto& a, const auto& b) {
        return a.location < b.location;
    });
}
const ShaderInputs& Shader::GetInputs() { return inputs_; }

void Shader::SetBindings(ShaderBindings sb) { bindings_ = std::move(sb); }
const ShaderBindings& Shader::GetBindings() { return bindings_; }

bool ShaderDesc::operator==(const goma::ShaderDesc& rhs) const {
    const auto& lhs = *this;
    return lhs.name == rhs.name && lhs.stage == rhs.stage &&
           lhs.preamble == rhs.preamble &&
           // Compare sources if name is empty
           (!lhs.name.empty() || lhs.source == rhs.source);
}

}  // namespace goma
