#include "Common/Functions.hlsl"
#include "Common/VertexLayouts.hlsl"
#include "Common/ConstantBuffers.hlsl"

PS_Input_Full VS(VS_Input_PNCT input)
{
    PS_Input_Full output;
    output.position = ApplyMVP(input.position);
    output.normal = 0.0f;
    output.color = 0.0f;
    output.texcoord = 0.0f;
    return output;
}

float4 PS(PS_Input_Full input) : SV_TARGET
{
    return float4(0.0f, 0.0f, 0.0f, 0.0f);
}
