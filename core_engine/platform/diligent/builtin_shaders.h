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

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float4 main(VSOutput i) : SV_TARGET
{
    return g_SceneColor.Sample(g_SceneColor_sampler, i.uv);
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
