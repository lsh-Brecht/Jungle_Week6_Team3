#pragma once

#include "Components/PrimitiveComponent.h"
#include "Core/MeshDecalTypes.h"
#include "Core/PropertyTypes.h"

class UMaterialInterface;
class FPrimitiveSceneProxy;
class FArchive;

class UMeshDecalComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UMeshDecalComponent, UPrimitiveComponent)

	UMeshDecalComponent() = default;
	~UMeshDecalComponent() override = default;

	void Serialize(FArchive& Ar) override;
	void PostDuplicate() override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;
	void OnTransformDirty() override;

	void SetMeshDecalSize(const FVector& InSize);
	const FVector& GetMeshDecalSize() const { return MeshDecalSize; }

	void SetMeshDecalMaterial(UMaterialInterface* InMaterial);
	UMaterialInterface* GetMeshDecalMaterial() const { return MeshDecalMaterial; }

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

private:
	void BuildMeshDecalMesh();
	void ReloadMaterialFromPath();

private:
	FVector MeshDecalSize = FVector(1.0f, 1.0f, 1.0f);
	UMaterialInterface* MeshDecalMaterial = nullptr;
	FMaterialSlot MeshDecalMaterialSlot;
	FString MeshDecalMaterialPath = "None";
	FMeshDecalRenderableMesh RenderableMesh;
	int32 SortOrder = 0;
	bool bMeshDecalDirty = true;
	bool bReceivesDecalOnly = true;
	bool bExcludeSameOwner = false;
	bool bLooseTriangleAccept = true;
};
