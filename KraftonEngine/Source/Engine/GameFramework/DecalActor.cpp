#include "GameFramework/DecalActor.h"
#include "Object/ObjectFactory.h"
#include "Components/DecalComponent.h"
#include "Components/BillboardComponent.h"

IMPLEMENT_CLASS(ADecalActor, AActor)

ADecalActor::ADecalActor()
{
    Decal = AddComponent<UDecalComponent>();
    SetRootComponent(Decal);

	SpriteComponent = AddComponent<UBillboardComponent>();
	SpriteComponent->AttachToComponent(Decal);
	SpriteComponent->SetTexture(FName("DecalSprite"));
}
