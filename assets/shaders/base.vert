#version 450

#ifdef HAS_POSITIONS
layout(location = 0) in vec3 inPosition;
#endif
#ifdef HAS_NORMALS
layout(location = 1) in vec3 inNormal;
#endif
#ifdef HAS_TANGENTS
layout(location = 2) in vec3 inTangent;
#endif
#ifdef HAS_BITANGENTS
layout(location = 3) in vec3 inBitangent;
#endif
#ifdef HAS_COLORS
layout(location = 4) in vec4 inColor;
#endif
#ifdef HAS_UV0
layout(location = 5) in vec2 inUV0;
#endif
#ifdef HAS_UV1
layout(location = 6) in vec2 inUV1;
#endif

layout(location = 0) out vec2 outUV0;

layout(set = 0, binding = 0, std140) uniform UBO {
	mat4 mvp;
} ubo;

void main() {
    gl_Position = ubo.mvp * vec4(inPosition, 1.0);
    outUV0 = inUV0;
}
