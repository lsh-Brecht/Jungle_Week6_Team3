#include "Common/ConstantBuffers.hlsl"
#include "Common/FogCommon.hlsl"

Texture2D<float4> SceneColorTex : register(t0);
Texture2D<float> DepthTex : register(t1);

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

float3 ReconstructWorldPosition(float2 uv, float depth)
{
    float2 ndcXY = uv * 2.0f - 1.0f;
    float4 clipPos = float4(ndcXY, depth, 1.0f);
    float4 worldPos = mul(clipPos, InverseViewProjection);
    return worldPos.xyz / max(worldPos.w, 0.0001f);
}

float ComputeSceneFogWeight(FogUniformParameters fog, float depth, float3 worldPos)
{
    const float sampleHeight = min(worldPos.z, CameraPosition.z);
    const float heightAttenuation = ComputeFogHeightAttenuation(fog, sampleHeight);
    const float distanceToCamera = max(LinearizeFogViewDepth(depth, Projection), 0.0f);
    return ComputeFogWeightFromDistance(fog, distanceToCamera, heightAttenuation);
}

float ComputeFarDepthFogWeight(FogUniformParameters fog)
{
    const float heightAttenuation = ComputeFogHeightAttenuation(fog, CameraPosition.z);
    return ComputeFogWeightFromDistance(fog, FOG_FAR_DEPTH_DISTANCE, heightAttenuation);
}

float4 PS(PS_Input input) : SV_TARGET
{
    const int2 coord = int2(input.position.xy);
    const float4 sourceColor = SceneColorTex.Load(int3(coord, 0));

    if (FogCount == 0)
    {
        return sourceColor;
    }

    const float depth = DepthTex.Load(int3(coord, 0));
    if (depth >= 1.0f)
    {
        float3 finalColor = sourceColor.rgb;

        [loop]
        for (uint i = 0; i < FogCount; ++i)
        {
            const float fogWeight = ComputeFarDepthFogWeight(Fogs[i]);
            finalColor = lerp(finalColor, Fogs[i].ExponentialFogColorParameter.rgb, fogWeight);
        }

        return float4(finalColor, sourceColor.a);
    }

    const float3 worldPos = ReconstructWorldPosition(input.uv, depth);

    float3 finalColor = sourceColor.rgb;
    [loop]
    for (uint i = 0; i < FogCount; ++i)
    {
        const float fogWeight = ComputeSceneFogWeight(Fogs[i], depth, worldPos);
        finalColor = lerp(finalColor, Fogs[i].ExponentialFogColorParameter.rgb, fogWeight);
    }

    return float4(finalColor, sourceColor.a);
}
