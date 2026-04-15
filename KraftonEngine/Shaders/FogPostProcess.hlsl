#include "Common/ConstantBuffers.hlsl"
#include "Common/FogCommon.hlsl"

Texture2D<float4> SceneColorTex : register(t0);
Texture2D<float> DepthTex : register(t1);

// 포스트 프로세스 패스에서 픽셀 셰이더로 넘길 최소 입력입니다.
// 화면 위치와 UV만 있으면 scene color/depth를 다시 읽어 fog를 합성할 수 있습니다.
struct PS_Input
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

PS_Input VS(uint vertexID : SV_VertexID)
{
    PS_Input output;
    // 버텍스 버퍼 없이 포스트 프로세스용 full-screen triangle을 만듭니다.
    // 화면 전체를 덮는 삼각형 한 장만 사용해 후처리 패스를 단순하게 유지합니다.
    output.uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return output;
}

float3 ReconstructWorldPosition(float2 uv, float depth)
{
    // deferred 예제는 SceneWorldPos 텍스처를 직접 읽었지만,
    // 현재 프로젝트는 forward 렌더링이므로 depth와 역행렬로 월드 좌표를 복원해야 합니다.
    float2 ndcXY = uv * 2.0f - 1.0f;
    float4 clipPos = float4(ndcXY, depth, 1.0f);
    float4 worldPos = mul(clipPos, InverseViewProjection);
    return worldPos.xyz / max(worldPos.w, 0.0001f);
}

float ComputeSceneDistanceToCamera(float3 worldPos)
{
    // deferred 예제의 length(worldPos - CameraPosition.xyz)와 같은 거리입니다.
    // view-space 전방 거리보다 실제 광선 길이에 가까워 화면 가장자리에서도 더 자연스럽게 동작합니다.
    return length(worldPos - CameraPosition.xyz);
}

float ComputeSceneFogWeight(FogUniformParameters fog, float3 worldPos)
{
    // 표면 높이와 카메라 높이 중 더 낮은 높이를 사용해,
    // 시선이 fog 층을 통과하는 구간을 deferred 참고 구현과 비슷한 의미로 평가합니다.
    const float sampleHeight = min(worldPos.z, CameraPosition.z);
    const float heightAttenuation = ComputeFogHeightAttenuation(fog, sampleHeight);
    const float distanceToCamera = ComputeSceneDistanceToCamera(worldPos);
    return ComputeFogWeightFromDistance(fog, distanceToCamera, heightAttenuation);
}

float ComputeFarDepthFogWeight(FogUniformParameters fog)
{
    // deferred 참고 코드의 rawDepth >= 1.0f 분기와 같은 역할입니다.
    // far plane 밖 픽셀은 실제 표면까지의 거리가 없으므로 임의의 원거리 값을 사용합니다.
    const float heightAttenuation = ComputeFogHeightAttenuation(fog, CameraPosition.z);
    return ComputeFogWeightFromDistance(fog, FOG_FAR_DEPTH_DISTANCE, heightAttenuation);
}

float4 PS(PS_Input input) : SV_TARGET
{
    // 현재 화면 색을 읽어 fog 색과 섞는 형태의 후처리 패스입니다.
    // deferred 예제의 SceneLightColor가 이 셰이더에서는 이미 합성된 SceneColorTex에 대응합니다.
    const int2 coord = int2(input.position.xy);
    const float4 sourceColor = SceneColorTex.Load(int3(coord, 0));

    if (FogCount == 0)
    {
        // 활성화된 fog actor가 없으면 원본 장면 색을 그대로 반환합니다.
        return sourceColor;
    }

    const float depth = DepthTex.Load(int3(coord, 0));
    if (depth >= 1.0f)
    {
        // 하늘이나 far plane 픽셀은 표면 위치를 복원할 수 없으므로 고정된 먼 거리 기준으로 fog를 계산합니다.
        // deferred 예제의 fakeDist = 1000과 같은 역할이며, 배경에도 fog 색을 자연스럽게 입히기 위한 분기입니다.
        float3 finalColor = sourceColor.rgb;

        // CPU에서 선택한 Primary fog 하나만 적용합니다.
        for (uint i = 0; i < FogCount; ++i)
        {
            const float fogWeight = ComputeFarDepthFogWeight(Fogs[i]);
            finalColor = lerp(finalColor, Fogs[i].ExponentialFogColorParameter.rgb, fogWeight);
        }

        return float4(finalColor, sourceColor.a);
    }

    const float3 worldPos = ReconstructWorldPosition(input.uv, depth);

    float3 finalColor = sourceColor.rgb;
    for (uint i = 0; i < FogCount; ++i)
    {
        // forward 렌더링이라 GBuffer의 월드 좌표를 읽지 못하므로,
        // 복원한 worldPos와 카메라 위치로 실제 표면 거리 및 높이 감쇠를 계산합니다.
        // 그 뒤 deferred 예제와 동일한 형태로 최종 fog weight를 구해 scene color와 섞습니다.
        const float fogWeight = ComputeSceneFogWeight(Fogs[i], worldPos);
        finalColor = lerp(finalColor, Fogs[i].ExponentialFogColorParameter.rgb, fogWeight);
    }

    return float4(finalColor, sourceColor.a);
}
