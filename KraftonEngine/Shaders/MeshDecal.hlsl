#include "Common/Functions.hlsl"
#include "Common/VertexLayouts.hlsl"
#include "Common/ConstantBuffers.hlsl"

Texture2D g_txColor : register(t0);
SamplerState g_Sample : register(s0);

PS_Input_Full VS(VS_Input_PNCT input)
{
    PS_Input_Full output;
    output.position = ApplyMVP(input.position);
    output.normal = normalize(mul(input.normal, (float3x3)Model));
    output.color = input.color * SectionColor;
    output.worldPos = mul(float4(input.position, 1.0f), Model).xyz;
    output.texcoord = input.texcoord;
    return output;
}

float4 PS(PS_Input_Full input) : SV_TARGET
{
    const float4 texColor = g_txColor.Sample(g_Sample, input.texcoord);
    return texColor * input.color;
}

