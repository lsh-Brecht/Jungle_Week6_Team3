#pragma once
#include "Render/Proxy/PrimitiveSceneProxy.h"

class UDecalComponent;

// ============================================================
// FDecalSceneProxy — UDecalComponent 전용 프록시
// ============================================================
class FDecalSceneProxy : public FPrimitiveSceneProxy
{
public:
	FDecalSceneProxy(UDecalComponent* InComponent);
	~FDecalSceneProxy() override;

	void UpdateMaterial() override;
	void UpdateMesh() override;

private:
	UDecalComponent* GetDecalComponent() const;

	FConstantBuffer DecalCB;
	const FTextureResource* DecalTexture = nullptr;
};
