#include "Common/ConstantBuffers.hlsl"

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

float LinearizeViewDepth(float depth)
{
    const float proj33 = Projection[2][2];
    const float proj43 = Projection[3][2];
    return proj43 / min(depth - proj33, -0.0001f);
}

float ComputeFogWeight(FogUniformParameters fog, float depth, float3 worldPos)
{
    const float fogDensity = max(fog.ExponentialFogParameters.x, 0.0f);
    const float scaledFogDensity = fogDensity * 0.1f;
    const float fogHeightFalloff = max(fog.ExponentialFogParameters.y, 0.0001f);
    const float startDistance = max(fog.ExponentialFogParameters.w, 0.0f);
    const float fogHeight = fog.ExponentialFogParameters3.y;
    const float cutoffDistance = max(fog.ExponentialFogParameters3.w, 0.0f);
    const float maxOpacity = saturate(1.0f - fog.ExponentialFogColorParameter.a);

    const float distanceToCamera = max(LinearizeViewDepth(depth), 0.0f);
    if (distanceToCamera <= startDistance)
    {
        return 0.0f;
    }

    if (cutoffDistance > 0.0f && distanceToCamera >= cutoffDistance)
    {
        return 0.0f;
    }

    const float effectiveDistance = distanceToCamera - startDistance;
    const float sampleHeight = min(worldPos.z, CameraPosition.z);
    const float relativeHeight = max(sampleHeight - fogHeight, 0.0f);
    const float heightAttenuation = exp(-relativeHeight * fogHeightFalloff);
    const float fogIntegral = effectiveDistance * scaledFogDensity * heightAttenuation;
    return min(saturate(1.0f - exp(-fogIntegral)), maxOpacity);
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
            const float fogDensity = max(Fogs[i].ExponentialFogParameters.x, 0.0f) * 0.1f;
            const float fogHeightFalloff = max(Fogs[i].ExponentialFogParameters.y, 0.0001f);
            const float fogHeight = Fogs[i].ExponentialFogParameters3.y;
            const float maxOpacity = saturate(1.0f - Fogs[i].ExponentialFogColorParameter.a);

            const float fakeDistance = 1000.0f;
            const float cameraRelativeHeight = max(CameraPosition.z - fogHeight, 0.0f);
            const float cameraHeightAttenuation = exp(-cameraRelativeHeight * fogHeightFalloff);
            const float transmittance = saturate(exp(-fogDensity * cameraHeightAttenuation * fakeDistance));
            const float fogWeight = min(1.0f - transmittance, maxOpacity);

            finalColor = lerp(finalColor, Fogs[i].ExponentialFogColorParameter.rgb, fogWeight);
        }

        return float4(finalColor, sourceColor.a);
    }

    const float3 worldPos = ReconstructWorldPosition(input.uv, depth);

    float3 finalColor = sourceColor.rgb;
    [loop]
    for (uint i = 0; i < FogCount; ++i)
    {
        const float fogWeight = ComputeFogWeight(Fogs[i], depth, worldPos);
        finalColor = lerp(finalColor, Fogs[i].ExponentialFogColorParameter.rgb, fogWeight);
    }

    return float4(finalColor, sourceColor.a);
}
