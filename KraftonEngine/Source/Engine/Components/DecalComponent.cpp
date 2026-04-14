#include "Components/DecalComponent.h"
#include "Components/DecalComponent.h"

#include <cstring>

#include "Collision/RayUtils.h"
#include "Mesh/ObjManager.h"
#include "Materials/MaterialInterface.h"
#include "Object/ObjectFactory.h"
#include "Resource/ResourceManager.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Render/Proxy/DecalSceneProxy.h"
#include "Render/Proxy/FScene.h"
#include "Serialization/Archive.h"
#include "Texture/Texture2D.h"

IMPLEMENT_CLASS(UDecalComponent, UPrimitiveComponent)

namespace
{
	constexpr const char* DecalTextureMaterialPrefix = "Texture:";

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

UDecalComponent::UDecalComponent()
{
}

float UDecalComponent::GetFadeStartDelay() const { return FadeStartDelay; }
float UDecalComponent::GetFadeDuration() const { return FadeDuration; }
float UDecalComponent::GetFadeInStartDelay() const { return FadeInStartDelay; }
float UDecalComponent::GetFadeInDuration() const { return FadeInDuration; }

void UDecalComponent::SetFadeOut(float StartDelay, float Duration, bool DestroyOwnerAfterFade)
{
    FadeStartDelay = StartDelay;
    FadeDuration = Duration;
    bDestroyOwnerAfterFade = DestroyOwnerAfterFade;
}

void UDecalComponent::SetFadeIn(float StartDelay, float Duration)
{
    FadeInStartDelay = StartDelay;
    FadeInDuration = Duration;
}

void UDecalComponent::SetDecalColor(const FLinearColor& Color)
{
    DecalColor = Color;
    MarkProxyDirty(EDirtyFlag::Transform);
}

void UDecalComponent::SetDecalMaterial(UMaterialInterface* NewDecalMaterial)
{
	DecalMaterial = NewDecalMaterial;
	DecalTextureName = FName();
	DecalTexture = nullptr;
	DecalMaterialSlot.Path = (DecalMaterial != nullptr) ? DecalMaterial->GetAssetPathFileName() : "None";
	MarkProxyDirty(EDirtyFlag::Material);
}

UMaterialInterface* UDecalComponent::GetDecalMaterial() const
{
	return DecalMaterial;
}

void UDecalComponent::SetDecalTexture(const FName& TextureName)
{
	DecalMaterial = nullptr;
	DecalTextureName = TextureName;
	DecalTexture = FResourceManager::Get().FindTexture(TextureName);
	DecalMaterialSlot.Path = DecalTexture ? MakeDecalTextureMaterialPath(TextureName) : "None";
	MarkProxyDirty(EDirtyFlag::Material);
}

const FTextureResource* UDecalComponent::GetDecalTexture() const
{
	return DecalTexture;
}

bool UDecalComponent::FitSizeToTextureAspect()
{
	uint32 TextureWidth = 0;
	uint32 TextureHeight = 0;

	if (DecalTexture)
	{
		TextureWidth = DecalTexture->Width;
		TextureHeight = DecalTexture->Height;
	}
	else if (DecalMaterial)
	{
		if (UTexture2D* DiffuseTexture = DecalMaterial->GetDiffuseTexture())
		{
			TextureWidth = DiffuseTexture->GetWidth();
			TextureHeight = DiffuseTexture->GetHeight();
		}
	}

	if (TextureWidth == 0 || TextureHeight == 0)
	{
		return false;
	}

	if (DecalSize.Y <= 0.0f)
	{
		DecalSize.Y = 1.0f;
	}

	const FVector WorldScale = GetWorldScale();
	const float SafeWorldScaleY = (WorldScale.Y > 0.001f) ? WorldScale.Y : 1.0f;
	const float SafeWorldScaleZ = (WorldScale.Z > 0.001f) ? WorldScale.Z : 1.0f;
	const float TextureAspectYZ = static_cast<float>(TextureHeight) / static_cast<float>(TextureWidth);
	DecalSize.Z = (DecalSize.Y * SafeWorldScaleY * TextureAspectYZ) / SafeWorldScaleZ;
	MarkProxyDirty(EDirtyFlag::Transform);
	MarkWorldBoundsDirty();
	return true;
}

void UDecalComponent::SetDecalSize(const FVector& InSize)
{
	DecalSize = InSize;
	MarkProxyDirty(EDirtyFlag::Transform);
	MarkWorldBoundsDirty();
}

FMatrix UDecalComponent::GetTransformIncludingDecalSize() const
{
    return FMatrix::MakeScaleMatrix(DecalSize) * GetWorldMatrix();
}

void UDecalComponent::UpdateWorldAABB() const
{
	// 1. DecalSize가 반영된 로컬 Extent(반지름)를 계산합니다.
	// 렌더링에 쓰이는 큐브 메쉬가 크기 1(-0.5 ~ 0.5)이므로 0.5를 곱해줍니다.
	FVector LExt = DecalSize * 0.5f;

	// 2. 부모(UPrimitiveComponent)의 투영 공식을 그대로 사용하여 OBB의 꼭짓점들을 AABB로 변환합니다.
	FMatrix worldMatrix = GetWorldMatrix();

	float NewEx = std::abs(worldMatrix.M[0][0]) * LExt.X + std::abs(worldMatrix.M[1][0]) * LExt.Y + std::abs(worldMatrix.M[2][0]) * LExt.Z;
	float NewEy = std::abs(worldMatrix.M[0][1]) * LExt.X + std::abs(worldMatrix.M[1][1]) * LExt.Y + std::abs(worldMatrix.M[2][1]) * LExt.Z;
	float NewEz = std::abs(worldMatrix.M[0][2]) * LExt.X + std::abs(worldMatrix.M[1][2]) * LExt.Y + std::abs(worldMatrix.M[2][2]) * LExt.Z;

	// 3. 엔진 AABB 변수에 갱신
	FVector WorldCenter = GetWorldLocation();
	WorldAABBMinLocation = WorldCenter - FVector(NewEx, NewEy, NewEz);
	WorldAABBMaxLocation = WorldCenter + FVector(NewEx, NewEy, NewEz);

	bWorldAABBDirty = false;
	bHasValidWorldAABB = true;
}

bool UDecalComponent::LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult)
{
	const FMeshData* Data = GetMeshData();
	if (!Data || Data->Indices.empty())
	{
		return false;
	}

	const FMatrix DecalWorldMatrix = GetTransformIncludingDecalSize();
	const FMatrix DecalWorldInverseMatrix = DecalWorldMatrix.GetInverse();
	const bool bHit = FRayUtils::RaycastTriangles(
		Ray,
		DecalWorldMatrix,
		DecalWorldInverseMatrix,
		&Data->Vertices[0].Position,
		sizeof(FVertex),
		Data->Indices,
		OutHitResult);

	if (bHit)
	{
		OutHitResult.HitComponent = this;
	}
	return bHit;
}

FMeshBuffer* UDecalComponent::GetMeshBuffer() const
{
    return &FMeshBufferManager::Get().GetMeshBuffer(EMeshShape::Cube);
}

const FMeshData* UDecalComponent::GetMeshData() const
{
    return &FMeshBufferManager::Get().GetMeshData(EMeshShape::Cube);
}

FPrimitiveSceneProxy* UDecalComponent::CreateSceneProxy()
{
    return new FDecalSceneProxy(this);
}

void UDecalComponent::CreateRenderState()
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

	ArrowOuterProxy = new FDecalArrowSceneProxy(this, false);
	Scene->RegisterProxy(ArrowOuterProxy);

	ArrowInnerProxy = new FDecalArrowSceneProxy(this, true);
	Scene->RegisterProxy(ArrowInnerProxy);
}

void UDecalComponent::DestroyRenderState()
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

void UDecalComponent::Serialize(FArchive& Ar)
{
    UPrimitiveComponent::Serialize(Ar);

    Ar << FadeStartDelay;
    Ar << FadeDuration;
    Ar << FadeInDuration;
    Ar << FadeInStartDelay;
    Ar << bDestroyOwnerAfterFade;
    Ar << DecalSize;
    Ar << DecalColor;
    Ar << DecalMaterialSlot.Path;
    Ar << DecalMaterialSlot.bUVScroll;
}

void UDecalComponent::PostDuplicate()
{
    UPrimitiveComponent::PostDuplicate();

	if (IsDecalTextureMaterialPath(DecalMaterialSlot.Path))
	{
		SetDecalTexture(FName(GetDecalTextureNameFromMaterialPath(DecalMaterialSlot.Path)));
	}
	else if (!DecalMaterialSlot.Path.empty() && DecalMaterialSlot.Path != "None")
	{
		DecalMaterial = FObjManager::GetOrLoadMaterial(DecalMaterialSlot.Path);
		DecalTextureName = FName();
		DecalTexture = nullptr;
	}
	else
	{
		DecalMaterial = nullptr;
		DecalTextureName = FName();
		DecalTexture = nullptr;
	}

    MarkRenderStateDirty();
}

void UDecalComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UPrimitiveComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Decal Size", EPropertyType::Vec3, &DecalSize });
    OutProps.push_back({ "Decal Color", EPropertyType::Vec4, &DecalColor });
    OutProps.push_back({ "Decal Material", EPropertyType::MaterialSlot, &DecalMaterialSlot });
}

void UDecalComponent::PostEditProperty(const char* PropertyName)
{
    UPrimitiveComponent::PostEditProperty(PropertyName);

    if (std::strcmp(PropertyName, "Decal Size") == 0)
    {
        MarkProxyDirty(EDirtyFlag::Transform);
		MarkWorldBoundsDirty();
    }
    else if (std::strcmp(PropertyName, "Decal Color") == 0)
    {
        SetDecalColor(DecalColor);
    }
    else if (std::strcmp(PropertyName, "Decal Material") == 0 || std::strcmp(PropertyName, "Element 0") == 0)
    {
		if (DecalMaterialSlot.Path.empty() || DecalMaterialSlot.Path == "None")
		{
			SetDecalMaterial(nullptr);
		}
		else if (IsDecalTextureMaterialPath(DecalMaterialSlot.Path))
		{
			SetDecalTexture(FName(GetDecalTextureNameFromMaterialPath(DecalMaterialSlot.Path)));
		}
		else
		{
			SetDecalMaterial(FObjManager::GetOrLoadMaterial(DecalMaterialSlot.Path));
		}
    }
}

