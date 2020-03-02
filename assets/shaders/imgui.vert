#version 450 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;

layout(binding = 1, std140) uniform UBO {
    vec4 uScaleTranslate; // scale: xy, translate: zw
} ubo;

out gl_PerVertex {
    vec4 gl_Position;
};

layout(location = 0) out struct {
    vec4 Color;
    vec2 UV;
} Out;

void main()
{
    Out.Color = aColor;
    Out.UV = aUV;
    gl_Position = vec4(aPos * ubo.uScaleTranslate.xy + ubo.uScaleTranslate.zw, 0, 1);
}
