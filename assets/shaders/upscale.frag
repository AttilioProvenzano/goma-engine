#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform sampler2D baseTex;

layout(location = 0) in vec2 inUVs;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1, std140) uniform UBO {
    vec2 halfPixel;
} ubo;

void main() {
    vec2 hp = ubo.halfPixel;

    vec4 sum = texture(baseTex, inUVs + vec2(-hp.x * 2.0, 0.0));
    sum += texture(baseTex, inUVs + vec2(-hp.x, hp.y)) * 2.0;
    sum += texture(baseTex, inUVs + vec2(0.0, hp.y * 2.0));
    sum += texture(baseTex, inUVs + vec2(hp.x, hp.y)) * 2.0;
    sum += texture(baseTex, inUVs + vec2(hp.x * 2.0, 0.0));
    sum += texture(baseTex, inUVs + vec2(hp.x, -hp.y)) * 2.0;
    sum += texture(baseTex, inUVs + vec2(0.0, -hp.y * 2.0));
    sum += texture(baseTex, inUVs + vec2(-hp.x, -hp.y)) * 2.0;

    outColor = sum / 12.0;
}
