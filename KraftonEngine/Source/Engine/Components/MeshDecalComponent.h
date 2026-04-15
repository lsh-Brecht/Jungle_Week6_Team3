#pragma once

#include "Components/PrimitiveComponent.h"
#include "Core/EngineTypes.h"
#include "Core/MeshDecalTypes.h"
#include "Core/PropertyTypes.h"
#include "Core/ResourceTypes.h"

class UMaterialInterface;
class FPrimitiveSceneProxy;
class FArchive;

class UMeshDecalComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UMeshDecalComponent, UPrimitiveComponent)

	UMeshDecalComponent();
	~UMeshDecalComponent() override = default;

	void Serialize(FArchive& Ar) override;
	void PostDuplicate() override;
	void BeginPlay() override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;
	void OnTransformDirty() override;
	void CreateRenderState() override;
	void DestroyRenderState() override;

	void SetMeshDecalSize(const FVector& InSize);
	const FVector& GetMeshDecalSize() const { return MeshDecalSize; }

	void SetMeshDecalMaterial(UMaterialInterface* InMaterial);
	UMaterialInterface* GetMeshDecalMaterial() const { return MeshDecalMaterial; }
	void SetMeshDecalTexture(const FName& TextureName);
	const FTextureResource* GetMeshDecalTexture() const { return MeshDecalTexture; }
	bool IsMeshDecalUVScrollEnabled() const { return MeshDecalMaterialSlot.bUVScroll != 0; }
	bool FitSizeToTextureAspect();
	void SetMeshDecalColor(const FLinearColor& Color);
	const FLinearColor& GetMeshDecalColor() const { return MeshDecalColor; }

	void SetFadeOut(float StartDelay, float Duration, bool DestroyOwnerAfterFade = true);
	void SetFadeIn(float StartDelay, float Duration);

	void SetSortOrder(int32 InSortOrder);
	int32 GetSortOrder() const { return SortOrder; }

	void SetReceivesDecalOnly(bool bEnable) { bReceivesDecalOnly = bEnable; MarkMeshDecalDirty(); }
	bool IsReceivesDecalOnlyEnabled() const { return bReceivesDecalOnly; }
	void SetExcludeSameOwner(bool bEnable) { bExcludeSameOwner = bEnable; MarkMeshDecalDirty(); }
	bool IsExcludeSameOwnerEnabled() const { return bExcludeSameOwner; }
	void SetLooseTriangleAccept(bool bEnable) { bLooseTriangleAccept = bEnable; MarkMeshDecalDirty(); }
	bool IsLooseTriangleAcceptEnabled() const { return bLooseTriangleAccept; }

	void MarkMeshDecalDirty();
	void EnsureMeshDecalMeshBuilt();
	const FMeshDecalRenderableMesh& GetRenderableMesh() const { return RenderableMesh; }

	FMatrix GetMeshDecalLocalToWorldMatrix() const;
	FMatrix GetWorldToMeshDecalMatrix() const;
	void GetMeshDecalBoxCorners(FVector(&OutCorners)[8]) const;
	FBoundingBox GetMeshDecalWorldAABB() const;

	void UpdateWorldAABB() const override;
	bool SupportsOutline() const override { return false; }
	bool SupportsPicking() const override { return false; }

	FPrimitiveSceneProxy* CreateSceneProxy() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
	void BuildMeshDecalMesh();
	void ReloadMaterialFromPath();
	void RestartFadePreviewSequence();

private:
	FVector MeshDecalSize = FVector(1.0f, 1.0f, 1.0f);
	FLinearColor MeshDecalColor = FLinearColor::White();
	float FadeStartDelay = 0.0f;
	float FadeDuration = 0.0f;
	float FadeInDuration = 0.0f;
	float FadeInStartDelay = 0.0f;
	bool bDestroyOwnerAfterFade = true;
	UMaterialInterface* MeshDecalMaterial = nullptr;
	FName MeshDecalTextureName;
	FTextureResource* MeshDecalTexture = nullptr;
	FMaterialSlot MeshDecalMaterialSlot;
	FString MeshDecalMaterialPath = "None";
	FMeshDecalRenderableMesh RenderableMesh;
	FPrimitiveSceneProxy* ArrowOuterProxy = nullptr;
	FPrimitiveSceneProxy* ArrowInnerProxy = nullptr;
	int32 SortOrder = 0;
	bool bMeshDecalDirty = true;
	bool bReceivesDecalOnly = true;
	bool bExcludeSameOwner = false;
	bool bLooseTriangleAccept = true;
	float FadeOutTimeElapsed = 0.0f;
	float FadeInTimeElapsed = 0.0f;
	bool bIsFadeOutActive = false;
	bool bIsFadeInActive = false;
	bool bPendingFadeOutAfterFadeIn = false;
	float OriginalAlpha = 1.0f;
};
