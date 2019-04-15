#version 450
#extension GL_ARB_separate_shader_objects : enable

#ifdef HAS_DIFFUSE_MAP
layout(set = 0, binding = 0) uniform sampler2D diffuseTex;
#endif

#ifdef HAS_SPECULAR_MAP
layout(set = 0, binding = 1) uniform sampler2D specularTex;
#endif

#ifdef HAS_AMBIENT_MAP
layout(set = 0, binding = 2) uniform sampler2D ambientTex;
#endif

#ifdef HAS_EMISSIVE_MAP
layout(set = 0, binding = 3) uniform sampler2D emissiveTex;
#endif

#ifdef HAS_METALLIC_ROUGHNESS_MAP
layout(set = 0, binding = 4) uniform sampler2D metallicRoughnessTex;
#endif

#ifdef HAS_HEIGHT_MAP
layout(set = 0, binding = 5) uniform sampler2D heightTex;
#endif

#ifdef HAS_NORMAL_MAP
layout(set = 0, binding = 6) uniform sampler2D normalTex;
#endif

#ifdef HAS_SHININESS_MAP
layout(set = 0, binding = 7) uniform sampler2D shininessTex;
#endif

#ifdef HAS_OPACITY_MAP
layout(set = 0, binding = 8) uniform sampler2D opacityTex;
#endif

#ifdef HAS_DISPLACEMENT_MAP
layout(set = 0, binding = 9) uniform sampler2D displacementTex;
#endif

#ifdef HAS_LIGHT_MAP
layout(set = 0, binding = 10) uniform sampler2D lightTex;
#endif

#ifdef HAS_REFLECTION_MAP
layout(set = 0, binding = 11) uniform sampler2D reflectionTex;
#endif

layout(location = 0) in vec2 inUVs;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(diffuseTex, vec2(1.0, -1.0) * inUVs);
}
