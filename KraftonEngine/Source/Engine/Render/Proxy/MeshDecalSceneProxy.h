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

private:
	UMeshDecalComponent* GetMeshDecalComponent() const;

private:
	FMeshBuffer OwnedMeshBuffer;
};
