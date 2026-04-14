#include "GameFramework/DecalActor.h"
#include "Object/ObjectFactory.h"
#include "Components/DecalComponent.h"
#include "Components/BillboardComponent.h"
#include "Mesh/ObjManager.h"

namespace
{
	UMaterialInterface* ResolveDefaultDecalMaterial()
	{
		FObjManager::ScanMaterialAssets();
		const TArray<FMaterialAssetListItem>& MaterialFiles = FObjManager::GetAvailableMaterialFiles();
		if (!MaterialFiles.empty())
		{
			return FObjManager::GetOrLoadMaterial(MaterialFiles[0].FullPath);
		}

		return nullptr;
	}
}

IMPLEMENT_CLASS(ADecalActor, AActor)

ADecalActor::ADecalActor()
{
    Decal = AddComponent<UDecalComponent>();
	SetRootComponent(Decal);

	SpriteComponent = AddComponent<UBillboardComponent>();
	SpriteComponent->AttachToComponent(Decal);
	SpriteComponent->SetTexture(FName("DecalSprite"));

   SetDecalMaterial(ResolveDefaultDecalMaterial());
}

void ADecalActor::SetDecalMaterial(UMaterialInterface* NewDecalMaterial)
{
	if (Decal)
	{
		Decal->SetDecalMaterial(NewDecalMaterial);
	}
}

void ADecalActor::SetDecalMaterial(const FString& MaterialPath)
{
	if (MaterialPath.empty() || MaterialPath == "None")
	{
		SetDecalMaterial(static_cast<UMaterialInterface*>(nullptr));
		return;
	}

	SetDecalMaterial(FObjManager::GetOrLoadMaterial(MaterialPath));
}

UMaterialInterface* ADecalActor::GetDecalMaterial() const
{
	return Decal ? Decal->GetDecalMaterial() : nullptr;
}

//void ADecalActor::SetDecalSize(const FVector& InDecalSize)
//{
//	if (Decal)
//	{
//		Decal->DecalSize = InDecalSize;
//		Decal->MarkProxyDirty(EDirtyFlag::Transform);
//	}
//}

FVector ADecalActor::GetDecalSize() const
{
	return Decal ? Decal->DecalSize : FVector(0.0f, 0.0f, 0.0f);
}

void ADecalActor::BeginPlay()
{
	AActor::BeginPlay();

	if (SpriteComponent)
	{
		SpriteComponent->SetVisibility(false);
	}
}

void ADecalActor::Serialize(FArchive& Ar)
{
	AActor::Serialize(Ar);
}
