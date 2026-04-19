#pragma once

namespace CoreEngine::BuiltinShaders {

inline constexpr const char *kUnlitVS = R"HLSL(
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
};

struct PSInput
{
    float4 pos   : SV_POSITION;
    float3 color : COLOR0;
    float2 uv    : TEXCOORD0;
};

void main(in VSInput i, out PSInput o)
{
    float4 world = mul(g_Model, float4(i.pos, 1.0));
    o.pos        = mul(g_ViewProj, world);
    o.color      = i.color;
    o.uv         = i.uv;
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
    float2 uv    : TEXCOORD0;
};

float4 main(in PSInput i) : SV_TARGET
{
    return float4(i.color, 1.0) * g_Tint;
}
)HLSL";

} // namespace CoreEngine::BuiltinShaders
