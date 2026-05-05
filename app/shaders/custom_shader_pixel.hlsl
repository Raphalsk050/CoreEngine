// #pragma hlsl profile ps_6_6
// #pragma hlsl entry main

Texture2D g_Albedo;
SamplerState g_Albedo_sampler;

cbuffer PerMaterial : register(b2) {
  float4 g_Tint;
  float g_Alpha;
};

struct PSInput {
  float4 pos : SV_POSITION;
  float3 color : COLOR0;
  float3 norm : TEXCOORD2;
  float2 uv : TEXCOORD0;
  float4 time : TEXCOORD1;
};

float4 main(in PSInput i) : SV_TARGET {
  float4 albedoSample = g_Albedo.Sample(g_Albedo_sampler, i.uv);

  float4 debugColor = float4(i.uv.x, 0.0, 0.0, 1.0);

  float sinFactor = sin(i.time.y) * 0.5 + 0.5;

  float lerpOffset = lerp(0.0, 0.2, sinFactor);

  float mask =
      smoothstep(0.8 - lerpOffset, 0.9 - lerpOffset, 1.0 - length(i.uv - 0.5)) *
      abs(i.norm.z);

  float3 finalColor = i.color * abs(i.norm) * albedoSample.xyz;
  float finalAlpha = g_Alpha * mask;

  return float4(finalColor * mask, finalAlpha);
}
