#include "Common/Functions.hlsl"
#include "Common/VertexLayouts.hlsl"
#include "Common/ConstantBuffers.hlsl"

Texture2D g_txColor : register(t0);
SamplerState g_Sample : register(s0);

struct PS_Input_Decal
{
    float4 position : SV_POSITION;
    float4 color    : COLOR;
    float3 localPos : TEXCOORD0;
    float3 localNormal : TEXCOORD1;
};

PS_Input_Decal VS(VS_Input_PNCT input)
{
    PS_Input_Decal output;

    /*
        CPU uploads the SAT-filtered receiver meshes in decal-local space.
        The vertex position is already in decal-local coordinates, so VS only does MVP.
    */
    output.position = ApplyMVP(input.position);
    output.color = input.color * SectionColor * PrimitiveColor;
    output.localPos = input.position;
    output.localNormal = normalize(input.normal);
    return output;
}

float4 PS(PS_Input_Decal input) : SV_TARGET
{
    /*
        This is not a deferred path.
        CPU only chooses candidate meshes with OBB-vs-AABB SAT.
        Final decal box masking happens here with per-pixel discard.
    */
    if (abs(input.localPos.x) > 0.5f || abs(input.localPos.y) > 0.5f || abs(input.localPos.z) > 0.5f)
    {
        discard;
    }

    /*
        Projection itself is driven by decal-local position, not by normals.
        A hard normal discard created visible holes on overlapping receivers because
        neighboring triangles could fall on different sides of the threshold.

        Keep only a mild alpha fade for grazing angles so we preserve coverage first.
        In decal-local space, +X is the projector axis, so surfaces facing -X get full alpha.
    */
    const float projectorFacing = saturate(-input.localNormal.x);
    const float facingFade = lerp(0.65f, 1.0f, projectorFacing);

    const float2 decalUV = float2(input.localPos.y + 0.5f, 0.5f - input.localPos.z);
    if (decalUV.x < 0.0f || decalUV.x > 1.0f || decalUV.y < 0.0f || decalUV.y > 1.0f)
    {
        discard;
    }

    float4 texColor = g_txColor.Sample(g_Sample, decalUV);
    if (texColor.a < 0.001f)
    {
        discard;
    }

    float4 finalColor = texColor * input.color;
    finalColor.a *= facingFade;
    finalColor.rgb = saturate(finalColor.rgb);
    finalColor.rgb = lerp(finalColor.rgb, WireframeRGB, bIsWireframe);
    return finalColor;
}
