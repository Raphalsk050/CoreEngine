#pragma once

namespace CoreEngine::BuiltinShaders {
    inline constexpr const char *kUnlitVS = R"HLSL(
cbuffer PerFrame : register(b0)
{
    float4x4 g_ViewProj;
    float4   g_frameTime;
};

cbuffer PerObject : register(b1)
{
    float4x4 g_Model;
};

struct VSInput
{
    float3 pos   : ATTRIB0;
    float3 norm  : ATTRIB1;
    float3 color : ATTRIB2;
    float2 uv    : ATTRIB3;
    float4 time  : ATTRIB4;
};

struct PSInput
{
    float4 pos   : SV_POSITION;
    float3 color : COLOR0;
    float3 norm  : COLOR1;
    float2 uv    : TEXCOORD0;
    float4 time  : TEXCOORD1;
};

void main(in VSInput i, out PSInput o)
{
    float4 world = mul(g_Model, float4(i.pos, 1.0));
    o.pos        = mul(g_ViewProj, world);
    o.color      = i.color;
    o.norm       = i.norm;
    o.uv         = i.uv;
    o.time       = i.time;
}
)HLSL";

    inline constexpr const char *kUnlitPS = R"HLSL(
cbuffer PerMaterial : register(b2)
{
    float4 g_Tint;
};

struct PSInput
{
    float4 pos   : SV_POSITION;
    float3 color : COLOR0;
    float3 norm  : COLOR1;
    float2 uv    : TEXCOORD0;
    float4 time  : TEXCOORD1;
};

float4 main(in PSInput i) : SV_TARGET
{
    return float4(i.color, 1.0) * g_Tint;
}
)HLSL";

    inline constexpr const char *kTexturedUnlitPS = R"HLSL(
Texture2D g_Albedo;
SamplerState g_Albedo_sampler;

cbuffer PerMaterial : register(b2)
{
    float4 g_Tint;
};

struct PSInput
{
    float4 pos   : SV_POSITION;
    float3 color : COLOR0;
    float3 norm  : COLOR1;
    float2 uv    : TEXCOORD0;
    float4 time  : TEXCOORD1;
};

float4 main(in PSInput i) : SV_TARGET
{
    return g_Albedo.Sample(g_Albedo_sampler, i.uv) * float4(i.color, 1.0) * g_Tint;
}
)HLSL";

    inline constexpr const char *kPbrStandardVS = R"HLSL(
cbuffer PerFrame : register(b0)
{
    float4x4 g_ViewProj;
    float4   g_frameTime;
    float4   g_CameraPositionExposure;
    float4   g_DirectionalLightDirectionIlluminance;
    float4   g_DirectionalLightColorEnabled;
    float4   g_EnvironmentLightDiffuseIntensity;
    float4   g_EnvironmentLightSpecularIntensity;
    float4   g_EnvironmentLightEnabled;
    float4   g_PointLightPositionRange[8];
    float4   g_PointLightColorIntensity[8];
    float4   g_PointLightShadowParams[8];
    float4   g_PointLightParams;
    float4x4 g_DirectionalShadowViewProj[4];
    float4   g_DirectionalShadowSplits;
    float4   g_DirectionalShadowParams;
    float4   g_DirectionalShadowExtra;
    float4   g_PointShadowParams;
    float4x4 g_PointShadowViewProj[36];
    float4   g_ReflectionProbePositionRadius;
    float4   g_ReflectionProbeParams;
    float4   g_PbrDebugParams;
};

cbuffer PerObject : register(b1)
{
    float4x4 g_Model;
};

struct VSInput
{
    float3 pos   : ATTRIB0;
    float3 norm  : ATTRIB1;
    float3 color : ATTRIB2;
    float2 uv    : ATTRIB3;
    float4 tangent : ATTRIB4;
};

struct PSInput
{
    float4 pos       : SV_POSITION;
    float3 color     : COLOR0;
    float3 normal_ws : COLOR1;
    float2 uv        : TEXCOORD0;
    float3 world_pos : TEXCOORD1;
    float4 tangent_ws : TEXCOORD2;
};

void main(in VSInput i, out PSInput o)
{
    float4 world  = mul(g_Model, float4(i.pos, 1.0));
    o.pos         = mul(g_ViewProj, world);
    o.world_pos   = world.xyz;
    o.color       = i.color;
    o.normal_ws   = normalize(mul((float3x3)g_Model, i.norm));
    o.uv          = i.uv;
    o.tangent_ws  = float4(mul((float3x3)g_Model, i.tangent.xyz), i.tangent.w);
}
)HLSL";

    inline constexpr const char *kPbrStandardPS = R"HLSL(
#ifndef CORE_ENGINE_HAS_BASE_COLOR_TEXTURE
#define CORE_ENGINE_HAS_BASE_COLOR_TEXTURE 0
#endif

#if CORE_ENGINE_HAS_BASE_COLOR_TEXTURE
Texture2D g_BaseColorTexture;
SamplerState g_BaseColorTexture_sampler;
#endif

#ifndef CORE_ENGINE_HAS_NORMAL_TEXTURE
#define CORE_ENGINE_HAS_NORMAL_TEXTURE 0
#endif

#if CORE_ENGINE_HAS_NORMAL_TEXTURE
Texture2D g_NormalTexture;
SamplerState g_NormalTexture_sampler;
#endif

#ifndef CORE_ENGINE_HAS_METALLIC_TEXTURE
#define CORE_ENGINE_HAS_METALLIC_TEXTURE 0
#endif

#if CORE_ENGINE_HAS_METALLIC_TEXTURE
Texture2D g_MetallicTexture;
SamplerState g_MetallicTexture_sampler;
#endif

#ifndef CORE_ENGINE_HAS_ROUGHNESS_TEXTURE
#define CORE_ENGINE_HAS_ROUGHNESS_TEXTURE 0
#endif

#if CORE_ENGINE_HAS_ROUGHNESS_TEXTURE
Texture2D g_RoughnessTexture;
SamplerState g_RoughnessTexture_sampler;
#endif

#ifndef CORE_ENGINE_HAS_METALLIC_ROUGHNESS_TEXTURE
#define CORE_ENGINE_HAS_METALLIC_ROUGHNESS_TEXTURE 0
#endif

#if CORE_ENGINE_HAS_METALLIC_ROUGHNESS_TEXTURE
Texture2D g_MetallicRoughnessTexture;
SamplerState g_MetallicRoughnessTexture_sampler;
#endif

#ifndef CORE_ENGINE_HAS_AO_TEXTURE
#define CORE_ENGINE_HAS_AO_TEXTURE 0
#endif

#if CORE_ENGINE_HAS_AO_TEXTURE
Texture2D g_AmbientOcclusionTexture;
SamplerState g_AmbientOcclusionTexture_sampler;
#endif

#ifndef CORE_ENGINE_HAS_EMISSIVE_TEXTURE
#define CORE_ENGINE_HAS_EMISSIVE_TEXTURE 0
#endif

#if CORE_ENGINE_HAS_EMISSIVE_TEXTURE
Texture2D g_EmissiveTexture;
SamplerState g_EmissiveTexture_sampler;
#endif

Texture2DArray<float> g_DirectionalShadowMap;
Texture2DArray<float> g_PointShadowMap;
TextureCube g_IrradianceMap;
TextureCube g_PrefilteredSpecularMap;
Texture2D g_BrdfLut;
SamplerState g_PbrSampler;

cbuffer PerFrame : register(b0)
{
    float4x4 g_ViewProj;
    float4   g_frameTime;
    float4   g_CameraPositionExposure;
    float4   g_DirectionalLightDirectionIlluminance;
    float4   g_DirectionalLightColorEnabled;
    float4   g_EnvironmentLightDiffuseIntensity;
    float4   g_EnvironmentLightSpecularIntensity;
    float4   g_EnvironmentLightEnabled;
    float4   g_PointLightPositionRange[8];
    float4   g_PointLightColorIntensity[8];
    float4   g_PointLightShadowParams[8];
    float4   g_PointLightParams;
    float4x4 g_DirectionalShadowViewProj[4];
    float4   g_DirectionalShadowSplits;
    float4   g_DirectionalShadowParams;
    float4   g_DirectionalShadowExtra;
    float4   g_PointShadowParams;
    float4x4 g_PointShadowViewProj[36];
    float4   g_ReflectionProbePositionRadius;
    float4   g_ReflectionProbeParams;
    float4   g_PbrDebugParams;
};

cbuffer PerMaterial : register(b2)
{
    float4 g_BaseColor;
    float4 g_Emissive;
    float4 g_Surface;
};

struct PSInput
{
    float4 pos       : SV_POSITION;
    float3 color     : COLOR0;
    float3 normal_ws : COLOR1;
    float2 uv        : TEXCOORD0;
    float3 world_pos : TEXCOORD1;
    float4 tangent_ws : TEXCOORD2;
};

static const float PI = 3.14159265359;
static const float MIN_PERCEPTUAL_ROUGHNESS = 0.089;

float3 F_Schlick(float u, float3 f0, float f90)
{
    float f = 1.0 - saturate(u);
    float f2 = f * f;
    float f5 = f2 * f2 * f;
    return f0 + (f90 - f0) * f5;
}

float D_GGX(float NoH, float roughness)
{
    float a = NoH * roughness;
    float k = roughness / max(1.0 - NoH * NoH + a * a, 1.0e-5);
    return k * k * (1.0 / PI);
}

float V_SmithGGXCorrelatedFast(float NoV, float NoL, float roughness)
{
    float GGXV = NoL * (NoV * (1.0 - roughness) + roughness);
    float GGXL = NoV * (NoL * (1.0 - roughness) + roughness);
    return 0.5 / max(GGXV + GGXL, 1.0e-5);
}

float2 EnvBRDFApprox(float NoV, float perceptual_roughness)
{
    const float4 c0 = float4(-1.0, -0.0275, -0.572, 0.022);
    const float4 c1 = float4(1.0, 0.0425, 1.04, -0.04);
    float4 r = perceptual_roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * saturate(NoV))) * r.x + r.y;
    return float2(-1.04, 1.04) * a004 + r.zw;
}

float3 EvaluatePunctualLight(float3 diffuse, float3 f0, float roughness, float3 n, float3 v, float3 l,
                             float3 illuminance)
{
    float NoL = saturate(dot(n, l));
    float NoV = max(abs(dot(n, v)), 1.0e-5);
    float3 h = normalize(v + l);
    float NoH = saturate(dot(n, h));
    float LoH = saturate(dot(l, h));

    float3 f = F_Schlick(LoH, f0, 1.0);
    float d = D_GGX(NoH, roughness);
    float visibility = V_SmithGGXCorrelatedFast(NoV, NoL, roughness);
    float3 specular = d * visibility * f;

    return (diffuse + specular) * illuminance * NoL;
}

float DistanceAttenuation(float distance_sq, float range)
{
    float inv_range = 1.0 / max(range, 1.0e-3);
    float factor = distance_sq * inv_range * inv_range;
    float smooth_factor = saturate(1.0 - factor * factor);
    return (smooth_factor * smooth_factor) / max(distance_sq, 1.0e-4);
}

float4 SampleBaseColor(float2 uv)
{
#if CORE_ENGINE_HAS_BASE_COLOR_TEXTURE
    return g_BaseColorTexture.Sample(g_BaseColorTexture_sampler, uv) * g_BaseColor;
#else
    return g_BaseColor;
#endif
}

float ResolveMetallic(float2 uv)
{
    float metallic = g_Surface.x;
#if CORE_ENGINE_HAS_METALLIC_ROUGHNESS_TEXTURE
    metallic *= g_MetallicRoughnessTexture.Sample(g_MetallicRoughnessTexture_sampler, uv).b;
#endif
#if CORE_ENGINE_HAS_METALLIC_TEXTURE
    metallic *= g_MetallicTexture.Sample(g_MetallicTexture_sampler, uv).r;
#endif
    return saturate(metallic);
}

float ResolvePerceptualRoughness(float2 uv)
{
    float perceptual_roughness = g_Surface.y;
#if CORE_ENGINE_HAS_METALLIC_ROUGHNESS_TEXTURE
    perceptual_roughness *= g_MetallicRoughnessTexture.Sample(g_MetallicRoughnessTexture_sampler, uv).g;
#endif
#if CORE_ENGINE_HAS_ROUGHNESS_TEXTURE
    perceptual_roughness *= g_RoughnessTexture.Sample(g_RoughnessTexture_sampler, uv).r;
#endif
    return max(saturate(perceptual_roughness), MIN_PERCEPTUAL_ROUGHNESS);
}

float ResolveAmbientOcclusion(float2 uv)
{
    float ambient_occlusion = g_Surface.w;
#if CORE_ENGINE_HAS_AO_TEXTURE
    ambient_occlusion *= g_AmbientOcclusionTexture.Sample(g_AmbientOcclusionTexture_sampler, uv).r;
#endif
    return saturate(ambient_occlusion);
}

float3 ResolveEmissive(float2 uv)
{
    float3 emissive = g_Emissive.rgb;
#if CORE_ENGINE_HAS_EMISSIVE_TEXTURE
    emissive *= g_EmissiveTexture.Sample(g_EmissiveTexture_sampler, uv).rgb;
#endif
    return max(emissive, float3(0.0, 0.0, 0.0));
}

float3 ResolveSurfaceNormal(float2 uv, float3 normal_ws, float4 tangent_ws)
{
    float3 n = normalize(normal_ws);
#if CORE_ENGINE_HAS_NORMAL_TEXTURE
    float tangent_length_sq = dot(tangent_ws.xyz, tangent_ws.xyz);
    if (tangent_length_sq <= 1.0e-8)
    {
        return n;
    }

    float3 t = normalize(tangent_ws.xyz - n * dot(n, tangent_ws.xyz));
    float3 b = cross(n, t) * (tangent_ws.w < 0.0 ? -1.0 : 1.0);
    float3 tangent_normal = g_NormalTexture.Sample(g_NormalTexture_sampler, uv).xyz * 2.0 - 1.0;
    return normalize(tangent_normal.x * t + tangent_normal.y * b + tangent_normal.z * n);
#else
    return n;
#endif
}
)HLSL"
R"HLSL(

float ShadowCompareDepth(float depth, float receiver_depth, float bias)
{
    return (receiver_depth - bias) <= depth ? 1.0 : 0.0;
}

float ShadowDepthLoad(Texture2DArray<float> shadow_map, int2 texel, int slice, int resolution)
{
    int2 clamped_texel = clamp(texel, int2(0, 0), int2(resolution - 1, resolution - 1));
    return shadow_map.Load(int4(clamped_texel, slice, 0));
}

float ShadowCompareBilinear(Texture2DArray<float> shadow_map, float2 uv, int slice, int resolution,
                            float receiver_depth, float bias)
{
    float2 texel_pos = uv * (float)resolution - 0.5;
    int2 base_texel = (int2)floor(texel_pos);
    float2 blend = frac(texel_pos);

    float s00 = ShadowCompareDepth(ShadowDepthLoad(shadow_map, base_texel, slice, resolution),
                                   receiver_depth, bias);
    float s10 = ShadowCompareDepth(ShadowDepthLoad(shadow_map, base_texel + int2(1, 0), slice, resolution),
                                   receiver_depth, bias);
    float s01 = ShadowCompareDepth(ShadowDepthLoad(shadow_map, base_texel + int2(0, 1), slice, resolution),
                                   receiver_depth, bias);
    float s11 = ShadowCompareDepth(ShadowDepthLoad(shadow_map, base_texel + int2(1, 1), slice, resolution),
                                   receiver_depth, bias);

    return lerp(lerp(s00, s10, blend.x), lerp(s01, s11, blend.x), blend.y);
}

float ShadowComparePoint(Texture2DArray<float> shadow_map, float2 uv, int slice, int resolution,
                         float receiver_depth, float bias)
{
    int2 texel = (int2)floor(uv * (float)resolution);
    return ShadowCompareDepth(ShadowDepthLoad(shadow_map, texel, slice, resolution), receiver_depth, bias);
}

float SampleShadowArray(Texture2DArray<float> shadow_map, float4 light_clip, int slice, float resolution, float bias,
                        float pcf_radius)
{
    if (slice < 0 || light_clip.w <= 0.0)
    {
        return 1.0;
    }

    float3 ndc = light_clip.xyz / light_clip.w;
    float2 uv = ndc.xy * 0.5 + 0.5;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 || ndc.z < 0.0 || ndc.z > 1.0)
    {
        return 1.0;
    }

    int radius = min(max((int)pcf_radius, 0), 4);
    int shadow_resolution = max((int)resolution, 1);
    float2 shadow_uv = saturate(float2(uv.x, 1.0 - uv.y));
    float2 texel_size = 1.0 / max(float2((float)shadow_resolution, (float)shadow_resolution), float2(1.0, 1.0));
    if (radius <= 0)
    {
        return ShadowComparePoint(shadow_map, shadow_uv, slice, shadow_resolution, ndc.z, bias);
    }

    float shadow = 0.0;
    float weight_sum = 0.0;
    [loop]
    for (int y = -radius; y <= radius; ++y)
    {
        [loop]
        for (int x = -radius; x <= radius; ++x)
        {
            float2 sample_uv = saturate(shadow_uv + float2((float)x, (float)y) * texel_size);
            float2 offset = float2((float)x, (float)y) / max((float)radius, 1.0);
            float weight = saturate(1.0 - 0.35 * dot(offset, offset));
            shadow += ShadowCompareBilinear(shadow_map, sample_uv, slice, shadow_resolution, ndc.z, bias) * weight;
            weight_sum += weight;
        }
    }

    return shadow / max(weight_sum, 1.0e-5);
}

float ResolveDirectionalShadow(float3 world_pos, float3 normal_ws, out int cascade_index)
{
    cascade_index = -1;
    int cascade_count = min((int)g_DirectionalShadowParams.x, 4);
    if (cascade_count <= 0)
    {
        return 1.0;
    }

    float camera_distance = distance(g_CameraPositionExposure.xyz, world_pos);
    [unroll]
    for (int cascade = 0; cascade < 4; ++cascade)
    {
        if (cascade >= cascade_count)
        {
            break;
        }

        if (camera_distance <= g_DirectionalShadowSplits[cascade] || cascade == cascade_count - 1)
        {
            cascade_index = cascade;
            float3 light_l = normalize(-g_DirectionalLightDirectionIlluminance.xyz);
            float slope = saturate(1.0 - dot(normalize(normal_ws), light_l));
            float normal_bias = g_DirectionalShadowParams.w * (1.0 + slope * 2.0);
            float bias = g_DirectionalShadowParams.z * (1.0 + slope * 3.0);
            float4 light_clip = mul(g_DirectionalShadowViewProj[cascade],
                                    float4(world_pos + normal_ws * normal_bias, 1.0));
            return SampleShadowArray(g_DirectionalShadowMap, light_clip, cascade,
                                     g_DirectionalShadowParams.y, bias, g_DirectionalShadowExtra.y);
        }
    }

    return 1.0;
}

int PointShadowFace(float3 v)
{
    float3 av = abs(v);
    if (av.x >= av.y && av.x >= av.z)
    {
        return v.x >= 0.0 ? 0 : 1;
    }
    if (av.y >= av.x && av.y >= av.z)
    {
        return v.y >= 0.0 ? 2 : 3;
    }
    return v.z >= 0.0 ? 4 : 5;
}

float ResolvePointShadow(float3 world_pos, float3 normal_ws, int light_index)
{
    float4 shadow_params = g_PointLightShadowParams[light_index];
    if (shadow_params.x <= 0.5)
    {
        return 1.0;
    }

    int shadow_index = (int)shadow_params.y;
    float3 to_light = normalize(g_PointLightPositionRange[light_index].xyz - world_pos);
    float slope = saturate(1.0 - dot(normalize(normal_ws), to_light));
    float3 shadow_pos = world_pos + normal_ws * (shadow_params.z * (1.0 + slope * 2.0));
    float3 from_light = shadow_pos - g_PointLightPositionRange[light_index].xyz;
    int face = PointShadowFace(from_light);
    int slice = shadow_index * 6 + face;
    float4 light_clip = mul(g_PointShadowViewProj[slice], float4(shadow_pos, 1.0));
    return SampleShadowArray(g_PointShadowMap, light_clip, slice, g_PointShadowParams.y,
                             shadow_params.w * (1.0 + slope * 3.0),
                             g_PointShadowParams.z);
}

float ResolveReflectionProbeInfluence(float3 world_pos)
{
    if (g_ReflectionProbeParams.x <= 0.5)
    {
        return 1.0;
    }

    float radius = max(g_ReflectionProbePositionRadius.w, 0.001);
    float distance_to_probe = distance(world_pos, g_ReflectionProbePositionRadius.xyz);
    return saturate(1.0 - distance_to_probe / radius);
}

float3 ResolveIrradianceSampleDirection(float3 normal_ws)
{
    return normalize(float3(normal_ws.x, -normal_ws.y, normal_ws.z));
}

float4 main(in PSInput i) : SV_TARGET
{
    float4 base = saturate(SampleBaseColor(i.uv)) * float4(saturate(i.color), 1.0);
    float metallic = ResolveMetallic(i.uv);
    float perceptual_roughness = ResolvePerceptualRoughness(i.uv);
    float roughness = perceptual_roughness * perceptual_roughness;
    float reflectance = saturate(g_Surface.z);
    float ambient_occlusion = ResolveAmbientOcclusion(i.uv);

    float3 n = ResolveSurfaceNormal(i.uv, i.normal_ws, i.tangent_ws);
    float3 v = normalize(g_CameraPositionExposure.xyz - i.world_pos);
    float light_enabled = g_DirectionalLightColorEnabled.w > 0.5 ? 1.0 : 0.0;

    float NoV = max(abs(dot(n, v)), 1.0e-5);

    float3 diffuse_color = base.rgb * (1.0 - metallic);
    float3 f0 = 0.16 * reflectance * reflectance * (1.0 - metallic) + base.rgb * metallic;

    float3 diffuse = diffuse_color * (1.0 / PI);
    float3 directional_l = normalize(-g_DirectionalLightDirectionIlluminance.xyz);
    float3 directional_illuminance = g_DirectionalLightColorEnabled.rgb * g_DirectionalLightDirectionIlluminance.w;
    int cascade_index = -1;
    float directional_shadow = ResolveDirectionalShadow(i.world_pos, n, cascade_index);
    float directional_shadow_strength = saturate(g_DirectionalShadowExtra.x);
    float directional_shadow_factor = lerp(1.0, directional_shadow, directional_shadow_strength);
    float final_shadow_factor = directional_shadow_factor;
    float3 direct = EvaluatePunctualLight(diffuse, f0, roughness, n, v, directional_l, directional_illuminance) *
        light_enabled * directional_shadow_factor;

    int point_light_count = min((int)g_PointLightParams.x, 8);
    for (int light_index = 0; light_index < point_light_count; ++light_index)
    {
        float4 position_range = g_PointLightPositionRange[light_index];
        float3 to_light = position_range.xyz - i.world_pos;
        float distance_sq = dot(to_light, to_light);
        if (distance_sq <= 1.0e-6)
        {
            continue;
        }

        float3 point_l = to_light * rsqrt(distance_sq);
        float attenuation = DistanceAttenuation(distance_sq, position_range.w);
        float4 color_intensity = g_PointLightColorIntensity[light_index];
        float3 point_illuminance = color_intensity.rgb * color_intensity.w * attenuation;
        float point_shadow = ResolvePointShadow(i.world_pos, n, light_index);
        final_shadow_factor = min(final_shadow_factor, point_shadow);
        direct += EvaluatePunctualLight(diffuse, f0, roughness, n, v, point_l, point_illuminance) * point_shadow;
    }

    float environment_enabled = g_EnvironmentLightEnabled.x > 0.5 ? 1.0 : 0.0;
    float3 environment_irradiance =
        max(g_EnvironmentLightDiffuseIntensity.rgb, 0.0) * max(g_EnvironmentLightDiffuseIntensity.w, 0.0);
    float3 environment_radiance =
        max(g_EnvironmentLightSpecularIntensity.rgb, 0.0) * max(g_EnvironmentLightSpecularIntensity.w, 0.0);
    float2 env_brdf = EnvBRDFApprox(NoV, perceptual_roughness);
    float reflection_probe_influence = ResolveReflectionProbeInfluence(i.world_pos);
    float ibl_texture_weight = saturate(g_EnvironmentLightEnabled.y);
    if (g_ReflectionProbeParams.x > 0.5)
    {
        ibl_texture_weight *= reflection_probe_influence;
    }
    float3 reflection_dir = reflect(-v, n);
    float3 ibl_irradiance =
        max(g_IrradianceMap.SampleLevel(g_PbrSampler, ResolveIrradianceSampleDirection(n), 0.0).rgb, 0.0);
    float ibl_max_specular_lod = max(g_EnvironmentLightEnabled.z - 1.0, 0.0);
    float3 ibl_radiance =
        max(g_PrefilteredSpecularMap.SampleLevel(g_PbrSampler, reflection_dir,
            perceptual_roughness * ibl_max_specular_lod).rgb, 0.0);
    if (g_ReflectionProbeParams.x > 0.5)
    {
        float reflection_probe_intensity = max(g_ReflectionProbeParams.y, 0.0);
        ibl_irradiance *= reflection_probe_intensity;
        ibl_radiance *= reflection_probe_intensity;
    }
    float2 ibl_brdf = max(g_BrdfLut.SampleLevel(g_PbrSampler, float2(NoV, perceptual_roughness), 0.0).rg, 0.0);
    environment_irradiance = lerp(environment_irradiance, ibl_irradiance, ibl_texture_weight);
    environment_radiance = lerp(environment_radiance, ibl_radiance, ibl_texture_weight);
    env_brdf = lerp(env_brdf, ibl_brdf, ibl_texture_weight);
    float3 environment_diffuse = diffuse * environment_irradiance;
    float3 environment_specular =
        environment_radiance * max(f0 * env_brdf.x + env_brdf.y, float3(0.0, 0.0, 0.0));
    float3 environment = (environment_diffuse + environment_specular) * environment_enabled * ambient_occlusion;
    float3 color = direct + environment + ResolveEmissive(i.uv);

    float debug_mode = g_PbrDebugParams.x;
    if (debug_mode > 0.5 && debug_mode < 1.5)
    {
        return float4(base.rgb, 1.0);
    }
    if (debug_mode < 2.5 && debug_mode > 1.5)
    {
        return float4(n * 0.5 + 0.5, 1.0);
    }
    if (debug_mode < 3.5 && debug_mode > 2.5)
    {
        return float4(metallic, metallic, metallic, 1.0);
    }
    if (debug_mode < 4.5 && debug_mode > 3.5)
    {
        return float4(perceptual_roughness, perceptual_roughness, perceptual_roughness, 1.0);
    }
    if (debug_mode < 5.5 && debug_mode > 4.5)
    {
        return float4(ambient_occlusion, ambient_occlusion, ambient_occlusion, 1.0);
    }
    if (debug_mode < 6.5 && debug_mode > 5.5)
    {
        return float4(ResolveEmissive(i.uv), 1.0);
    }
    if (debug_mode < 7.5 && debug_mode > 6.5)
    {
        return float4(final_shadow_factor, final_shadow_factor, final_shadow_factor, 1.0);
    }
    if (debug_mode < 8.5 && debug_mode > 7.5)
    {
        float3 cascade_colors[4] =
        {
            float3(0.1, 0.6, 1.0),
            float3(0.2, 1.0, 0.4),
            float3(1.0, 0.85, 0.15),
            float3(1.0, 0.25, 0.2)
        };
        return float4(cascade_index >= 0 ? cascade_colors[cascade_index] : float3(0.0, 0.0, 0.0), 1.0);
    }
    if (debug_mode < 9.5 && debug_mode > 8.5)
    {
        float normalized_count = saturate(g_PointLightParams.x / 8.0);
        return float4(normalized_count, light_enabled, g_PointShadowParams.x / 6.0, 1.0);
    }
    if (debug_mode < 10.5 && debug_mode > 9.5)
    {
        return float4(reflection_probe_influence.xxx, 1.0);
    }
    if (debug_mode < 11.5 && debug_mode > 10.5)
    {
        return float4(reflection_probe_influence, g_ReflectionProbeParams.x, saturate(g_ReflectionProbeParams.y / 1000.0), 1.0);
    }
    if (debug_mode < 12.5 && debug_mode > 11.5)
    {
        float specular_lod = perceptual_roughness * ibl_max_specular_lod;
        float normalized_lod = ibl_max_specular_lod > 0.0 ? saturate(specular_lod / ibl_max_specular_lod) : 0.0;
        return float4(normalized_lod, ibl_texture_weight, saturate(ibl_max_specular_lod / 8.0), 1.0);
    }

    return float4(max(color, float3(0.0, 0.0, 0.0)), base.a);
}
)HLSL";

    inline constexpr const char *kCompositeVS = R"HLSL(
struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

void main(uint vertex_id : SV_VertexID, out VSOutput o)
{
    float2 positions[3] =
    {
        float2(-1.0, -1.0),
        float2(-1.0,  3.0),
        float2( 3.0, -1.0)
    };

    float2 uvs[3] =
    {
        float2(0.0, 1.0),
        float2(0.0, -1.0),
        float2(2.0, 1.0)
    };

    o.pos = float4(positions[vertex_id], 0.0, 1.0);
    o.uv  = uvs[vertex_id];
}
)HLSL";

    inline constexpr const char *kCompositePS = R"HLSL(
Texture2D g_SceneColor;
SamplerState g_SceneColor_sampler;

cbuffer Composite : register(b0)
{
    float4 g_CompositeParams;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float3 ToneMapReinhard(float3 color)
{
    return color / (color + 1.0);
}

float3 ToneMapAcesFitted(float3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
}

float4 main(VSOutput i) : SV_TARGET
{
    float4 scene = g_SceneColor.Sample(g_SceneColor_sampler, i.uv);
    float3 color = max(scene.rgb, 0.0) * max(g_CompositeParams.x, 0.0);
    float mode = g_CompositeParams.y;

    if (mode < 0.5)
    {
        color = saturate(color);
    }
    else if (mode < 1.5)
    {
        color = saturate(ToneMapReinhard(color));
    }
    else
    {
        color = ToneMapAcesFitted(color);
    }

    return float4(color, scene.a);
}
)HLSL";

    inline constexpr const char *kDepthVisualizationPS = R"HLSL(
Texture2D<float> g_DepthTexture;
SamplerState g_DepthTexture_sampler;

cbuffer DepthVisualization : register(b0)
{
    float4 g_DepthParams;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float4 main(VSOutput i) : SV_TARGET
{
    float value = g_DepthTexture.Sample(g_DepthTexture_sampler, i.uv);
    value = saturate(value * g_DepthParams.x + g_DepthParams.y);
    value = pow(value, max(g_DepthParams.z, 0.0001));

    if (g_DepthParams.w > 0.5)
    {
        value = 1.0 - value;
    }

    return float4(value, value, value, 1.0);
}
)HLSL";

    inline constexpr const char *kPbrShadowDepthVS = R"HLSL(
cbuffer PerFrame : register(b0)
{
    float4x4 g_ViewProj;
};

cbuffer PerObject : register(b1)
{
    float4x4 g_Model;
};

struct VSInput
{
    float3 pos   : ATTRIB0;
    float3 norm  : ATTRIB1;
    float3 color : ATTRIB2;
    float2 uv    : ATTRIB3;
    float4 tangent : ATTRIB4;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
};

void main(in VSInput i, out VSOutput o)
{
    o.pos = mul(g_ViewProj, mul(g_Model, float4(i.pos, 1.0)));
}
)HLSL";

    inline constexpr const char *kPbrEquirectToCubePS = R"HLSL(
Texture2D g_EquirectangularTexture;
SamplerState g_IblSampler;

cbuffer IblGenerate : register(b0)
{
    float4 g_IblParams;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float3 IblFaceDirection(float2 uv, int face)
{
    float2 p = uv * 2.0 - 1.0;
    if (face == 0) return normalize(float3(1.0, -p.y, -p.x));
    if (face == 1) return normalize(float3(-1.0, -p.y, p.x));
    if (face == 2) return normalize(float3(p.x, 1.0, p.y));
    if (face == 3) return normalize(float3(p.x, -1.0, -p.y));
    if (face == 4) return normalize(float3(p.x, -p.y, 1.0));
    return normalize(float3(-p.x, -p.y, -1.0));
}

float2 DirectionToEquirectUv(float3 dir)
{
    const float inv_atan_x = 0.15915494309;
    const float inv_atan_y = 0.31830988618;
    return float2(atan2(dir.x, dir.z) * inv_atan_x + 0.5, 0.5 - asin(clamp(dir.y, -1.0, 1.0)) * inv_atan_y);
}

float4 main(VSOutput i) : SV_TARGET
{
    float3 dir = IblFaceDirection(i.uv, (int)g_IblParams.x);
    return max(g_EquirectangularTexture.SampleLevel(g_IblSampler, DirectionToEquirectUv(dir), 0.0), 0.0);
}
)HLSL";

    inline constexpr const char *kPbrIrradianceCubePS = R"HLSL(
TextureCube g_EnvironmentCube;
SamplerState g_IblSampler;

cbuffer IblGenerate : register(b0)
{
    float4 g_IblParams;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float3 IblFaceDirection(float2 uv, int face)
{
    float2 p = uv * 2.0 - 1.0;
    if (face == 0) return normalize(float3(1.0, -p.y, -p.x));
    if (face == 1) return normalize(float3(-1.0, -p.y, p.x));
    if (face == 2) return normalize(float3(p.x, 1.0, p.y));
    if (face == 3) return normalize(float3(p.x, -1.0, -p.y));
    if (face == 4) return normalize(float3(p.x, -p.y, 1.0));
    return normalize(float3(-p.x, -p.y, -1.0));
}

static const float PI = 3.14159265359;
static const uint IRRADIANCE_SAMPLE_COUNT = 96u;

float RadicalInverseVdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

float2 Hammersley(uint sample_index, uint sample_count)
{
    return float2(float(sample_index) / float(sample_count), RadicalInverseVdC(sample_index));
}

float3 TangentToWorld(float3 local_dir, float3 normal)
{
    float3 up = abs(normal.y) < 0.999 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(up, normal));
    float3 bitangent = cross(normal, tangent);
    return normalize(tangent * local_dir.x + bitangent * local_dir.y + normal * local_dir.z);
}

float4 main(VSOutput i) : SV_TARGET
{
    float3 n = IblFaceDirection(i.uv, (int)g_IblParams.x);
    float3 irradiance = float3(0.0, 0.0, 0.0);
    [loop]
    for (uint sample_index = 0u; sample_index < IRRADIANCE_SAMPLE_COUNT; ++sample_index)
    {
        float2 xi = Hammersley(sample_index, IRRADIANCE_SAMPLE_COUNT);
        float phi = 2.0 * PI * xi.x;
        float cos_theta = sqrt(max(1.0 - xi.y, 0.0));
        float sin_theta = sqrt(max(xi.y, 0.0));
        float3 local_dir = float3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
        float3 sample_dir = TangentToWorld(local_dir, n);
        irradiance += max(g_EnvironmentCube.SampleLevel(g_IblSampler, sample_dir, 0.0).rgb, 0.0);
    }

    irradiance *= PI / float(IRRADIANCE_SAMPLE_COUNT);
    return float4(max(irradiance, 0.0), 1.0);
}
)HLSL";

    inline constexpr const char *kPbrPrefilteredSpecularCubePS = R"HLSL(
TextureCube g_EnvironmentCube;
SamplerState g_IblSampler;

cbuffer IblGenerate : register(b0)
{
    float4 g_IblParams;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float3 IblFaceDirection(float2 uv, int face)
{
    float2 p = uv * 2.0 - 1.0;
    if (face == 0) return normalize(float3(1.0, -p.y, -p.x));
    if (face == 1) return normalize(float3(-1.0, -p.y, p.x));
    if (face == 2) return normalize(float3(p.x, 1.0, p.y));
    if (face == 3) return normalize(float3(p.x, -1.0, -p.y));
    if (face == 4) return normalize(float3(p.x, -p.y, 1.0));
    return normalize(float3(-p.x, -p.y, -1.0));
}

static const float PI = 3.14159265359;
static const uint SPECULAR_SAMPLE_COUNT = 128u;

float RadicalInverseVdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

float2 Hammersley(uint sample_index, uint sample_count)
{
    return float2(float(sample_index) / float(sample_count), RadicalInverseVdC(sample_index));
}

float3 TangentToWorld(float3 local_dir, float3 normal)
{
    float3 up = abs(normal.y) < 0.999 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(up, normal));
    float3 bitangent = cross(normal, tangent);
    return normalize(tangent * local_dir.x + bitangent * local_dir.y + normal * local_dir.z);
}

float3 ImportanceSampleGGX(float2 xi, float3 normal, float perceptual_roughness)
{
    float alpha = max(perceptual_roughness * perceptual_roughness, 0.001);
    float alpha2 = alpha * alpha;
    float phi = 2.0 * PI * xi.x;
    float cos_theta = sqrt(max((1.0 - xi.y) / max(1.0 + (alpha2 - 1.0) * xi.y, 1.0e-5), 0.0));
    float sin_theta = sqrt(max(1.0 - cos_theta * cos_theta, 0.0));
    float3 h = float3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
    return TangentToWorld(h, normal);
}

float4 main(VSOutput i) : SV_TARGET
{
    float3 r = IblFaceDirection(i.uv, (int)g_IblParams.x);
    float perceptual_roughness = saturate(g_IblParams.z);
    float3 n = r;
    float3 v = r;
    float3 color = float3(0.0, 0.0, 0.0);
    float weight_sum = 0.0;

    [loop]
    for (uint sample_index = 0u; sample_index < SPECULAR_SAMPLE_COUNT; ++sample_index)
    {
        float2 xi = Hammersley(sample_index, SPECULAR_SAMPLE_COUNT);
        float3 h = ImportanceSampleGGX(xi, n, perceptual_roughness);
        float3 l = normalize(2.0 * dot(v, h) * h - v);
        float NoL = saturate(dot(n, l));
        if (NoL > 0.0)
        {
            color += max(g_EnvironmentCube.SampleLevel(g_IblSampler, l, 0.0).rgb, 0.0) * NoL;
            weight_sum += NoL;
        }
    }

    color /= max(weight_sum, 1.0e-5);
    return float4(max(color, 0.0), 1.0);
}
)HLSL";

    inline constexpr const char *kPbrBrdfLutPS = R"HLSL(
struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

static const float PI = 3.14159265359;
static const uint BRDF_SAMPLE_COUNT = 128u;

float RadicalInverseVdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

float2 Hammersley(uint sample_index, uint sample_count)
{
    return float2(float(sample_index) / float(sample_count), RadicalInverseVdC(sample_index));
}

float3 ImportanceSampleGGX(float2 xi, float perceptual_roughness)
{
    float alpha = max(perceptual_roughness * perceptual_roughness, 0.001);
    float alpha2 = alpha * alpha;
    float phi = 2.0 * PI * xi.x;
    float cos_theta = sqrt(max((1.0 - xi.y) / max(1.0 + (alpha2 - 1.0) * xi.y, 1.0e-5), 0.0));
    float sin_theta = sqrt(max(1.0 - cos_theta * cos_theta, 0.0));
    return float3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
}

float GeometrySchlickGGX(float NoV, float perceptual_roughness)
{
    float alpha = max(perceptual_roughness * perceptual_roughness, 0.001);
    float k = alpha * 0.5;
    return NoV / max(NoV * (1.0 - k) + k, 1.0e-5);
}

float GeometrySmith(float NoV, float NoL, float perceptual_roughness)
{
    return GeometrySchlickGGX(NoV, perceptual_roughness) * GeometrySchlickGGX(NoL, perceptual_roughness);
}

float2 IntegrateBRDF(float NoV, float perceptual_roughness)
{
    float3 v = float3(sqrt(max(1.0 - NoV * NoV, 0.0)), 0.0, NoV);
    float a = 0.0;
    float b = 0.0;

    [loop]
    for (uint sample_index = 0u; sample_index < BRDF_SAMPLE_COUNT; ++sample_index)
    {
        float2 xi = Hammersley(sample_index, BRDF_SAMPLE_COUNT);
        float3 h = ImportanceSampleGGX(xi, perceptual_roughness);
        float3 l = normalize(2.0 * dot(v, h) * h - v);
        float NoL = saturate(l.z);
        float NoH = saturate(h.z);
        float VoH = saturate(dot(v, h));
        if (NoL > 0.0)
        {
            float g = GeometrySmith(NoV, NoL, perceptual_roughness);
            float g_vis = (g * VoH) / max(NoH * NoV, 1.0e-5);
            float fc = pow(1.0 - VoH, 5.0);
            a += (1.0 - fc) * g_vis;
            b += fc * g_vis;
        }
    }

    return float2(a, b) / float(BRDF_SAMPLE_COUNT);
}

float4 main(VSOutput i) : SV_TARGET
{
    return float4(max(IntegrateBRDF(saturate(i.uv.x), saturate(i.uv.y)), 0.0), 0.0, 1.0);
}
)HLSL";

    inline constexpr const char *kPbrSkyboxPS = R"HLSL(
TextureCube g_SkyboxCube;
SamplerState g_SkyboxSampler;

cbuffer Skybox : register(b0)
{
    float4 g_SkyboxCameraRightTanX;
    float4 g_SkyboxCameraUpTanY;
    float4 g_SkyboxCameraForwardIntensity;
    float4 g_SkyboxFallbackHorizon;
    float4 g_SkyboxFallbackZenith;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float4 main(VSOutput i) : SV_TARGET
{
    float2 ndc = float2(i.uv.x * 2.0 - 1.0, (1.0 - i.uv.y) * 2.0 - 1.0);
    float3 direction = normalize(g_SkyboxCameraForwardIntensity.xyz +
                                 g_SkyboxCameraRightTanX.xyz * (ndc.x * g_SkyboxCameraRightTanX.w) +
                                 g_SkyboxCameraUpTanY.xyz * (ndc.y * g_SkyboxCameraUpTanY.w));
    float3 radiance = max(g_SkyboxCube.SampleLevel(g_SkyboxSampler, direction, 0.0).rgb, 0.0);
    return float4(radiance * max(g_SkyboxCameraForwardIntensity.w, 0.0), 1.0);
}
)HLSL";

    inline constexpr const char *kPbrSkyboxFallbackPS = R"HLSL(
cbuffer Skybox : register(b0)
{
    float4 g_SkyboxCameraRightTanX;
    float4 g_SkyboxCameraUpTanY;
    float4 g_SkyboxCameraForwardIntensity;
    float4 g_SkyboxFallbackHorizon;
    float4 g_SkyboxFallbackZenith;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float4 main(VSOutput i) : SV_TARGET
{
    float2 ndc = float2(i.uv.x * 2.0 - 1.0, (1.0 - i.uv.y) * 2.0 - 1.0);
    float3 direction = normalize(g_SkyboxCameraForwardIntensity.xyz +
                                 g_SkyboxCameraRightTanX.xyz * (ndc.x * g_SkyboxCameraRightTanX.w) +
                                 g_SkyboxCameraUpTanY.xyz * (ndc.y * g_SkyboxCameraUpTanY.w));
    float sky = saturate(direction.y * 0.5 + 0.5);
    float3 radiance = lerp(g_SkyboxFallbackHorizon.rgb, g_SkyboxFallbackZenith.rgb, sky);
    return float4(radiance * max(g_SkyboxCameraForwardIntensity.w, 0.0), 1.0);
}
)HLSL";

    inline constexpr const char *kPbrDebugTexture2DPS = R"HLSL(
Texture2D g_DebugTexture;
SamplerState g_DebugSampler;

cbuffer DebugTexture : register(b0)
{
    float4 g_DebugTextureParams;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float4 main(VSOutput i) : SV_TARGET
{
    return g_DebugTexture.SampleLevel(g_DebugSampler, i.uv, g_DebugTextureParams.z);
}
)HLSL";

    inline constexpr const char *kPbrDebugTexture2DArrayPS = R"HLSL(
Texture2DArray<float> g_DebugTexture;
SamplerState g_DebugSampler;

cbuffer DebugTexture : register(b0)
{
    float4 g_DebugTextureParams;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float4 main(VSOutput i) : SV_TARGET
{
    float value = g_DebugTexture.SampleLevel(g_DebugSampler, float3(i.uv, g_DebugTextureParams.x),
                                             g_DebugTextureParams.z);
    return float4(value, value, value, 1.0);
}
)HLSL";

    inline constexpr const char *kPbrDebugTextureCubePS = R"HLSL(
TextureCube g_DebugTexture;
SamplerState g_DebugSampler;

cbuffer DebugTexture : register(b0)
{
    float4 g_DebugTextureParams;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float3 FaceDirection(float2 uv, int face)
{
    float2 p = uv * 2.0 - 1.0;
    if (face == 0) return normalize(float3(1.0, -p.y, -p.x));
    if (face == 1) return normalize(float3(-1.0, -p.y, p.x));
    if (face == 2) return normalize(float3(p.x, 1.0, p.y));
    if (face == 3) return normalize(float3(p.x, -1.0, -p.y));
    if (face == 4) return normalize(float3(p.x, -p.y, 1.0));
    return normalize(float3(-p.x, -p.y, -1.0));
}

float4 main(VSOutput i) : SV_TARGET
{
    int face = (int)g_DebugTextureParams.y;
    return g_DebugTexture.SampleLevel(g_DebugSampler, FaceDirection(i.uv, face), g_DebugTextureParams.z);
}
)HLSL";
} // namespace CoreEngine::BuiltinShaders
