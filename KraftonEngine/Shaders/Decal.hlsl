#include "Common/ConstantBuffers.hlsl"
#include "Common/VertexLayouts.hlsl"

Texture2D g_txColor : register(t0);
Texture2D<float> DepthTex : register(t1);
SamplerState g_Sample : register(s0);

struct PS_Input_Decal
{
    float4 position : SV_POSITION;
    float3 localPos : TEXCOORD0;
};

cbuffer DecalObjectBuffer : register(b2)
{
    float4x4 InverseDecalModel;
}

PS_Input_Decal VS(VS_Input_PC input)
{
    PS_Input_Decal output;
    float4 world = mul(float4(input.position, 1.0f), Model);
    float4 view = mul(world, View);
    output.position = mul(view, Projection);
    output.localPos = input.position;
    return output;
}

float3 ReconstructWorldPosition(float2 uv, float depth)
{
    float2 ndcXY = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 clipPos = float4(ndcXY, depth, 1.0f);
    float4 worldPos = mul(clipPos, InverseViewProjection);
    return worldPos.xyz / max(worldPos.w, 0.0001f);
}

float4 PS(PS_Input_Decal input) : SV_TARGET
{
    // 2) Deferred decal projection using scene depth
    uint width, height;
    DepthTex.GetDimensions(width, height);

    const float2 invSize = 1.0f / float2(width, height);
    const float2 uv = (input.position.xy) * invSize;

    const float depth = DepthTex.Load(int3(input.position.xy, 0));
    if (depth <= 0.0f || depth >= 1.0f)
    {
        discard;
    }

    const float3 worldPos = ReconstructWorldPosition(uv, depth);
    const float4 localDecalPos = mul(float4(worldPos, 1.0f), InverseDecalModel);

    if (abs(localDecalPos.x) > 0.5f || abs(localDecalPos.y) > 0.5f || abs(localDecalPos.z) > 0.5f)
    {
        discard;
    }

    const float2 decalUV = float2(localDecalPos.y + 0.5f, 0.5f - localDecalPos.z);
    if (decalUV.x < 0.0f || decalUV.x > 1.0f || decalUV.y < 0.0f || decalUV.y > 1.0f)
    {
        discard;
    }

    float4 texColor = g_txColor.Sample(g_Sample, decalUV);
    if (texColor.a < 0.001f)
    {
        discard;
    }

    float4 finalColor = texColor * SectionColor * PrimitiveColor;
    finalColor.rgb = saturate(finalColor.rgb);
    finalColor.rgb = lerp(finalColor.rgb, WireframeRGB, bIsWireframe);
    
    return finalColor;
}
