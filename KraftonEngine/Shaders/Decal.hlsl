#include "Common/Functions.hlsl"
#include "Common/VertexLayouts.hlsl"
#include "Common/ConstantBuffers.hlsl"

Texture2D g_DecalTexture : register(t0);
Texture2D g_SceneDepth : register(t1);
SamplerState g_Sample : register(s0);

cbuffer DecalBuffer : register(b7)
{
    float4x4 WorldToDecal;
    float4 DecalColor;
}

PS_Input_Color VS(VS_Input_PC input)
{
    PS_Input_Color output;
    output.position = ApplyMVP(input.position);
    output.color = input.color;
    return output;
}

float4 PS(PS_Input_Color input) : SV_TARGET
{
    uint width;
    uint height;
    g_SceneDepth.GetDimensions(width, height);

    float2 screenUV = input.position.xy / float2(width, height);
    int2 pixel = int2(input.position.xy);
    float depth = g_SceneDepth.Load(int3(pixel, 0)).r;
    if (depth >= 1.0f)
    {
        discard;
    }

    float2 ndcXY = screenUV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
    float4 clip = float4(ndcXY, depth, 1.0f);
    float4 worldH = mul(clip, InverseViewProjection);
    if (abs(worldH.w) <= 0.00001f)
    {
        discard;
    }
    float invW = rcp(worldH.w);
    float3 worldPos = worldH.xyz * invW;
    float3 decalLocal = mul(float4(worldPos, 1.0f), WorldToDecal).xyz;

    if (any(abs(decalLocal) > 0.5f))
    {
        discard;
    }

    float2 decalUV = decalLocal.yz + 0.5f;
    decalUV.y = 1.0f - decalUV.y;

    float4 decal = g_DecalTexture.Sample(g_Sample, decalUV) * DecalColor;
    if (decal.a <= 0.001f)
    {
        discard;
    }

    return float4(ApplyWireframe(decal.rgb), decal.a);
}
