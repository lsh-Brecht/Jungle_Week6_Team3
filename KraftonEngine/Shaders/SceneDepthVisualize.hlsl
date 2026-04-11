#include "Common/ConstantBuffers.hlsl"

Texture2D<float> DepthTex : register(t0);
SamplerState PointSampler : register(s0);

struct PS_Input
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

PS_Input VS(uint vertexID : SV_VertexID)
{
    PS_Input output;
    output.uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return output;
}

float4 PS(PS_Input input) : SV_Target
{
    float d = DepthTex.Sample(PointSampler, input.uv);
    float denom = max(FarPlane - d * (FarPlane - NearPlane), 1e-6f);
    float viewDepth = (NearPlane * FarPlane) / denom;

    // Visualize with a logarithmic ramp so mid/far depth keeps visible contrast.
    float depth01 = saturate(log2(1.0f + viewDepth) / log2(1.0f + FarPlane));
    float gray = 1.0f - depth01;
    return float4(gray, gray, gray, 1.0);
}
