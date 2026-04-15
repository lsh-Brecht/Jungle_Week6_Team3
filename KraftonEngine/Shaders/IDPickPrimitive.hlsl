#include "Common/Functions.hlsl"
#include "Common/VertexLayouts.hlsl"
#include "Common/ConstantBuffers.hlsl"

struct VS_Output_ID
{
    float4 position : SV_POSITION;
};

VS_Output_ID VS(VS_Input_PC input)
{
    VS_Output_ID output;
    output.position = ApplyMVP(input.position);
    return output;
}

uint PS(VS_Output_ID input) : SV_TARGET
{
    return (uint)(PrimitiveColor.x + 0.5f);
}

