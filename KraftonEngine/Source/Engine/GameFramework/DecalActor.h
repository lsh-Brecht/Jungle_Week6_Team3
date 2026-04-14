#pragma once

#include "GameFramework/AActor.h"

class UDecalComponent;
class UMaterialInterface;

class ADecalActor : public AActor
{
public:
	DECLARE_CLASS(ADecalActor, AActor)

	ADecalActor() = default;

	void InitDefaultComponents();

	UDecalComponent* GetDecal() const;

	// Component의 함수를 래핑하는 편의성 API
	void SetDecalMaterial(UMaterialInterface* NewDecalMaterial);
	UMaterialInterface* GetDecalMaterial() const;

	virtual void Serialize(FArchive& Ar) override;
	
private:
	
	mutable UDecalComponent* Decal = nullptr;
};
