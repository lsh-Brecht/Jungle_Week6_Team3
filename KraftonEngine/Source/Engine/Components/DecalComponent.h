#pragma once

#include "Components/PrimitiveComponent.h"
#include "Core/DecalTypes.h"

class UMaterialInterface;
class FArchive;
class FRenderBus;

enum EDecalTargetFilterBits : int32
{
	DecalTarget_None = 0,
	DecalTarget_StaticMeshComponent = 1 << 0,
	DecalTarget_ReceivesDecalOnly = 1 << 1,
	DecalTarget_ExcludeSameOwner = 1 << 2,
	DecalTarget_AllPrimitive = 1 << 3,
};

class UDecalComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UDecalComponent, UPrimitiveComponent)

	UDecalComponent() = default;
	~UDecalComponent() override = default;

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;
	void CollectEditorVisualizations(FRenderBus& RenderBus) const override;

	void Serialize(FArchive& AR) override;
	void PostDuplicate() override;

public:
	void MarkDecalDirty();
	void ClearDecalDirty() { bDecalDirty = false; }
	bool IsDecalDirty() const { return bDecalDirty; }
	void EnsureDecalMeshBuilt();

	FPrimitiveSceneProxy* CreateSceneProxy() override;

	void RebuildDecalMeshNow();
	void OnTransformDirty() override;

	void UpdateWorldAABB() const override;
	bool SupportsOutline() const override { return false; }

public:
	void SetDecalSize(const FVector& InSize);
	const FVector& GetDecalSize() const { return DecalSize; }

	void SetDecalMaterial(UMaterialInterface* NewDecalMaterial);
	UMaterialInterface* GetDecalMaterial() const { return DecalMaterial; }
	const FString& GetDecalMaterialPath() const { return DecalMaterialPath; }
	void SetDecalTexture(const FName& TextureName);
	const FName& GetDecalTextureName() const { return DecalTextureName; }
	bool FitSizeToTextureAspect();

	void SetSortOrder(int32 Value);
	int32 GetSortOrder() const { return SortOrder; }
	int32 GetSortPriority() const override { return SortOrder; }

	void SetTargetFilter(int32 InFilter);
	int32 GetTargetFilter() const { return TargetFilter; }

	void SetDrawDebugOBB(bool bEnable) { bDrawDebugOBB = bEnable; }
	bool IsDrawDebugOBBEnabled() const { return bDrawDebugOBB; }
	void SetDrawDebugReceiverTriangles(bool bEnable) { bDrawDebugReceiverTriangles = bEnable; }
	bool IsDrawDebugReceiverTrianglesEnabled() const { return bDrawDebugReceiverTriangles; }

	FTransform GetTransformIncludingDecalSize() const;
	FMatrix GetDecalLocalToWorldMatrix() const;
	FMatrix GetWorldToDecalMatrix() const;

	void GetDecalBoxCorners(FVector (&OutCorners)[8]) const;
	FBoundingBox GetDecalWorldAABB() const;

	void SetFadeOut(float StartDelay, float Duration, bool DestroyOwnerAfterFade = true);

	void DebugRunBroadPhase() const;

	const FDecalRenderableMesh& GetRenderableMesh() const { return RenderableMesh; }

private:
	void BuildDecalMesh();
	void SyncTargetFilterMaskFromOptions();
	void SyncTargetFilterOptionsFromMask();
	void ReloadMaterialFromPath();
	void AddDebugOBBLines(FRenderBus& RenderBus, const FColor& BoxColor) const;
	void AddDebugReceiverTriangleLines(FRenderBus& RenderBus, const FColor& TriangleColor) const;
	void AddDebugClippedTriangleLines(FRenderBus& RenderBus, const FColor& TriangleColor) const;

private:
	FVector DecalSize = FVector(1.0f, 1.0f, 1.0f);
	UMaterialInterface* DecalMaterial = nullptr;
	FString DecalMaterialPath = "None";
	FName DecalTextureName;

	FDecalRenderableMesh RenderableMesh;
	TArray<FDecalSATTriangle> DebugReceiverTriangles;
	TArray<FDecalTriangulatedTriangle> DebugClippedTriangles;

	int32 SortOrder = 0;
	int32 DebugTriangleDrawLimit = 256;

	int32 TargetFilter = DecalTarget_StaticMeshComponent | DecalTarget_ReceivesDecalOnly;

	bool bDecalDirty = true;
	bool bDrawDebugOBB = true;
	bool bDrawDebugReceiverTriangles = false;
	bool bTargetStaticMeshComponent = true;
	bool bTargetReceivesDecalOnly = true;
	bool bExcludeSameOwner = false;
};
