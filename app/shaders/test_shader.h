#pragma once

namespace Game::Shaders {
    inline constexpr const char *kTestVS = R"HLSL(
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
    o.time       = g_frameTime;
    float4 world = mul(g_Model, float4(i.pos, 1.0));
    o.pos        = mul(g_ViewProj, world);
    o.color      = i.color;
    o.norm       = i.norm;
    o.uv         = i.uv;
}
)HLSL";
    inline constexpr const char *kTestPS = R"HLSL(
Texture2D g_Albedo;
SamplerState g_Albedo_sampler;

cbuffer PerMaterial : register(b2)
{
    float4 g_Tint;
    float alpha;
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
    float4 albedo_sample = g_Albedo.Sample(g_Albedo_sampler, i.uv);
    float4 new_color = float4(i.uv.x,0.0,0.0,1.0);
    float new_alpha = (sin(i.time.y) * 0.5 + 0.5);
    float lerp_offset = lerp(0.0,0.2, new_alpha);
    float new_value = smoothstep(0.8 - lerp_offset,0.9 - lerp_offset, 1.0 - length(i.uv - 0.5)) * abs(i.norm.z);
    return float4(albedo_sample.xyz, alpha);
}
)HLSL";
} // namespace CoreEngine::BuiltinShaders
