#pragma once

#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Resource/Buffer.h"

class UDecalComponent;

class FDecalSceneProxy : public FPrimitiveSceneProxy
{
public:
	FDecalSceneProxy(UDecalComponent* InComponent);
	~FDecalSceneProxy() override = default;

	void UpdateTransform() override;
	void UpdateMaterial() override;
	void UpdateMesh() override;

private:
	UDecalComponent* GetDecalComponent() const;

private:
	FMeshBuffer OwnedMeshBuffer;
};
