#pragma once

#include "GameFramework/AActor.h"

//class UArrowComponent;
class UBillboardComponent;
class UBoxComponent;
class UDecalComponent;

class ADecalActor : public AActor
{
public:
	DECLARE_CLASS(ADecalActor, AActor)

	ADecalActor();

	UDecalComponent* GetDecal() const { return Decal; }

	UBillboardComponent* GetSpriteComponent() const { return SpriteComponent; }

	// Component의 함수를 래핑하는 편의성 API
	//void SetDecalMaterial(UMaterialInterface* NewDecalMaterial);
	//UMaterialInterface* GetDecalMaterial() const;

	//virtual void Serialize(FArchive& Ar) override;
	
private:
	
	UDecalComponent* Decal = nullptr;
	UBillboardComponent* SpriteComponent = nullptr; // Editor에서 데칼 위치/회전 편집용 시각적 가이드. 게임에서는 숨김.
};