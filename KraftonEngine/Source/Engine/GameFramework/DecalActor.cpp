#include "GameFramework/DecalActor.h"

#include "Component/DecalComponent.h"
#include "Resource/ResourceManager.h"

IMPLEMENT_CLASS(ADecalActor, AActor)

void ADecalActor::InitDefaultComponents()
{
	if (Decal)
	{
		return;
	}

	Decal = AddComponent<UDecalComponent>();
	if (!Decal)
	{
		return;
	}
	SetRootComponent(Decal);
	Decal->SetRelativeScale(FVector(2.0f, 2.0f, 2.0f));

	if (FResourceManager::Get().FindTexture(FName("Pawn")))
	{
		Decal->SetTexture(FName("Pawn"));
	}
	else
	{
		const TArray<FString> TextureNames = FResourceManager::Get().GetTextureNames();
		if (!TextureNames.empty())
		{
			Decal->SetTexture(FName(TextureNames[0]));
		}
	}
}

UDecalComponent* ADecalActor::GetDecal() const
{
	if (Decal)
	{
		return Decal;
	}

	for (UActorComponent* Component : OwnedComponents)
	{
		if (UDecalComponent* DecalComponent = Cast<UDecalComponent>(Component))
		{
			Decal = DecalComponent;
			return Decal;
		}
	}

	return nullptr;
}

void ADecalActor::SetDecalMaterial(UMaterialInterface* /*NewDecalMaterial*/)
{
}

UMaterialInterface* ADecalActor::GetDecalMaterial() const
{
	return nullptr;
}

void ADecalActor::Serialize(FArchive& Ar)
{
	AActor::Serialize(Ar);
}
