#include "Common/Functions.hlsl"
#include "Common/VertexLayouts.hlsl"
#include "Common/ConstantBuffers.hlsl"

Texture2D g_txColor : register(t0);
SamplerState g_Sample : register(s0);

struct VS_Output
{
    float4 position : SV_POSITION;
    float3 normal   : NORMAL;
    float4 color    : COLOR;
    float2 texcoord : TEXCOORD;
};

struct PS_Output
{
    float4 albedo : SV_TARGET0;
    float4 normal : SV_TARGET1;
};

VS_Output VS(VS_Input_PNCT input)
{
    VS_Output output;
    output.position = ApplyMVP(input.position);
    output.normal = normalize(mul(input.normal, (float3x3) Model));
    output.color = input.color * SectionColor;

    float2 texcoord = input.texcoord;
    if (bIsUVScroll != 0)
    {
        texcoord.x += Time * 0.5f;
    }
    output.texcoord = texcoord;

    return output;
}

PS_Output PS(VS_Output input)
{
    float4 texColor = g_txColor.Sample(g_Sample, input.texcoord);
    if (texColor.a < 0.001f)
    {
        texColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    float4 finalColor = texColor * input.color;
    finalColor.a = texColor.a * input.color.a;

    PS_Output output;
    output.albedo = finalColor;
    output.normal = float4(normalize(input.normal) * 0.5f + 0.5f, finalColor.a);
    return output;
}
