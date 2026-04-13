#pragma once
#include "Components/SceneComponent.h"
#include "Core/EngineTypes.h"

class UMaterialInterface;
class FDeferredDecalProxy;

class UDecalComponent : public USceneComponent
{
public:
	DECLARE_CLASS(UDecalComponent, USceneComponent)

	// 렌더링 순서 제어 (겹친 데칼 처리용)
	//UPROPERTY() int32 SortOrder;

	// 데칼의 크기 (X: 투영 깊이/Forward, Y: 너비, Z: 높이)
	//UPROPERTY() FVector DecalSize;

	// 머티리얼에 넘겨줄 데칼 컬러
	//UPROPERTY() FLinearColor DecalColor;

	// 페이드 아웃 관련 데이터
	float FadeStartDelay;
	float FadeDuration;
	float FadeInDuration;
	float FadeInStartDelay;
	//UPROPERTY() uint8 bDestroyOwnerAfterFade : 1;

	float GetFadeStartDelay() const;
	float GetFadeDuration() const;
	float GetFadeInStartDelay() const;
	float GetFadeInDuration() const;

	FVector DecalSize;
	FLinearColor DecalColor = FLinearColor::White();
	
	void SetFadeOut(float StartDelay, float Duration, bool DestroyOwnerAfterFade = true);
	void SetFadeIn(float StartDelay, float Duration);
	void SetDecalColor(const FLinearColor& Color);
	void SetDecalMaterial(UMaterialInterface* NewDecalMaterial);
	UMaterialInterface* GetDecalMaterial() const;

	// 자체 엔진의 OBB 혹은 AABB 기반 Culling을 위한 Bounds 계산
	//virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

	// 데칼 Size가 적용된 최종 Transform 반환 (렌더러/셰이더에서 사용)
	FTransform GetTransformIncludingDecalSize() const;

	// 렌더 스레드에 보낼 프록시 생성
	virtual FDeferredDecalProxy* CreateSceneProxy();

	// 직렬화 (FArchive를 통한 저장/로드)
	virtual void Serialize(FArchive& Ar) override;

protected:

	UMaterialInterface* DecalMaterial;
};