#pragma once

#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Resource/Buffer.h"

class UMeshDecalComponent;

class FMeshDecalSceneProxy : public FPrimitiveSceneProxy
{
public:
	explicit FMeshDecalSceneProxy(UMeshDecalComponent* InComponent);
	~FMeshDecalSceneProxy() override = default;

	void UpdateTransform() override;
	void UpdateMaterial() override;
	void UpdateMesh() override;
	void CollectEntries(FRenderBus& Bus) override;

private:
	UMeshDecalComponent* GetMeshDecalComponent() const;

private:
	FMeshBuffer OwnedMeshBuffer;
};

class FMeshDecalArrowSceneProxy : public FPrimitiveSceneProxy
{
public:
	explicit FMeshDecalArrowSceneProxy(UMeshDecalComponent* InComponent, bool bInner = false);

	void UpdateMesh() override;
	void UpdatePerViewport(const FRenderBus& Bus) override;

private:
	UMeshDecalComponent* GetMeshDecalComponent() const;

	bool bIsInner = false;
};
