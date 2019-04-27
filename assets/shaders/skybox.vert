#version 450

layout(set = 0, binding = 12, std140) uniform SkyboxUBO {
    mat4 vp;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 0) out vec3 outUVW;

void main() {
    gl_Position = ubo.vp * vec4(inPosition, 0.0);
    outUVW = inPosition;
}
