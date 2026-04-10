#include "FireBallComponent.h"

#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

#include <cstring>

IMPLEMENT_CLASS(UFireBallComponent, UPrimitiveComponent)

UFireBallComponent::UFireBallComponent()
{
	SyncLocalExtents();
}

FPrimitiveSceneProxy* UFireBallComponent::CreateSceneProxy()
{
	return nullptr;
}

void UFireBallComponent::UpdateWorldAABB() const
{
	const FVector Center = GetWorldLocation();
	const FVector Extent(Radius, Radius, Radius);
	WorldAABBMinLocation = Center - Extent;
	WorldAABBMaxLocation = Center + Extent;
	bWorldAABBDirty = false;
	bHasValidWorldAABB = (Radius > 0.0f);
}

bool UFireBallComponent::LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult)
{
	return false;
}

void UFireBallComponent::Serialize(FArchive& Ar)
{
	UPrimitiveComponent::Serialize(Ar);
	Ar << Intensity;
	Ar << Radius;
	Ar << RadiusFallOff;
	Ar << Color.R;
	Ar << Color.G;
	Ar << Color.B;
	Ar << Color.A;

	if (Ar.IsLoading())
	{
		SyncLocalExtents();
	}
}

void UFireBallComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPrimitiveComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Intensity", EPropertyType::Float, &Intensity, 0.0f, 16.0f, 0.05f });
	OutProps.push_back({ "Radius", EPropertyType::Float, &Radius, 0.0f, 100.0f, 0.1f });
	OutProps.push_back({ "Radius FallOff", EPropertyType::Float, &RadiusFallOff, 0.01f, 16.0f, 0.05f });
	OutProps.push_back({ "Color", EPropertyType::Vec4, &Color, 0.0f, 1.0f, 0.01f });
}

void UFireBallComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);

	if (std::strcmp(PropertyName, "Intensity") == 0 && Intensity < 0.0f)
	{
		Intensity = 0.0f;
	}
	else if (std::strcmp(PropertyName, "Radius") == 0 && Radius < 0.0f)
	{
		Radius = 0.0f;
	}
	else if (std::strcmp(PropertyName, "Radius FallOff") == 0)
	{
		if (RadiusFallOff < 0.01f)
		{
			RadiusFallOff = 0.01f;
		}
	}

	if (std::strcmp(PropertyName, "Radius") == 0)
	{
		SyncLocalExtents();
		MarkWorldBoundsDirty();
	}
}

void UFireBallComponent::SyncLocalExtents()
{
	LocalExtents = FVector(Radius, Radius, Radius);
}
