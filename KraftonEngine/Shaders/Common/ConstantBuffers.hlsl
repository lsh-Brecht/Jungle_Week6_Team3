#ifndef CONSTANT_BUFFERS_HLSL
#define CONSTANT_BUFFERS_HLSL

#pragma pack_matrix(row_major)

// b0: 프레임 공통 — ViewProj, 와이어프레임 설정
cbuffer FrameBuffer : register(b0)
{
    float4x4 View;
    float4x4 Projection;
    float bIsWireframe;
    float3 WireframeRGB;
    float Time;
    float3 _framePad;
    float3 CameraPosition;
    float _framePad2;
    float4x4 InverseView;
    float4x4 InverseProjection;
    float4x4 InverseViewProjection;
}

struct LocalTintEffectData
{
    float4 PositionRadius;
    float4 Color;
    float4 Params;
};

cbuffer SceneEffectBuffer : register(b5)
{
    LocalTintEffectData LocalTints[8];
    uint LocalTintCount;
    float3 _sceneEffectPad;
}

// b1: 오브젝트별 — 월드 변환, 색상
cbuffer PerObjectBuffer : register(b1)
{
    float4x4 Model;
    float4 PrimitiveColor;
};

// b2: 기즈모 전용
cbuffer GizmoBuffer : register(b2)
{
    float4 GizmoColorTint;
    uint bIsInnerGizmo;
    uint bClicking;
    uint SelectedAxis;
    float HoveredAxisOpacity;
    uint AxisMask; // 비트 0=X, 1=Y, 2=Z
    uint3 _gizmoPad;
};

// ── Outline 설정 (b3) ──
cbuffer OutlinePostProcessCB : register(b3)
{
    float4 OutlineColor; // 아웃라인 색상 + 알파
    float OutlineThickness; // 샘플링 오프셋 (픽셀 단위, 보통 1.0)
    float OutlineFalloff;  // Radius 감쇠 지수
    float2 _Pad;
};

struct FogUniformParameters
{
    float4 ExponentialFogParameters;
    float4 ExponentialFogColorParameter;
    float4 ExponentialFogParameters3;
};

cbuffer FogPostProcessCB : register(b6)
{
    FogUniformParameters Fogs[8];
    uint FogCount;
    float3 _FogPad;
}

// b4: Material properties
cbuffer MaterialBuffer : register(b4)
{
    uint bIsUVScroll;
    float3 _matPad;
    float4 SectionColor;
}

#endif // CONSTANT_BUFFERS_HLSL
