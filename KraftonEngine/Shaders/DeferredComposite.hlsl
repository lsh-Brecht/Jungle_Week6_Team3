#include "Common/Functions.hlsl"
#include "Common/ConstantBuffers.hlsl"

Texture2D AlbedoTex : register(t0);
Texture2D NormalTex : register(t1);

struct PS_Input
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

PS_Input VS(uint vertexID : SV_VertexID)
{
    PS_Input output;
    output.uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return output;
}

float4 PS(PS_Input input) : SV_TARGET
{
    int2 coord = int2(input.position.xy);
    float4 albedo = AlbedoTex.Load(int3(coord, 0));
    float3 normal = NormalTex.Load(int3(coord, 0)).xyz * 2.0f - 1.0f;

    float normalWeight = saturate(normal.z * 0.25f + 0.75f);
    float3 color = albedo.rgb * normalWeight;
    return float4(ApplyWireframe(color), albedo.a);
}
