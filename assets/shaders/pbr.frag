//
// This fragment shader defines a reference implementation for Physically Based Shading of
// a microfacet surface material defined by a glTF model.
//
// References:
// [1] Real Shading in Unreal Engine 4
//     http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
// [2] Physically Based Shading at Disney
//     http://blog.selfshadow.com/publications/s2012-shading-course/burley/s2012_pbs_disney_brdf_notes_v3.pdf
// [3] README.md - Environment Maps
//     https://github.com/KhronosGroup/glTF-WebGL-PBR/#environment-maps
// [4] "An Inexpensive BRDF Model for Physically based Rendering" by Christophe Schlick
//     https://www.cs.virginia.edu/~jdl/bib/appearance/analytic%20models/schlick94b.pdf
#version 450

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
layout(set = 0, binding = 11) uniform samplerCube reflectionTex;
layout(set = 0, binding = 16) uniform sampler2D brdfLUT;
#endif

layout(set = 0, binding = 14) uniform sampler2DShadow shadowTex;

const float M_PI = 3.141592653589793;
const float c_MinReflectance = 0.04;

layout(location = 0) in vec3 inPosition;

#ifdef HAS_COLORS
layout(location = 4) in vec4 inColor;
#endif

layout(location = 5) in vec2 inUV0;
layout(location = 6) in vec2 inUV1;

#ifdef HAS_NORMALS
#ifdef HAS_TANGENTS
layout(location = 8) in mat3 inTBN;
#else
layout(location = 8) in vec3 inNormal;
#endif
#endif

layout(location = 11) in vec3 inShadowPos;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 13, std140) uniform FragUBO {
	vec4 egmr; // x: exposure / y: gamma / z: metallic / w: roughness
	vec4 baseColor;
	vec4 cameraAndCutoff; // xyz: camera / w: alpha cutoff
	vec2 mipIBL; // x: reflection mipCount / y: IBL strength
} ubo;

struct Light
{
    vec4 directionAndType;
    vec4 colorAndIntensity;
    vec4 positionAndRange;
    vec2 innerAndOuterConeCos;
};

const int LightType_Directional = 0;
const int LightType_Point = 1;
const int LightType_Spot = 2;

const int MAX_LIGHTS = 64;

layout(set = 0, binding = 15, std140) uniform LightUBO {
    vec4 ambientAndNumLights;
    vec4 shadowIds;
    Light lights[MAX_LIGHTS];
} lightUbo;

struct AngularInfo
{
    float NdotL;                  // cos angle between normal and light direction
    float NdotV;                  // cos angle between normal and view direction
    float NdotH;                  // cos angle between normal and half vector
    float LdotH;                  // cos angle between light direction and half vector

    float VdotH;                  // cos angle between view direction and half vector

    vec3 padding;
};

vec4 getVertexColor()
{
   vec4 color = vec4(1.0, 1.0, 1.0, 1.0);

#ifdef HAS_COLORS
    color = inColor;
#endif

   return color;
}

// Find the normal for this fragment, pulling either from a predefined normal map
// or from the interpolated mesh normal and tangent attributes.
vec3 getNormal()
{
    vec2 UV = inUV0;

    // Retrieve the tangent space matrix
#ifndef HAS_TANGENTS
    vec3 pos_dx = dFdx(inPosition);
    vec3 pos_dy = dFdy(inPosition);
    vec3 tex_dx = dFdx(vec3(UV, 0.0));
    vec3 tex_dy = dFdy(vec3(UV, 0.0));
    vec3 t = (tex_dy.t * pos_dx - tex_dx.t * pos_dy) / (tex_dx.s * tex_dy.t - tex_dy.s * tex_dx.t);

#ifdef HAS_NORMALS
    vec3 ng = normalize(inNormal);
#else
    vec3 ng = cross(pos_dx, pos_dy);
#endif

    t = normalize(t - ng * dot(ng, t));
    vec3 b = normalize(cross(ng, t));
    mat3 tbn = mat3(t, b, ng);
#else // HAS_TANGENTS
    mat3 tbn = inTBN;
#endif

#ifdef HAS_NORMAL_MAP
    vec3 n = texture(normalTex, UV).rgb;
    n = normalize(tbn * (2.0 * n - 1.0));
#else
    // The tbn matrix is linearly interpolated, so we need to re-normalize
    vec3 n = normalize(tbn[2].xyz);
#endif

    return n;
}

float getPerceivedBrightness(vec3 vector)
{
    return sqrt(0.299 * vector.r * vector.r + 0.587 * vector.g * vector.g + 0.114 * vector.b * vector.b);
}

// https://github.com/KhronosGroup/glTF/blob/master/extensions/2.0/Khronos/KHR_materials_pbrSpecularGlossiness/examples/convert-between-workflows/js/three.pbrUtilities.js#L34
float solveMetallic(vec3 diffuse, vec3 specular, float oneMinusSpecularStrength) {
    float specularBrightness = getPerceivedBrightness(specular);

    if (specularBrightness < c_MinReflectance) {
        return 0.0;
    }

    float diffuseBrightness = getPerceivedBrightness(diffuse);

    float a = c_MinReflectance;
    float b = diffuseBrightness * oneMinusSpecularStrength / (1.0 - c_MinReflectance) + specularBrightness - 2.0 * c_MinReflectance;
    float c = c_MinReflectance - specularBrightness;
    float D = b * b - 4.0 * a * c;

    return clamp((-b + sqrt(D)) / (2.0 * a), 0.0, 1.0);
}

AngularInfo getAngularInfo(vec3 pointToLight, vec3 normal, vec3 view)
{
    // Standard one-letter names
    vec3 n = normalize(normal);           // Outward direction of surface point
    vec3 v = normalize(view);             // Direction from surface point to view
    vec3 l = normalize(pointToLight);     // Direction from surface point to light
    vec3 h = normalize(l + v);            // Direction of the vector between l and v

    float NdotL = clamp(dot(n, l), 0.0, 1.0);
    float NdotV = clamp(dot(n, v), 0.0, 1.0);
    float NdotH = clamp(dot(n, h), 0.0, 1.0);
    float LdotH = clamp(dot(l, h), 0.0, 1.0);
    float VdotH = clamp(dot(v, h), 0.0, 1.0);

    return AngularInfo(
        NdotL,
        NdotV,
        NdotH,
        LdotH,
        VdotH,
        vec3(0, 0, 0)
    );
}

struct MaterialInfo
{
    float perceptualRoughness;    // roughness value, as authored by the model creator (input to shader)
    vec3 reflectance0;            // full reflectance color (normal incidence angle)

    float alphaRoughness;         // roughness mapped to a more linear change in the roughness (proposed by [2])
    vec3 diffuseColor;            // color contribution from diffuse lighting

    vec3 reflectance90;           // reflectance color at grazing angle
    vec3 specularColor;           // color contribution from specular lighting
};

// Gamma Correction in Computer Graphics
// see https://www.teamten.com/lawrence/graphics/gamma/
vec3 gammaCorrection(vec3 color)
{
    return pow(color, vec3(1.0 / ubo.egmr.y));
}

// sRGB to linear approximation
// see http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
vec4 SRGBtoLINEAR(vec4 srgbIn)
{
    return vec4(pow(srgbIn.xyz, vec3(ubo.egmr.y)), srgbIn.w);
}

// Uncharted 2 tone map
// see: http://filmicworlds.com/blog/filmic-tonemapping-operators/
vec3 toneMapUncharted2Impl(vec3 color)
{
    const float A = 0.15;
    const float B = 0.50;
    const float C = 0.10;
    const float D = 0.20;
    const float E = 0.02;
    const float F = 0.30;
    return ((color*(A*color+C*B)+D*E)/(color*(A*color+B)+D*F))-E/F;
}

vec3 toneMapUncharted(vec3 color)
{
    const float W = 11.2;
    color = toneMapUncharted2Impl(color * 2.0);
    vec3 whiteScale = 1.0 / toneMapUncharted2Impl(vec3(W));
    return gammaCorrection(color * whiteScale);
}

// Hejl Richard tone map
// see: http://filmicworlds.com/blog/filmic-tonemapping-operators/
vec3 toneMapHejlRichard(vec3 color)
{
    color = max(vec3(0.0), color - vec3(0.004));
    return (color*(6.2*color+.5))/(color*(6.2*color+1.7)+0.06);
}

vec3 toneMap(vec3 color)
{
    color *= ubo.egmr.x;

#ifdef TONEMAP_UNCHARTED
    return toneMapUncharted(color);
#endif

#ifdef TONEMAP_HEJLRICHARD
    return toneMapHejlRichard(color);
#endif

    return gammaCorrection(color);
}

// Lambert lighting
// see https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
vec3 diffuse(MaterialInfo materialInfo)
{
    return materialInfo.diffuseColor / M_PI;
}

// The following equation models the Fresnel reflectance term of the spec equation (aka F())
// Implementation of fresnel from [4], Equation 15
vec3 specularReflection(MaterialInfo materialInfo, AngularInfo angularInfo)
{
    return materialInfo.reflectance0 + (materialInfo.reflectance90 - materialInfo.reflectance0) * pow(clamp(1.0 - angularInfo.VdotH, 0.0, 1.0), 5.0);
}

// Smith Joint GGX
// Note: Vis = G / (4 * NdotL * NdotV)
// see Eric Heitz. 2014. Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs. Journal of Computer Graphics Techniques, 3
// see Real-Time Rendering. Page 331 to 336.
// see https://google.github.io/filament/Filament.md.html#materialsystem/specularbrdf/geometricshadowing(specularg)
float visibilityOcclusion(MaterialInfo materialInfo, AngularInfo angularInfo)
{
    float NdotL = angularInfo.NdotL;
    float NdotV = angularInfo.NdotV;
    float alphaRoughnessSq = materialInfo.alphaRoughness * materialInfo.alphaRoughness;

    float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
    float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);

    return 0.5 / (GGXV + GGXL);
}

// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float microfacetDistribution(MaterialInfo materialInfo, AngularInfo angularInfo)
{
    float alphaRoughnessSq = materialInfo.alphaRoughness * materialInfo.alphaRoughness;
    float f = (angularInfo.NdotH * alphaRoughnessSq - angularInfo.NdotH) * angularInfo.NdotH + 1.0;
    return alphaRoughnessSq / (M_PI * f * f);
}

vec3 getPointShade(vec3 pointToLight, MaterialInfo materialInfo, vec3 normal, vec3 view)
{
    AngularInfo angularInfo = getAngularInfo(pointToLight, normal, view);

    if (angularInfo.NdotL > 0.0 && angularInfo.NdotV > 0.0)
    {
        // Calculate the shading terms for the microfacet specular shading model
        vec3 F = specularReflection(materialInfo, angularInfo);
        float Vis = visibilityOcclusion(materialInfo, angularInfo);
        float D = microfacetDistribution(materialInfo, angularInfo);

        // Calculation of analytical lighting contribution
        vec3 diffuseContrib = (1.0 - F) * diffuse(materialInfo);
        vec3 specContrib = F * Vis * D;

        // Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
        return angularInfo.NdotL * (diffuseContrib + specContrib);
    }

    return vec3(0.0, 0.0, 0.0);
}

// https://github.com/KhronosGroup/glTF/blob/master/extensions/2.0/Khronos/KHR_lights_punctual/README.md#range-property
float getRangeAttenuation(float range, float distance)
{
    if (range < 0.0)
    {
        // negative range means unlimited
        return 1.0;
    }
    return max(min(1.0 - pow(distance / range, 4.0), 1.0), 0.0) / pow(distance, 2.0);
}

// https://github.com/KhronosGroup/glTF/blob/master/extensions/2.0/Khronos/KHR_lights_punctual/README.md#inner-and-outer-cone-angles
float getSpotAttenuation(vec3 pointToLight, vec3 spotDirection, float outerConeCos, float innerConeCos)
{
    float actualCos = dot(normalize(spotDirection), normalize(-pointToLight));
    if (actualCos > outerConeCos)
    {
        if (actualCos < innerConeCos)
        {
            return smoothstep(outerConeCos, innerConeCos, actualCos);
        }
        return 1.0;
    }
    return 0.0;
}

vec3 applyDirectionalLight(Light light, MaterialInfo materialInfo, vec3 normal, vec3 view)
{
    vec3 pointToLight = -light.directionAndType.xyz;
    vec3 shade = getPointShade(pointToLight, materialInfo, normal, view);
    return light.colorAndIntensity.a * light.colorAndIntensity.rgb * shade;
}

vec3 applyPointLight(Light light, MaterialInfo materialInfo, vec3 normal, vec3 view)
{
    vec3 pointToLight = light.positionAndRange.xyz - inPosition;
    float distance = length(pointToLight);
    float attenuation = getRangeAttenuation(light.positionAndRange.w, distance);
    vec3 shade = getPointShade(pointToLight, materialInfo, normal, view);
    return attenuation * light.colorAndIntensity.a * light.colorAndIntensity.rgb * shade;
}

vec3 applySpotLight(Light light, MaterialInfo materialInfo, vec3 normal, vec3 view)
{
    vec3 pointToLight = light.positionAndRange.xyz - inPosition;
    float distance = length(pointToLight);
    float rangeAttenuation = getRangeAttenuation(light.positionAndRange.w, distance);
    float spotAttenuation = getSpotAttenuation(pointToLight, light.directionAndType.xyz,
        light.innerAndOuterConeCos.y, light.innerAndOuterConeCos.x);
    vec3 shade = getPointShade(pointToLight, materialInfo, normal, view);
    return rangeAttenuation * spotAttenuation * light.colorAndIntensity.a
        * light.colorAndIntensity.rgb * shade;
}

#ifdef HAS_REFLECTION_MAP
vec3 getIBLContribution(MaterialInfo materialInfo, vec3 n, vec3 v)
{
    float NdotV = clamp(dot(n, v), 0.0, 1.0);
    float mipCount = ubo.mipIBL.x;
    float lod = clamp(materialInfo.perceptualRoughness * mipCount, 0.0, mipCount);
    vec3 reflection = normalize(reflect(-v, n));

    vec2 brdfSamplePoint = clamp(vec2(NdotV, materialInfo.perceptualRoughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
    // retrieve a scale and bias to F0. See [1], Figure 3
    vec2 brdf = texture(brdfLUT, brdfSamplePoint).rg;
    vec4 specularSample = texture(reflectionTex, reflection, lod);

    vec3 specularLight = SRGBtoLINEAR(specularSample).rgb;
    vec3 specular = specularLight * (materialInfo.specularColor * brdf.x + brdf.y);
    return specular;
}
#endif

void main()
{
    // Metallic and Roughness material properties are packed together
    // In glTF, these factors can be specified by fixed scalar values
    // or from a metallic-roughness map
    float perceptualRoughness = 0.0;
    float metallic = 0.0;
    vec4 baseColor = vec4(0.0, 0.0, 0.0, 1.0);
    vec3 diffuseColor = vec3(0.0);
    vec3 specularColor= vec3(0.0);
    vec3 f0 = vec3(0.04);

#ifdef HAS_METALLIC_ROUGHNESS_MAP
    // Roughness is stored in the 'g' channel, metallic is stored in the 'b' channel.
    // This layout intentionally reserves the 'r' channel for (optional) occlusion map data
    vec4 mrSample = texture(metallicRoughnessTex, inUV0);
    perceptualRoughness = mrSample.g * ubo.egmr.w;
    metallic = mrSample.b * ubo.egmr.z;
#else
    metallic = ubo.egmr.z;
    perceptualRoughness = ubo.egmr.w;
#endif

    // The albedo may be defined from a base texture or a flat color
#ifdef HAS_DIFFUSE_MAP
    baseColor = SRGBtoLINEAR(texture(diffuseTex, inUV0)) * ubo.baseColor;
#else
    baseColor = ubo.baseColor;
#endif

    baseColor *= getVertexColor();

    diffuseColor = baseColor.rgb * (vec3(1.0) - f0) * (1.0 - metallic);

    specularColor = mix(f0, baseColor.rgb, metallic);

#ifdef ALPHAMODE_MASK
    if(baseColor.a < ubo.cameraAndCutoff.w)
    {
        discard;
    }
    baseColor.a = 1.0;
#endif

#ifdef ALPHAMODE_OPAQUE
    baseColor.a = 1.0;
#endif

#ifdef MATERIAL_UNLIT
    outColor = vec4(gammaCorrection(baseColor.rgb), baseColor.a);
    return;
#endif

    perceptualRoughness = clamp(perceptualRoughness, 0.0, 1.0);
    metallic = clamp(metallic, 0.0, 1.0);

    // Roughness is authored as perceptual roughness; as is convention,
    // convert to material roughness by squaring the perceptual roughness [2].
    float alphaRoughness = perceptualRoughness * perceptualRoughness;

    // Compute reflectance.
    float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);

    vec3 specularEnvironmentR0 = specularColor.rgb;
    // Anything less than 2% is physically impossible and is instead considered to be shadowing. Compare to "Real-Time-Rendering" 4th editon on page 325.
    vec3 specularEnvironmentR90 = vec3(clamp(reflectance * 50.0, 0.0, 1.0));

    MaterialInfo materialInfo = MaterialInfo(
        perceptualRoughness,
        specularEnvironmentR0,
        alphaRoughness,
        diffuseColor,
        specularEnvironmentR90,
        specularColor
    );

    // LIGHTING

    const vec3 ambientColor = lightUbo.ambientAndNumLights.rgb;
    vec3 color = ambientColor * diffuseColor;
    vec3 normal = getNormal();
    vec3 view = normalize(ubo.cameraAndCutoff.xyz - inPosition);

    int numLights = floatBitsToInt(lightUbo.ambientAndNumLights.w);
    int shadowId = floatBitsToInt(lightUbo.shadowIds[0]);
    for (int i = 0; i < numLights; ++i)
    {
        Light light = lightUbo.lights[i];

        int type = floatBitsToInt(light.directionAndType.w);
        if (type == LightType_Directional)
        {
            float shadow = 1.0;
            if (i == shadowId)
            {
                shadow = texture(shadowTex, inShadowPos).r;
            }

            color += shadow * applyDirectionalLight(light, materialInfo, normal, view);
        }
        else if (type == LightType_Point)
        {
            color += applyPointLight(light, materialInfo, normal, view);
        }
        else if (type == LightType_Spot)
        {
            color += applySpotLight(light, materialInfo, normal, view);
        }
    }

    float ao = 1.0;
    // Apply optional PBR terms for additional (optional) shading
#ifdef HAS_LIGHT_MAP
    // Occlusion map
    ao = texture(lightTex, inUV0).r;
    color = mix(color, color * ao, 1.0); // Forced occlusionStrength = 1.0
#endif

    vec3 emissive = vec3(0);
#ifdef HAS_EMISSIVE_MAP
    emissive = SRGBtoLINEAR(texture(emissiveTex, inUV0)).rgb;
    color += emissive;
#endif

#ifdef HAS_REFLECTION_MAP
    color += ubo.mipIBL.y * getIBLContribution(materialInfo, normal, view);
#endif

#ifndef DEBUG_OUTPUT // no debug

    // regular shading
    outColor = vec4(toneMap(color), baseColor.a);

#else // debug output

    #ifdef DEBUG_METALLIC
        outColor.rgb = vec3(metallic);
    #endif

    #ifdef DEBUG_ROUGHNESS
        outColor.rgb = vec3(perceptualRoughness);
    #endif

    #ifdef DEBUG_NORMAL
        #ifdef HAS_NORMAL_MAP
            outColor.rgb = texture(normalTex, inUV0).rgb;
        #else
            outColor.rgb = vec3(0.5, 0.5, 1.0);
        #endif
    #endif

    #ifdef DEBUG_BASECOLOR
        outColor.rgb = gammaCorrection(baseColor.rgb);
    #endif

    #ifdef DEBUG_OCCLUSION
        outColor.rgb = vec3(ao);
    #endif

    #ifdef DEBUG_EMISSIVE
        outColor.rgb = gammaCorrection(emissive);
    #endif

    #ifdef DEBUG_F0
        outColor.rgb = vec3(f0);
    #endif

    #ifdef DEBUG_ALPHA
        outColor.rgb = vec3(baseColor.a);
    #endif

    outColor.a = 1.0;

#endif // !DEBUG_OUTPUT
}
