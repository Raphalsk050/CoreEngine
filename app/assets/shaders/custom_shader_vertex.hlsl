// #pragma hlsl profile vs_6_6
// #pragma hlsl entry main

cbuffer PerFrame : register(b0) {
  float4x4 g_ViewProj;
  float4 g_frameTime;
};

cbuffer PerObject : register(b1) { float4x4 g_Model; };

struct VSInput {
  float3 pos : ATTRIB0;
  float3 norm : ATTRIB1;
  float3 color : ATTRIB2;
  float2 uv : ATTRIB3;
  float4 time : ATTRIB4;
};

struct PSInput {
  float4 pos : SV_POSITION;
  float3 color : COLOR0;
  float3 norm : COLOR1;
  float2 uv : TEXCOORD0;
  float4 time : TEXCOORD1;
};

void main(in VSInput i, out PSInput o) {
  o.time = g_frameTime;
  float4 world = mul(g_Model, float4(i.pos, 1.0));
  o.pos = mul(g_ViewProj, world);
  o.color = i.color;
  o.norm = i.norm;
  o.uv = i.uv;
}
