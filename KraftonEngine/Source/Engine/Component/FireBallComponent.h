#pragma once

#include "PrimitiveComponent.h"
#include "Core/EngineTypes.h"

// 복잡한 lighting 계산 대신, 월드 내 특정 위치 주변에 색을 더하는 간단한 반경 기반 효과용 컴포넌트.
class FPrimitiveSceneProxy;

class UFireBallComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UFireBallComponent, UPrimitiveComponent)

	UFireBallComponent();
	~UFireBallComponent() override = default;

	FPrimitiveSceneProxy* CreateSceneProxy() override;
	void UpdateWorldAABB() const override;
	bool LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult) override;
	bool SupportsOutline() const override { return false; }

	void Serialize(FArchive& Ar) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

	void PostEditProperty(const char* PropertyName) override;

	float GetIntensity() const { return Intensity; }
	float GetRadius() const { return Radius; }
	float GetRadiusFallOff() const { return RadiusFallOff; }
	const FLinearColor& GetColor() const { return Color; }

	void SetIntensity(float InIntensity) { Intensity = InIntensity; }
	void SetRadius(float InRadius) { Radius = InRadius; }
	void SetRadiusFallOff(float InRadiusFallOff) { RadiusFallOff = InRadiusFallOff; }
	void SetColor(const FLinearColor& InColor) { Color = InColor; }

private:
	void SyncLocalExtents();

	float Intensity = 1.0f;
	float Radius = 4.0f;
	float RadiusFallOff = 2.0f;
	FLinearColor Color = FLinearColor(1.0f, 0.45f, 0.15f, 1.0f);
};