#include "Components/DecalComponent.h"
#include "Components/DecalComponent.h"

#include <cstring>

#include "Mesh/ObjManager.h"
#include "Materials/MaterialInterface.h"
#include "Object/ObjectFactory.h"
#include "Render/Proxy/DecalSceneProxy.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(UDecalComponent, UPrimitiveComponent)

UDecalComponent::UDecalComponent()
{
}

float UDecalComponent::GetFadeStartDelay() const
{
    return FadeStartDelay;
}

float UDecalComponent::GetFadeDuration() const
{
    return FadeDuration;
}

float UDecalComponent::GetFadeInStartDelay() const
{
    return FadeInStartDelay;
}

float UDecalComponent::GetFadeInDuration() const
{
    return FadeInDuration;
}

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
    DecalMaterialSlot.Path = (DecalMaterial != nullptr) ? DecalMaterial->GetAssetPathFileName() : "None";
    MarkProxyDirty(EDirtyFlag::Material);
}

UMaterialInterface* UDecalComponent::GetDecalMaterial() const
{
    return DecalMaterial;
}

FMatrix UDecalComponent::GetTransformIncludingDecalSize() const
{
    return FMatrix::MakeScaleMatrix(DecalSize) * GetWorldMatrix();
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

    if (!DecalMaterialSlot.Path.empty() && DecalMaterialSlot.Path != "None")
    {
        DecalMaterial = FObjManager::GetOrLoadMaterial(DecalMaterialSlot.Path);
    }
    else
    {
        DecalMaterial = nullptr;
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
        else
        {
            SetDecalMaterial(FObjManager::GetOrLoadMaterial(DecalMaterialSlot.Path));
        }
    }
}

