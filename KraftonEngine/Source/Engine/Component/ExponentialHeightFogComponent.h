#pragma once

#include "Core/EngineTypes.h"
#include "Component/SceneComponent.h"

// UE5에서는 두 번째 안개 레이어를 위한 구조체를 별도로 사용합니다.
struct FExponentialHeightFogData
{
	float FogDensity = 0.0f;
	float FogHeightFalloff = 0.2f;
	float FogHeightOffset = 0.0f;
};

// Primitive가 아닌 SceneComponent 상속
class UExponentialHeightFogComponent : public USceneComponent
{
public:
	UExponentialHeightFogComponent();

	// --- 기본 안개 파라미터 ---
	float FogDensity;             // 기본 밀도
	float FogHeightFalloff;       // 고도에 따른 감소율

	// --- 컬러 및 거리 파라미터 ---
	FLinearColor FogInscatteringLuminance; // (UE5 방식) Inscattering Color 대신 Luminance 사용
	float FogMaxOpacity;                   // 최대 불투명도
	float StartDistance;                   // 안개 시작 거리
	float EndDistance;                     // 안개 종료 거리
	float FogCutoffDistance;               // 이 거리 너머로는 안개 미적용

	// --- 방향성 산란 (Directional Inscattering) ---
	float DirectionalInscatteringExponent;
	float DirectionalInscatteringStartDistance;
	FLinearColor DirectionalInscatteringLuminance;
};