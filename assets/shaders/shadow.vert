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

#ifdef HAS_UVW
layout(location = 7) in vec2 inUVW;
#endif

layout(set = 0, binding = 12, std140) uniform UBO {
	mat4 mvp;
	mat4 model;
	mat4 normal;
} ubo;

void main() {
    gl_Position = ubo.mvp * vec4(inPosition, 1.0);
}
