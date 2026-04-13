#include "Components/DecalComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(UDecalComponent, USceneComponent)

void UDecalComponent::SetFadeOut(float StartDelay, float Duration, bool DestroyOwnerAfterFade)
{
}

void UDecalComponent::SetDecalMaterial(UMaterialInterface* NewDecalMaterial)
{
    DecalMaterial = NewDecalMaterial;
}

UMaterialInterface* UDecalComponent::GetDecalMaterial() const
{
    return DecalMaterial;
}

FTransform UDecalComponent::GetTransformIncludingDecalSize() const
{
    return GetRelativeTransform();
}

FDeferredDecalProxy* UDecalComponent::CreateSceneProxy()
{
    return nullptr;
}

void UDecalComponent::Serialize(FArchive& Ar)
{
    USceneComponent::Serialize(Ar);
}
