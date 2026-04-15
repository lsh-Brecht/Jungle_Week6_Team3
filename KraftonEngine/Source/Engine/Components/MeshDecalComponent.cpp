#include "Components/MeshDecalComponent.h"

#include "Materials/MaterialInterface.h"
#include "Mesh/MeshDecalMeshBuilder.h"
#include "Mesh/ObjManager.h"
#include "Object/ObjectFactory.h"
#include "GameFramework/World.h"
#include "Render/Proxy/MeshDecalSceneProxy.h"
#include "Serialization/Archive.h"

#include <algorithm>
#include <cstring>

IMPLEMENT_CLASS(UMeshDecalComponent, UPrimitiveComponent)

namespace
{
	FVector SanitizeSize(const FVector& InSize)
	{
		return FVector(
			std::max(std::abs(InSize.X), 0.001f),
			std::max(std::abs(InSize.Y), 0.001f),
			std::max(std::abs(InSize.Z), 0.001f));
	}
}

void UMeshDecalComponent::Serialize(FArchive& Ar)
{
	UPrimitiveComponent::Serialize(Ar);

	if (Ar.IsSaving())
	{
		MeshDecalMaterialPath = MeshDecalMaterial ? MeshDecalMaterial->GetAssetPathFileName() : "None";
		MeshDecalMaterialSlot.Path = MeshDecalMaterialPath;
	}

	Ar << MeshDecalSize;
	Ar << MeshDecalMaterialPath;
	Ar << SortOrder;
	Ar << bReceivesDecalOnly;
	Ar << bExcludeSameOwner;
	Ar << bLooseTriangleAccept;

	if (Ar.IsLoading())
	{
		MeshDecalSize = SanitizeSize(MeshDecalSize);
		MeshDecalMaterialSlot.Path = MeshDecalMaterialPath;
		ReloadMaterialFromPath();
		MarkMeshDecalDirty();
		MarkWorldBoundsDirty();
	}
}

void UMeshDecalComponent::PostDuplicate()
{
	UPrimitiveComponent::PostDuplicate();
	ReloadMaterialFromPath();
	MarkMeshDecalDirty();
	MarkWorldBoundsDirty();
}

void UMeshDecalComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPrimitiveComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Mesh Decal Size", EPropertyType::Vec3, &MeshDecalSize });
	OutProps.push_back({ "Mesh Decal Material", EPropertyType::MaterialSlot, &MeshDecalMaterialSlot });
	OutProps.push_back({ "Sort Order", EPropertyType::Int, &SortOrder });
	OutProps.push_back({ "Receives Decal Only", EPropertyType::Bool, &bReceivesDecalOnly });
	OutProps.push_back({ "Exclude Same Owner", EPropertyType::Bool, &bExcludeSameOwner });
	OutProps.push_back({ "Loose Triangle Accept", EPropertyType::Bool, &bLooseTriangleAccept });
}

void UMeshDecalComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);

	if (std::strcmp(PropertyName, "Mesh Decal Size") == 0)
	{
		SetMeshDecalSize(MeshDecalSize);
	}
	else if (std::strcmp(PropertyName, "Mesh Decal Material") == 0 || std::strcmp(PropertyName, "Element 0") == 0)
	{
		MeshDecalMaterialPath = MeshDecalMaterialSlot.Path;
		ReloadMaterialFromPath();
		MarkMeshDecalDirty();
	}
	else if (std::strcmp(PropertyName, "Sort Order") == 0)
	{
		SetSortOrder(SortOrder);
	}
	else if (std::strcmp(PropertyName, "Receives Decal Only") == 0
		|| std::strcmp(PropertyName, "Exclude Same Owner") == 0
		|| std::strcmp(PropertyName, "Loose Triangle Accept") == 0)
	{
		MarkMeshDecalDirty();
	}
}

void UMeshDecalComponent::OnTransformDirty()
{
	UPrimitiveComponent::OnTransformDirty();
	MarkMeshDecalDirty();
}

void UMeshDecalComponent::SetMeshDecalSize(const FVector& InSize)
{
	MeshDecalSize = SanitizeSize(InSize);
	MarkWorldBoundsDirty();
	MarkMeshDecalDirty();
}

void UMeshDecalComponent::SetMeshDecalMaterial(UMaterialInterface* InMaterial)
{
	MeshDecalMaterial = InMaterial;
	MeshDecalMaterialPath = MeshDecalMaterial ? MeshDecalMaterial->GetAssetPathFileName() : "None";
	MeshDecalMaterialSlot.Path = MeshDecalMaterialPath;
	MarkMeshDecalDirty();
	MarkProxyDirty(EDirtyFlag::Material);
}

void UMeshDecalComponent::SetSortOrder(int32 InSortOrder)
{
	SortOrder = InSortOrder;
	MarkProxyDirty(EDirtyFlag::Material);
}

void UMeshDecalComponent::MarkMeshDecalDirty()
{
	bMeshDecalDirty = true;
	MarkProxyDirty(EDirtyFlag::Mesh);
}

void UMeshDecalComponent::EnsureMeshDecalMeshBuilt()
{
	if (!bMeshDecalDirty)
	{
		return;
	}

	BuildMeshDecalMesh();
	bMeshDecalDirty = false;
}

FMatrix UMeshDecalComponent::GetMeshDecalLocalToWorldMatrix() const
{
	return FMatrix::MakeScaleMatrix(MeshDecalSize) * GetWorldMatrix();
}

FMatrix UMeshDecalComponent::GetWorldToMeshDecalMatrix() const
{
	return GetMeshDecalLocalToWorldMatrix().GetInverse();
}

void UMeshDecalComponent::GetMeshDecalBoxCorners(FVector(&OutCorners)[8]) const
{
	static const FVector UnitCorners[8] =
	{
		FVector(-0.5f, -0.5f, -0.5f), FVector(0.5f, -0.5f, -0.5f),
		FVector(0.5f, 0.5f, -0.5f), FVector(-0.5f, 0.5f, -0.5f),
		FVector(-0.5f, -0.5f, 0.5f), FVector(0.5f, -0.5f, 0.5f),
		FVector(0.5f, 0.5f, 0.5f), FVector(-0.5f, 0.5f, 0.5f)
	};

	const FMatrix LocalToWorld = GetMeshDecalLocalToWorldMatrix();
	for (int32 i = 0; i < 8; ++i)
	{
		OutCorners[i] = LocalToWorld.TransformPositionWithW(UnitCorners[i]);
	}
}

FBoundingBox UMeshDecalComponent::GetMeshDecalWorldAABB() const
{
	FVector Corners[8];
	GetMeshDecalBoxCorners(Corners);

	FBoundingBox Bounds;
	for (const FVector& Corner : Corners)
	{
		Bounds.Expand(Corner);
	}
	return Bounds;
}

void UMeshDecalComponent::UpdateWorldAABB() const
{
	const FBoundingBox Bounds = GetMeshDecalWorldAABB();
	WorldAABBMinLocation = Bounds.Min;
	WorldAABBMaxLocation = Bounds.Max;
	bWorldAABBDirty = false;
	bHasValidWorldAABB = Bounds.IsValid();
}

FPrimitiveSceneProxy* UMeshDecalComponent::CreateSceneProxy()
{
	EnsureMeshDecalMeshBuilt();
	return new FMeshDecalSceneProxy(this);
}

void UMeshDecalComponent::BuildMeshDecalMesh()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		RenderableMesh.Clear();
		return;
	}

	FMeshDecalMeshBuilder::BuildRenderableMesh(*this, *World, RenderableMesh);
}

void UMeshDecalComponent::ReloadMaterialFromPath()
{
	if (MeshDecalMaterialPath.empty() || MeshDecalMaterialPath == "None")
	{
		MeshDecalMaterial = nullptr;
		return;
	}
	MeshDecalMaterial = FObjManager::GetOrLoadMaterial(MeshDecalMaterialPath);
}

