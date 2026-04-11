#include "Common/ConstantBuffers.hlsl"

Texture2D<float4> SceneColorTex : register(t0);
Texture2D<float4> OutlineMaskTex : register(t1);

struct PS_Input
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

PS_Input VS(uint vertexID : SV_VertexID)
{
    PS_Input output;
    output.uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return output;
}

float4 PS(PS_Input input) : SV_TARGET
{
    const int2 coord = int2(input.position.xy);
    const float4 sceneColor = SceneColorTex.Load(int3(coord, 0));
    const float centerMask = OutlineMaskTex.Load(int3(coord, 0)).a;

    // 내부 채우기 대신 외곽선만 그린다.
    if (centerMask > 0.0f)
    {
        return sceneColor;
    }

    const int radius = max((int)OutlineThickness, 1);
    float edgeAlpha = 0.0f;

    [loop]
    for (int y = -radius; y <= radius; ++y)
    {
        [loop]
        for (int x = -radius; x <= radius; ++x)
        {
            if (x == 0 && y == 0)
            {
                continue;
            }

            const float neighborMask = OutlineMaskTex.Load(int3(coord + int2(x, y), 0)).a;
            if (neighborMask <= 0.0f)
            {
                continue;
            }

            const float dist = sqrt((float)(x * x + y * y));
            const float normDist = saturate(dist / max((float)radius, 0.0001f));
            const float falloff = max(OutlineFalloff, 0.01f);
            const float fade = pow(saturate(1.0f - normDist), falloff);
            edgeAlpha = max(edgeAlpha, fade);
        }
    }

    if (edgeAlpha <= 0.0f)
    {
        return sceneColor;
    }

    const float outlineAlpha = saturate(edgeAlpha * OutlineColor.a);
    const float3 blended = lerp(sceneColor.rgb, OutlineColor.rgb, outlineAlpha);
    return float4(blended, sceneColor.a);
}
