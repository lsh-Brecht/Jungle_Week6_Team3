#include "Components/ProjectionDecalComponent.h"

#include "Materials/MaterialInterface.h"
#include "Mesh/ProjectionDecalMeshBuilder.h"
#include "Mesh/ObjManager.h"
#include "Object/ObjectFactory.h"
#include "GameFramework/World.h"
#include "Render/Proxy/FScene.h"
#include "Render/Proxy/ProjectionDecalSceneProxy.h"
#include "Resource/ResourceManager.h"
#include "Serialization/Archive.h"
#include "Texture/Texture2D.h"

#include <algorithm>
#include <cstring>

IMPLEMENT_CLASS(UProjectionDecalComponent, UPrimitiveComponent)

namespace
{
	constexpr const char* DecalTextureMaterialPrefix = "Texture:";

	FVector SanitizeSize(const FVector& InSize)
	{
		return FVector(
			std::max(std::abs(InSize.X), 0.001f),
			std::max(std::abs(InSize.Y), 0.001f),
			std::max(std::abs(InSize.Z), 0.001f));
	}

	bool IsDecalTextureMaterialPath(const FString& Path)
	{
		return Path.rfind(DecalTextureMaterialPrefix, 0) == 0;
	}

	FString GetDecalTextureNameFromMaterialPath(const FString& Path)
	{
		return IsDecalTextureMaterialPath(Path)
			? Path.substr(std::strlen(DecalTextureMaterialPrefix))
			: FString();
	}

	FString MakeDecalTextureMaterialPath(const FName& TextureName)
	{
		return FString(DecalTextureMaterialPrefix) + TextureName.ToString();
	}
}

void UProjectionDecalComponent::Serialize(FArchive& Ar)
{
	UPrimitiveComponent::Serialize(Ar);

	if (Ar.IsSaving())
	{
		if (ProjectionDecalTextureName.IsValid())
		{
			ProjectionDecalMaterialPath = MakeDecalTextureMaterialPath(ProjectionDecalTextureName);
		}
		else
		{
			ProjectionDecalMaterialPath = ProjectionDecalMaterial ? ProjectionDecalMaterial->GetAssetPathFileName() : "None";
		}
		ProjectionDecalMaterialSlot.Path = ProjectionDecalMaterialPath;
	}

	Ar << ProjectionDecalSize;
	Ar << ProjectionDecalMaterialPath;
	Ar << SortOrder;
	Ar << bReceivesDecalOnly;
	Ar << bExcludeSameOwner;
	Ar << bLooseTriangleAccept;

	if (Ar.IsLoading())
	{
		ProjectionDecalSize = SanitizeSize(ProjectionDecalSize);
		ProjectionDecalMaterialSlot.Path = ProjectionDecalMaterialPath;
		ReloadMaterialFromPath();
		MarkProjectionDecalDirty();
		MarkWorldBoundsDirty();
	}
}

void UProjectionDecalComponent::PostDuplicate()
{
	UPrimitiveComponent::PostDuplicate();
	ReloadMaterialFromPath();
	MarkProjectionDecalDirty();
	MarkWorldBoundsDirty();
}

void UProjectionDecalComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPrimitiveComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Projection Decal Size", EPropertyType::Vec3, &ProjectionDecalSize });
	OutProps.push_back({ "Projection Decal Material", EPropertyType::MaterialSlot, &ProjectionDecalMaterialSlot });
	OutProps.push_back({ "Sort Order", EPropertyType::Int, &SortOrder });
	OutProps.push_back({ "Receives Decal Only", EPropertyType::Bool, &bReceivesDecalOnly });
	OutProps.push_back({ "Exclude Same Owner", EPropertyType::Bool, &bExcludeSameOwner });
	OutProps.push_back({ "Loose Triangle Accept", EPropertyType::Bool, &bLooseTriangleAccept });
}

void UProjectionDecalComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);

	if (std::strcmp(PropertyName, "Projection Decal Size") == 0)
	{
		SetProjectionDecalSize(ProjectionDecalSize);
	}
	else if (std::strcmp(PropertyName, "Projection Decal Material") == 0 || std::strcmp(PropertyName, "Element 0") == 0)
	{
		ProjectionDecalMaterialPath = ProjectionDecalMaterialSlot.Path;
		ReloadMaterialFromPath();
		MarkProjectionDecalDirty();
	}
	else if (std::strcmp(PropertyName, "Sort Order") == 0)
	{
		SetSortOrder(SortOrder);
	}
	else if (std::strcmp(PropertyName, "Receives Decal Only") == 0
		|| std::strcmp(PropertyName, "Exclude Same Owner") == 0
		|| std::strcmp(PropertyName, "Loose Triangle Accept") == 0)
	{
		MarkProjectionDecalDirty();
	}
}

void UProjectionDecalComponent::OnTransformDirty()
{
	UPrimitiveComponent::OnTransformDirty();
	MarkProjectionDecalDirty();
}

void UProjectionDecalComponent::SetProjectionDecalSize(const FVector& InSize)
{
	ProjectionDecalSize = SanitizeSize(InSize);
	MarkWorldBoundsDirty();
	MarkProjectionDecalDirty();
}

void UProjectionDecalComponent::SetProjectionDecalMaterial(UMaterialInterface* InMaterial)
{
	ProjectionDecalMaterial = InMaterial;
	ProjectionDecalTextureName = FName();
	ProjectionDecalTexture = nullptr;
	ProjectionDecalMaterialPath = ProjectionDecalMaterial ? ProjectionDecalMaterial->GetAssetPathFileName() : "None";
	ProjectionDecalMaterialSlot.Path = ProjectionDecalMaterialPath;
	MarkProjectionDecalDirty();
	MarkProxyDirty(EDirtyFlag::Material);
}

void UProjectionDecalComponent::SetProjectionDecalTexture(const FName& TextureName)
{
	ProjectionDecalMaterial = nullptr;
	ProjectionDecalTextureName = TextureName;
	ProjectionDecalTexture = FResourceManager::Get().FindTexture(TextureName);
	ProjectionDecalMaterialPath = ProjectionDecalTexture ? MakeDecalTextureMaterialPath(TextureName) : "None";
	ProjectionDecalMaterialSlot.Path = ProjectionDecalMaterialPath;
	MarkProjectionDecalDirty();
	MarkProxyDirty(EDirtyFlag::Material);
}

bool UProjectionDecalComponent::FitSizeToTextureAspect()
{
	uint32 TextureWidth = 0;
	uint32 TextureHeight = 0;

	if (ProjectionDecalTexture)
	{
		TextureWidth = ProjectionDecalTexture->Width;
		TextureHeight = ProjectionDecalTexture->Height;
	}
	else if (ProjectionDecalMaterial)
	{
		if (UTexture2D* DiffuseTexture = ProjectionDecalMaterial->GetDiffuseTexture())
		{
			TextureWidth = DiffuseTexture->GetWidth();
			TextureHeight = DiffuseTexture->GetHeight();
		}
	}

	if (TextureWidth == 0 || TextureHeight == 0)
	{
		return false;
	}

	if (ProjectionDecalSize.Y <= 0.0f)
	{
		ProjectionDecalSize.Y = 1.0f;
	}

	const FVector WorldScale = GetWorldScale();
	const float SafeWorldScaleY = (WorldScale.Y > 0.001f) ? WorldScale.Y : 1.0f;
	const float SafeWorldScaleZ = (WorldScale.Z > 0.001f) ? WorldScale.Z : 1.0f;
	const float TextureAspectYZ = static_cast<float>(TextureHeight) / static_cast<float>(TextureWidth);
	ProjectionDecalSize.Z = (ProjectionDecalSize.Y * SafeWorldScaleY * TextureAspectYZ) / SafeWorldScaleZ;
	MarkWorldBoundsDirty();
	MarkProjectionDecalDirty();
	return true;
}

void UProjectionDecalComponent::SetSortOrder(int32 InSortOrder)
{
	SortOrder = InSortOrder;
	MarkProxyDirty(EDirtyFlag::Material);
}

void UProjectionDecalComponent::MarkProjectionDecalDirty()
{
	bProjectionDecalDirty = true;
	MarkProxyDirty(EDirtyFlag::Mesh);
}

void UProjectionDecalComponent::EnsureProjectionDecalMeshBuilt()
{
	if (!bProjectionDecalDirty)
	{
		return;
	}

	BuildProjectionDecalMesh();
	bProjectionDecalDirty = false;
}

FMatrix UProjectionDecalComponent::GetProjectionDecalLocalToWorldMatrix() const
{
	return FMatrix::MakeScaleMatrix(ProjectionDecalSize) * GetWorldMatrix();
}

FMatrix UProjectionDecalComponent::GetWorldToProjectionDecalMatrix() const
{
	return GetProjectionDecalLocalToWorldMatrix().GetInverse();
}

void UProjectionDecalComponent::GetProjectionDecalBoxCorners(FVector(&OutCorners)[8]) const
{
	static const FVector UnitCorners[8] =
	{
		FVector(-0.5f, -0.5f, -0.5f), FVector(0.5f, -0.5f, -0.5f),
		FVector(0.5f, 0.5f, -0.5f), FVector(-0.5f, 0.5f, -0.5f),
		FVector(-0.5f, -0.5f, 0.5f), FVector(0.5f, -0.5f, 0.5f),
		FVector(0.5f, 0.5f, 0.5f), FVector(-0.5f, 0.5f, 0.5f)
	};

	const FMatrix LocalToWorld = GetProjectionDecalLocalToWorldMatrix();
	for (int32 i = 0; i < 8; ++i)
	{
		OutCorners[i] = LocalToWorld.TransformPositionWithW(UnitCorners[i]);
	}
}

FBoundingBox UProjectionDecalComponent::GetProjectionDecalWorldAABB() const
{
	FVector Corners[8];
	GetProjectionDecalBoxCorners(Corners);

	FBoundingBox Bounds;
	for (const FVector& Corner : Corners)
	{
		Bounds.Expand(Corner);
	}
	return Bounds;
}

void UProjectionDecalComponent::UpdateWorldAABB() const
{
	const FBoundingBox Bounds = GetProjectionDecalWorldAABB();
	WorldAABBMinLocation = Bounds.Min;
	WorldAABBMaxLocation = Bounds.Max;
	bWorldAABBDirty = false;
	bHasValidWorldAABB = Bounds.IsValid();
}

FPrimitiveSceneProxy* UProjectionDecalComponent::CreateSceneProxy()
{
	EnsureProjectionDecalMeshBuilt();
	return new FProjectionDecalSceneProxy(this);
}

void UProjectionDecalComponent::CreateRenderState()
{
	if (SceneProxy)
	{
		return;
	}

	UPrimitiveComponent::CreateRenderState();
	if (!SceneProxy)
	{
		return;
	}

	FScene* Scene = nullptr;
	if (Owner && Owner->GetWorld())
	{
		Scene = &Owner->GetWorld()->GetScene();
	}

	if (!Scene)
	{
		return;
	}

	ArrowOuterProxy = new FProjectionDecalArrowSceneProxy(this, false);
	Scene->RegisterProxy(ArrowOuterProxy);

	ArrowInnerProxy = new FProjectionDecalArrowSceneProxy(this, true);
	Scene->RegisterProxy(ArrowInnerProxy);
}

void UProjectionDecalComponent::DestroyRenderState()
{
	if (Owner && Owner->GetWorld())
	{
		FScene& Scene = Owner->GetWorld()->GetScene();
		if (ArrowInnerProxy)
		{
			Scene.RemovePrimitive(ArrowInnerProxy);
			ArrowInnerProxy = nullptr;
		}
		if (ArrowOuterProxy)
		{
			Scene.RemovePrimitive(ArrowOuterProxy);
			ArrowOuterProxy = nullptr;
		}
	}

	UPrimitiveComponent::DestroyRenderState();
}

void UProjectionDecalComponent::BuildProjectionDecalMesh()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		RenderableMesh.Clear();
		return;
	}

	FProjectionDecalMeshBuilder::BuildRenderableMesh(*this, *World, RenderableMesh);
}

void UProjectionDecalComponent::ReloadMaterialFromPath()
{
	if (ProjectionDecalMaterialPath.empty() || ProjectionDecalMaterialPath == "None")
	{
		ProjectionDecalMaterial = nullptr;
		ProjectionDecalTextureName = FName();
		ProjectionDecalTexture = nullptr;
		return;
	}

	if (IsDecalTextureMaterialPath(ProjectionDecalMaterialPath))
	{
		ProjectionDecalMaterial = nullptr;
		ProjectionDecalTextureName = FName(GetDecalTextureNameFromMaterialPath(ProjectionDecalMaterialPath));
		ProjectionDecalTexture = FResourceManager::Get().FindTexture(ProjectionDecalTextureName);
		return;
	}

	ProjectionDecalMaterial = FObjManager::GetOrLoadMaterial(ProjectionDecalMaterialPath);
	ProjectionDecalTextureName = FName();
	ProjectionDecalTexture = nullptr;
}


