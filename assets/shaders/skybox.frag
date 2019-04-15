#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform samplerCube diffuseTex;

layout(location = 0) in vec3 inUVW;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(diffuseTex, inUVW);
}
