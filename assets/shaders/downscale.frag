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

    vec4 sum = texture(baseTex, inUVs) * 4.0;
    sum += texture(baseTex, inUVs - hp.xy);
    sum += texture(baseTex, inUVs + hp.xy);
    sum += texture(baseTex, inUVs + vec2(hp.x, -hp.y));
    sum += texture(baseTex, inUVs - vec2(hp.x, -hp.y));

    outColor = sum / 8.0;
}
