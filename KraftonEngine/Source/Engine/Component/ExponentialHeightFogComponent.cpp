#include "ExponentialHeightFogComponent.h"
#include "Object/ObjectFactory.h"
#include "SceneComponent.h"
#include "Serialization/Archive.h"
#include <cstring>

IMPLEMENT_CLASS(UExponentialHeightFogComponent, UPrimitiveComponent)

UExponentialHeightFogComponent::UExponentialHeightFogComponent()
{
	SanitizeProperties();
}

void UExponentialHeightFogComponent::SanitizeProperties()
{
	FogDensity = Clamp(FogDensity, 0.0f, 100000.0f);
	FogHeightFalloff = Clamp(FogHeightFalloff, 0.0f, 100000.0f);
	StartDistance = Clamp(StartDistance, 0.0f, 100000.0f);
	FogCutoffDistance = Clamp(FogCutoffDistance, 0.0f, 100000.0f);
	FogMaxOpacity = Clamp(FogMaxOpacity, 0.0f, 1.0f);
	EndDistance = Clamp(EndDistance, 0.0f, 100000.0f);
	DirectionalInscatteringExponent = Clamp(DirectionalInscatteringExponent, 0.0f, 100000.0f);
	DirectionalInscatteringStartDistance = Clamp(DirectionalInscatteringStartDistance, 0.0f, 100000.0f);
}

void UExponentialHeightFogComponent::Serialize(FArchive& Ar)
{
	UPrimitiveComponent::Serialize(Ar);

	Ar << FogDensity;
	Ar << FogHeightFalloff;
	Ar << StartDistance;
	Ar << FogCutoffDistance;
	Ar << FogMaxOpacity;

	Ar << FogInscatteringColor;
	Ar << FogInscatteringLuminance;

	Ar << EndDistance;
	Ar << DirectionalInscatteringExponent;
	Ar << DirectionalInscatteringStartDistance;
	Ar << DirectionalInscatteringLuminance;

	if (Ar.IsLoading())
	{
		SanitizeProperties();
	}
}

void UExponentialHeightFogComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPrimitiveComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Fog Density", EPropertyType::Float, &FogDensity, 0.0f, 10.0f, 0.001f });
	OutProps.push_back({ "Fog Height Falloff", EPropertyType::Float, &FogHeightFalloff, 0.0f, 10.0f, 0.001f });
	OutProps.push_back({ "Start Distance", EPropertyType::Float, &StartDistance, 0.0f, 10000.0f, 1.0f });
	OutProps.push_back({ "Fog Cutoff Distance", EPropertyType::Float, &FogCutoffDistance, 0.0f, 100000.0f, 1.0f });
	OutProps.push_back({ "Fog Max Opacity", EPropertyType::Float, &FogMaxOpacity, 0.0f, 1.0f, 0.01f });
	OutProps.push_back({ "End Distance", EPropertyType::Float, &EndDistance, 0.0f, 100000.0f, 1.0f });
	OutProps.push_back({ "Directional Inscattering Exponent", EPropertyType::Float, &DirectionalInscatteringExponent, 0.0f, 128.0f, 0.1f });
	OutProps.push_back({ "Directional Inscattering Start Distance", EPropertyType::Float, &DirectionalInscatteringStartDistance, 0.0f, 100000.0f, 1.0f });
}

void UExponentialHeightFogComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);

	if (std::strcmp(PropertyName, "Fog Density") == 0 ||
		std::strcmp(PropertyName, "Fog Height Falloff") == 0 ||
		std::strcmp(PropertyName, "Start Distance") == 0 ||
		std::strcmp(PropertyName, "Fog Cutoff Distance") == 0 ||
		std::strcmp(PropertyName, "Fog Max Opacity") == 0 ||
		std::strcmp(PropertyName, "End Distance") == 0 ||
		std::strcmp(PropertyName, "Directional Inscattering Exponent") == 0 ||
		std::strcmp(PropertyName, "Directional Inscattering Start Distance") == 0)
	{
		SanitizeProperties();
	}
}
