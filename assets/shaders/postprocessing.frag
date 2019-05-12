#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform sampler2D baseTex;
layout(set = 0, binding = 1) uniform sampler2D blurTex;
layout(set = 0, binding = 2) uniform sampler2DMS depthTex;

layout(location = 0) in vec2 inUVs;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 3, std140) uniform UBO {
    vec4 dofParams; // x: focusDistance / y: focusRange / z: strength
    vec2 nearFarPlane;
} ubo;

float linearizeDepth(float depth, float nearPlane, float farPlane)
{
    return nearPlane * farPlane / (farPlane + depth * (nearPlane - farPlane));
}

void main() {
    float focusDistance = 4.0; // ubo.dofParams.x;
    float focusRange = 10.0; // ubo.dofParams.y;
    float dofStrength = ubo.dofParams.z;

    // Depth of field
    float depth = texelFetch(depthTex, ivec2(gl_FragCoord.xy), 0).r;
    float linDepth = linearizeDepth(depth, ubo.nearFarPlane.x, ubo.nearFarPlane.y);

    float coc = clamp(abs(linDepth - focusDistance) / focusRange, 0.0, 1.0) * dofStrength;
    outColor = mix(texture(baseTex, inUVs), texture(blurTex, inUVs), coc);
}
