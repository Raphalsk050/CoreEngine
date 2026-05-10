// #pragma hlsl profile ps_6_6
// #pragma hlsl entry main

Texture2D g_Albedo;
SamplerState g_Albedo_sampler;

cbuffer PerMaterial : register(b2) {
  float4 g_Tint;
  float alpha;
};

struct PSInput {
  float4 pos : SV_POSITION;
  float3 color : COLOR0;
  float3 norm : COLOR1;
  float2 uv : TEXCOORD0;
  float4 time : TEXCOORD1;
};

float4 main(in PSInput i) : SV_TARGET {
  float4 albedo_sample = g_Albedo.Sample(g_Albedo_sampler, i.uv);
  return float4(albedo_sample.xyz, 1.0);
}
