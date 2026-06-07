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
    float4   g_PointLightParams;
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
    float4   g_PointLightParams;
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
    float3 direct = EvaluatePunctualLight(diffuse, f0, roughness, n, v, directional_l, directional_illuminance) *
        light_enabled;

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
        direct += EvaluatePunctualLight(diffuse, f0, roughness, n, v, point_l, point_illuminance);
    }

    float environment_enabled = g_EnvironmentLightEnabled.x > 0.5 ? 1.0 : 0.0;
    float3 environment_irradiance =
        max(g_EnvironmentLightDiffuseIntensity.rgb, 0.0) * max(g_EnvironmentLightDiffuseIntensity.w, 0.0);
    float3 environment_radiance =
        max(g_EnvironmentLightSpecularIntensity.rgb, 0.0) * max(g_EnvironmentLightSpecularIntensity.w, 0.0);
    float2 env_brdf = EnvBRDFApprox(NoV, perceptual_roughness);
    float3 environment_diffuse = diffuse * environment_irradiance;
    float3 environment_specular =
        environment_radiance * max(f0 * env_brdf.x + env_brdf.y, float3(0.0, 0.0, 0.0));
    float3 environment = (environment_diffuse + environment_specular) * environment_enabled * ambient_occlusion;
    float3 color = direct + environment + ResolveEmissive(i.uv);

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
} // namespace CoreEngine::BuiltinShaders
