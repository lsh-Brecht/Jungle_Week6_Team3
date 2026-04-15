#include "GameFramework/MeshDecalActor.h"

#include "Object/ObjectFactory.h"
#include "Components/BillboardComponent.h"
#include "Components/MeshDecalComponent.h"
#include "Mesh/ObjManager.h"

IMPLEMENT_CLASS(AMeshDecalActor, AActor)

AMeshDecalActor::AMeshDecalActor()
{
	MeshDecal = AddComponent<UMeshDecalComponent>();
	SetRootComponent(MeshDecal);

	SpriteComponent = AddComponent<UBillboardComponent>();
	SpriteComponent->AttachToComponent(MeshDecal);
	SpriteComponent->SetTexture(FName("DecalIcon"));
}

void AMeshDecalActor::SetMeshDecalMaterial(UMaterialInterface* NewMaterial)
{
	if (MeshDecal)
	{
		MeshDecal->SetMeshDecalMaterial(NewMaterial);
	}
}

void AMeshDecalActor::SetMeshDecalMaterial(const FString& MaterialPath)
{
	if (MaterialPath.empty() || MaterialPath == "None")
	{
		SetMeshDecalMaterial(static_cast<UMaterialInterface*>(nullptr));
		return;
	}
	SetMeshDecalMaterial(FObjManager::GetOrLoadMaterial(MaterialPath));
}

UMaterialInterface* AMeshDecalActor::GetMeshDecalMaterial() const
{
	return MeshDecal ? MeshDecal->GetMeshDecalMaterial() : nullptr;
}

void AMeshDecalActor::SetMeshDecalSize(const FVector& InSize)
{
	if (MeshDecal)
	{
		MeshDecal->SetMeshDecalSize(InSize);
	}
}

FVector AMeshDecalActor::GetMeshDecalSize() const
{
	return MeshDecal ? MeshDecal->GetMeshDecalSize() : FVector(0.0f, 0.0f, 0.0f);
}

void AMeshDecalActor::BeginPlay()
{
	AActor::BeginPlay();

	for (UActorComponent* Component : GetComponents())
	{
		if (UBillboardComponent* Billboard = Cast<UBillboardComponent>(Component))
		{
			Billboard->SetVisibility(false);
		}
	}
}

void AMeshDecalActor::Serialize(FArchive& Ar)
{
	AActor::Serialize(Ar);
}

