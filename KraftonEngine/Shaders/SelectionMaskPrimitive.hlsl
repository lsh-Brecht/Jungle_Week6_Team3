#include "Common/Functions.hlsl"
#include "Common/VertexLayouts.hlsl"

PS_Input_Color VS(VS_Input_PC input)
{
    PS_Input_Color output;
    output.position = ApplyMVP(input.position);
    output.color = 0.0f;
    return output;
}

float4 PS(PS_Input_Color input) : SV_TARGET
{
    return float4(0.0f, 0.0f, 0.0f, 0.0f);
}
