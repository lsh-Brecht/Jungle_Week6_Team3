#include "Common/Functions.hlsl"
#include "Common/VertexLayouts.hlsl"
#include "Common/ConstantBuffers.hlsl"

Texture2D g_txColor : register(t0);
SamplerState g_Sample : register(s0);

struct VS_Output_ID
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

VS_Output_ID VS(VS_Input_PC input)
{
    VS_Output_ID output;
    output.position = ApplyMVP(input.position);

    // Billboard quad local space:
    // y: [-0.5, 0.5], z: [-0.5, 0.5]
    const float u = input.position.y + 0.5f;
    const float v = 0.5f - input.position.z;
    output.texcoord = float2(u, v);
    return output;
}

uint PS(VS_Output_ID input) : SV_TARGET
{
    const float alpha = g_txColor.Sample(g_Sample, input.texcoord).a;
    if (alpha <= 1e-4f)
    {
        discard;
    }

    return (uint)(PrimitiveColor.x + 0.5f);
}

