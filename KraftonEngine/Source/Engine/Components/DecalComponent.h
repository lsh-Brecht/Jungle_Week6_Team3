#pragma once
#pragma once

#include "Components/PrimitiveComponent.h"
#include "Core/EngineTypes.h"
#include "Core/PropertyTypes.h"
#include "Core/ResourceTypes.h"
#include "Render/Resource/MeshBufferManager.h"

class UMaterialInterface;
class FPrimitiveSceneProxy;

class UDecalComponent : public UPrimitiveComponent
{
public:
 DECLARE_CLASS(UDecalComponent, UPrimitiveComponent)

	UDecalComponent();
	~UDecalComponent() override = default;

	// 페이드 아웃 관련 데이터
   float FadeStartDelay = 0.0f;
	float FadeDuration = 0.0f;
	float FadeInDuration = 0.0f;
	float FadeInStartDelay = 0.0f;
	bool bDestroyOwnerAfterFade = true;

	float GetFadeStartDelay() const;
	float GetFadeDuration() const;
	float GetFadeInStartDelay() const;
	float GetFadeInDuration() const;

	FVector DecalSize = FVector(1.0f, 1.0f, 1.0f);
	FLinearColor DecalColor = FLinearColor::White();
	
	void SetFadeOut(float StartDelay, float Duration, bool DestroyOwnerAfterFade = true);
	void SetFadeIn(float StartDelay, float Duration);
	void SetDecalColor(const FLinearColor& Color);
	void SetDecalMaterial(UMaterialInterface* NewDecalMaterial);
	UMaterialInterface* GetDecalMaterial() const;
	void SetDecalTexture(const FName& TextureName);
	const FTextureResource* GetDecalTexture() const;

	void SetDecalSize(const FVector& InSize);
	FMatrix GetTransformIncludingDecalSize() const;

	void UpdateWorldAABB() const override;
	FMeshBuffer* GetMeshBuffer() const override;
	const FMeshData* GetMeshData() const override;
	FPrimitiveSceneProxy* CreateSceneProxy() override;

	void Serialize(FArchive& Ar) override;
	void PostDuplicate() override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

protected:
	UMaterialInterface* DecalMaterial = nullptr;
	FMaterialSlot DecalMaterialSlot;
	FName DecalTextureName;
	FTextureResource* DecalTexture = nullptr;
};
