#pragma once
#include "Render/Proxy/PrimitiveSceneProxy.h"

class UDecalComponent;

class FDecalSceneProxy : public FPrimitiveSceneProxy
{
public:
    explicit FDecalSceneProxy(UDecalComponent* InComponent);

    void UpdateMesh() override;
    void UpdateMaterial() override;
    void UpdateTransform() override;

	void CollectEntries(FRenderBus& Bus) override;

	// 매 프레임 카메라 기준 OBB-Frustum 컬링 수행 (bPerViewportUpdate = true)
	void UpdatePerViewport(const FRenderBus& Bus) override;

private:
    UDecalComponent* GetDecalComponent() const;
    void RebuildSectionDraw();
};

class FDecalArrowSceneProxy : public FPrimitiveSceneProxy
{
public:
	explicit FDecalArrowSceneProxy(UDecalComponent* InComponent, bool bInner = false);

	void UpdateMesh() override;
	void UpdatePerViewport(const FRenderBus& Bus) override;

private:
	UDecalComponent* GetDecalComponent() const;

	bool bIsInner = false;
};
