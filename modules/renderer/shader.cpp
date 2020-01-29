#include "renderer/vulkan/shader.hpp"

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

void Shader::SetInputs(ShaderInputs si) { inputs_ = std::move(si); }
const ShaderInputs& Shader::GetInputs() { return inputs_; }

}  // namespace goma
