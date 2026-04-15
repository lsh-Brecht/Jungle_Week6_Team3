#pragma once

#include "GameFramework/AActor.h"

class UBillboardComponent;
class UMeshDecalComponent;
class UMaterialInterface;

class AMeshDecalActor : public AActor
{
public:
	DECLARE_CLASS(AMeshDecalActor, AActor)

	AMeshDecalActor();

	UMeshDecalComponent* GetMeshDecal() const { return MeshDecal; }
	UBillboardComponent* GetSpriteComponent() const { return SpriteComponent; }

	void SetMeshDecalMaterial(UMaterialInterface* NewMaterial);
	void SetMeshDecalMaterial(const FString& MaterialPath);
	UMaterialInterface* GetMeshDecalMaterial() const;

	void SetMeshDecalSize(const FVector& InSize);
	FVector GetMeshDecalSize() const;

	void BeginPlay() override;
	void Serialize(FArchive& Ar) override;

protected:
	UMeshDecalComponent* MeshDecal = nullptr;
	UBillboardComponent* SpriteComponent = nullptr;
};

