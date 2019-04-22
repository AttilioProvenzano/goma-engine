#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform sampler2D baseTex;
layout(set = 0, binding = 1) uniform sampler2D blurTex;
layout(set = 0, binding = 2) uniform sampler2DMS depthTex;

layout(location = 0) in vec2 inUVs;
layout(location = 0) out vec4 outColor;

void main() {
    float depth = texelFetch(depthTex, ivec2(gl_FragCoord.xy), 0).x;
    if (depth > 0.9999)
        outColor = mix(texture(baseTex, inUVs), texture(blurTex, inUVs), depth);
    else
        outColor = texture(baseTex, inUVs);
}
