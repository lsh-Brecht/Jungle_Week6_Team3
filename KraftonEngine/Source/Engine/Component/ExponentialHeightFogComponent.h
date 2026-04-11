#pragma once

#include "PrimitiveComponent.h"

// UE5에서는 두 번째 안개 레이어를 위한 구조체를 별도로 사용합니다.
struct FExponentialHeightFogData
{
	float FogDensity = 0.0f;
	float FogHeightFalloff = 0.2f;
	float FogHeightOffset = 0.0f;
};

// Primitive가 아닌 SceneComponent 상속
class UExponentialHeightFogComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UExponentialHeightFogComponent, UPrimitiveComponent);

	UExponentialHeightFogComponent();
	~UExponentialHeightFogComponent() override = default;

	void SetFogDensity(float InValue) { FogDensity = InValue; }
	void SetFogHeightFalloff(float InValue) { FogHeightFalloff = InValue; }
	void SetStartDistance(float InValue) { StartDistance = InValue; }
	void SetFogCutoffDistance(float InValue) { FogCutoffDistance = InValue; }
	void SetFogMaxOpacity(float InValue) { FogMaxOpacity = InValue; }
	void SetEndDistance(float InValue) { EndDistance = InValue; }
	void SetDirectionalInscatteringExponent(float InValue) { DirectionalInscatteringExponent = InValue; }
	void SetDirectionalInscatteringStartDistance(float InValue) { DirectionalInscatteringStartDistance = InValue; }

	float GetFogDensity() const { return FogDensity; }
	float GetFogHeightFalloff() const { return FogHeightFalloff; }
	float GetStartDistance() const { return StartDistance; }
	float GetFogCutoffDistance() const { return FogCutoffDistance; }
	float GetFogMaxOpacity() const { return FogMaxOpacity; }
	float GetEndDistance() const { return EndDistance; }
	float GetDirectionalInscatteringExponent() const { return DirectionalInscatteringExponent; }
	float GetDirectionalInscatteringStartDistance() const { return DirectionalInscatteringStartDistance; }

	void Serialize(FArchive& Ar) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

private:
	void SanitizeProperties();

	float FogDensity = 0.02f;		 // 기본 밀도
	float FogHeightFalloff = 0.2f;   // 고도에 따른 감소율
	float StartDistance = 0.0f;	     // 안개 시작 거리
	float FogCutoffDistance = 0.0f;  // 이 거리 너머로는 안개 미적용
	float FogMaxOpacity = 1.0f;	     // 최대 불투명도

	FLinearColor FogInscatteringColor = FLinearColor::White();

	// (TODO: 필수 구현 사항인가 확인)
	// --- 컬러 및 거리 파라미터, 방향성 산란 ---
	FLinearColor FogInscatteringLuminance = FLinearColor::White(); // (UE5 방식) Inscattering Color 대신 Luminance 사용
	float EndDistance = 0.0f;                                     // 안개 종료 거리
	float DirectionalInscatteringExponent = 4.0f;
	float DirectionalInscatteringStartDistance = 0.0f;
	FLinearColor DirectionalInscatteringLuminance = FLinearColor::White();
};
