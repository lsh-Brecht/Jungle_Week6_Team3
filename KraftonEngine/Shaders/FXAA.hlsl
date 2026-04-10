// FXAA.hlsl
#include "Common/Functions.hlsl"
#include "Common/VertexLayouts.hlsl"

// ── Resources ──
Texture2D SceneColor : register(t0);
SamplerState Sampler : register(s0);

// ── FXAA 전용 Constant Buffer (기존 b0~b4 사용 중이므로 b5 할당) ──
cbuffer FXAAParams : register(b5)
{
    float2 TexelSize; // 1.0f / ViewportWidth, 1.0f / ViewportHeight
    float2 _fxaaPad;
};

// 밝기(Luma)를 구하는 함수
float RgbToLuma(float3 rgb)
{
    return dot(rgb, float3(0.299f, 0.587f, 0.114f));
}

// ── VS: Fullscreen Triangle (OutlinePostProcess.hlsl 구조 활용) ──
// Vertex Buffer를 바인딩할 필요 없이 Draw(3, 0)으로 호출하면 화면 전체를 덮습니다.
PS_Input_Tex VS(uint vertexID : SV_VertexID)
{
    PS_Input_Tex output;
    output.texcoord = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return output;
}

// ── PS: FXAA 메인 로직 ──
float4 PS(PS_Input_Tex input) : SV_TARGET
{
    float2 uv = input.texcoord;
    float3 colorM = SceneColor.Sample(Sampler, uv).rgb;
    
    // 1. 현재 픽셀과 상하좌우 십자 샘플링
    float lumaM = RgbToLuma(colorM);
    float lumaS = RgbToLuma(SceneColor.Sample(Sampler, uv + float2(0, TexelSize.y)).rgb);
    float lumaN = RgbToLuma(SceneColor.Sample(Sampler, uv + float2(0, -TexelSize.y)).rgb);
    float lumaW = RgbToLuma(SceneColor.Sample(Sampler, uv + float2(-TexelSize.x, 0)).rgb);
    float lumaE = RgbToLuma(SceneColor.Sample(Sampler, uv + float2(TexelSize.x, 0)).rgb);

    // 2. 엣지 검출 (대비 계산)
    float lumaMax = max(lumaM, max(max(lumaN, lumaS), max(lumaW, lumaE)));
    float lumaMin = min(lumaM, min(min(lumaN, lumaS), min(lumaW, lumaE)));
    float lumaContrast = lumaMax - lumaMin;

    // 조기 종료 (Early Exit): 대비가 너무 낮으면 즉시 원본 반환
    float threshold = max(0.0312f, lumaMax * 0.125f);
    if (lumaContrast < threshold)
    {
        return float4(colorM, 1.0f);
    }

    // 3-1. 대각선 픽셀 추가 샘플링
    float lumaNW = RgbToLuma(SceneColor.Sample(Sampler, uv + float2(-TexelSize.x, -TexelSize.y)).rgb);
    float lumaNE = RgbToLuma(SceneColor.Sample(Sampler, uv + float2(TexelSize.x, -TexelSize.y)).rgb);
    float lumaSW = RgbToLuma(SceneColor.Sample(Sampler, uv + float2(-TexelSize.x, TexelSize.y)).rgb);
    float lumaSE = RgbToLuma(SceneColor.Sample(Sampler, uv + float2(TexelSize.x, TexelSize.y)).rgb);

    // 3-2. 수평 및 수직 변화량(Edge Gradient) 계산
    float edgeVert =
        abs((lumaNW + lumaSW) - 2.0 * lumaW) +
        abs((lumaN + lumaS) - 2.0 * lumaM) * 2.0 +
        abs((lumaNE + lumaSE) - 2.0 * lumaE);

    float edgeHorz =
        abs((lumaNW + lumaNE) - 2.0 * lumaN) +
        abs((lumaW + lumaE) - 2.0 * lumaM) * 2.0 +
        abs((lumaSW + lumaSE) - 2.0 * lumaS);

    // 3-3. 방향 판별
    bool isHorizontal = (edgeHorz >= edgeVert);

    // 3-4. 방향에 따른 이웃 픽셀 밝기 선택
    float luma1 = isHorizontal ? lumaN : lumaW;
    float luma2 = isHorizontal ? lumaS : lumaE;

    // 3-5. 어느 쪽 방향의 변화율이 더 큰지(가파른지) 계산
    float gradient1 = luma1 - lumaM;
    float gradient2 = luma2 - lumaM;
    
    bool is1Steeper = abs(gradient1) >= abs(gradient2);
    float gradientScaled = 0.5 * max(abs(gradient1), abs(gradient2));
    
    // 4-1. 탐색을 위한 로컬 좌표 및 증분 설정
    float lumaParent = 0.5 * (luma1 + luma2) + lumaM * 0.5;
    float2 posN = uv;
    float2 stepN = isHorizontal ? float2(TexelSize.x, 0) : float2(0, TexelSize.y);
    
    // 수직 방향으로 0.5 픽셀만큼 이동하여 모서리 경계선에 위치시킴
    if (isHorizontal)
        posN.y += (is1Steeper ? -0.5 : 0.5) * TexelSize.y;
    else
        posN.x += (is1Steeper ? -0.5 : 0.5) * TexelSize.x;

    // 4-2. 양방향 탐색 (Iteration)
    float2 posP = posN + stepN;
    float2 posN_end = posN - stepN;
    float lumaEndP = 0.0f;
    float lumaEndN = 0.0f;
    bool doneP = false, doneN = false;

    [unroll(10)]
    for (int i = 0; i < 10; i++)
    {
        if (!doneP)
            lumaEndP = RgbToLuma(SceneColor.SampleLevel(Sampler, posP, 0).rgb) - lumaParent;
        if (!doneN)
            lumaEndN = RgbToLuma(SceneColor.SampleLevel(Sampler, posN_end, 0).rgb) - lumaParent;

        doneP = doneP || (abs(lumaEndP) >= gradientScaled);
        doneN = doneN || (abs(lumaEndN) >= gradientScaled);

        if (doneP && doneN)
            break;

        if (!doneP)
            posP += stepN;
        if (!doneN)
            posN_end -= stepN;
    }

    // 4-3. 거리 계산 및 가중치 도출
    float distP = isHorizontal ? (posP.x - uv.x) : (posP.y - uv.y);
    float distN = isHorizontal ? (uv.x - posN_end.x) : (uv.y - posN_end.y);

    float distMin = min(distP, distN);
    float edgeLen = distP + distN;

    float pixelOffset = -distMin / edgeLen + 0.5;

    // 4-4. 최종 샘플링 및 반환
    float2 finalUV = uv;
    if (isHorizontal)
        finalUV.y += (is1Steeper == (lumaEndN < 0.0)) ? -pixelOffset * TexelSize.y : pixelOffset * TexelSize.y;
    else
        finalUV.x += (is1Steeper == (lumaEndN < 0.0)) ? -pixelOffset * TexelSize.x : pixelOffset * TexelSize.x;

    float3 finalColor = SceneColor.SampleLevel(Sampler, finalUV, 0).rgb;
    return float4(finalColor, 1.0f);
}