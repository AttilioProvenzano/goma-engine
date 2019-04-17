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

layout(location = 0) out vec3 outPosition;

#ifdef HAS_COLORS
layout(location = 4) out vec4 outColor;
#endif

layout(location = 5) out vec2 outUV0;
layout(location = 6) out vec2 outUV1;

#ifdef HAS_NORMALS
#ifdef HAS_TANGENTS
layout(location = 8) out mat3 outTBN;
#else
layout(location = 8) out vec3 outNormal;
#endif
#endif

layout(set = 0, binding = 12, std140) uniform VertUBO {
	mat4 mvp;
	mat4 model;
	mat4 normal;
} ubo;

void main()
{
    vec4 pos = ubo.model * vec4(inPosition, 1.0);
    outPosition = pos.xyz / pos.w;

#ifdef HAS_NORMALS
#ifdef HAS_TANGENTS
    vec3 normalW = normalize(vec3(ubo.normal * vec4(inNormal.xyz, 0.0)));
    vec3 tangentW = normalize(vec3(ubo.model * vec4(inTangent.xyz, 0.0)));
    vec3 bitangentW = cross(normalW, tangentW);
    outTBN = mat3(tangentW, bitangentW, normalW);
#else // !HAS_TANGENTS
    outNormal = normalize(vec3(ubo.normal * vec4(inNormal.xyz, 0.0)));
#endif
#endif // !HAS_NORMALS

    outUV0 = vec2(0.0, 0.0);
    outUV1 = vec2(0.0, 0.0);

#ifdef HAS_UV0
    outUV0 = inUV0 * vec2(1.0, -1.0);
#endif

#ifdef HAS_UV1
    outUV1 = inUV1 * vec2(1.0, -1.0);
#endif

#ifdef HAS_COLORS
    outColor = inColor;
#endif

    gl_Position = ubo.mvp * vec4(inPosition, 1.0);
}
