#version 450

layout(push_constant) uniform PC {
	mat4 mvp;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 0) out vec3 outUVW;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 0.0);
	outUVW = inPosition;
}
