#include "Render/Proxy/DecalSceneProxy.h"

#include "Component/DecalComponent.h"
#include "Render/Resource/ConstantBufferPool.h"
#include "Render/Resource/ShaderManager.h"

namespace
{
	struct FDecalConstants
	{
		FMatrix WorldToDecal;
		FVector4 Color;
	};
}

FDecalSceneProxy::FDecalSceneProxy(UDecalComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	// 최초 1회 초기화
	UpdateMesh();
}

FDecalSceneProxy::~FDecalSceneProxy()
{
}

UDecalComponent* FDecalSceneProxy::GetDecalComponent() const
{
	return static_cast<UDecalComponent*>(Owner);
}

void FDecalSceneProxy::UpdateMaterial()
{
	UDecalComponent* DecalComp = GetDecalComponent();
	if (!DecalComp)
	{
		return;
	}

	DecalTexture = DecalComp->GetTexture();
	DecalSRV = DecalTexture ? DecalTexture->SRV : nullptr;
	if (!SectionDraws.empty())
	{
		SectionDraws[0].DiffuseSRV = DecalSRV;
		UpdateSortKey();
	}

	auto& CB = ExtraCB.Bind<FDecalConstants>(
		FConstantBufferPool::Get().GetBuffer(ECBSlot::PerShader0, sizeof(FDecalConstants)),
		ECBSlot::PerShader0);
	CB.WorldToDecal = DecalComp->GetWorldMatrix().GetInverse();
	CB.Color = DecalComp->GetColor();
}

void FDecalSceneProxy::UpdateMesh()
{
	UpdateMaterial();

	MeshBuffer = Owner->GetMeshBuffer();
	SectionDraws.clear();
	if (MeshBuffer)
	{
		FMeshSectionDraw Draw;
		Draw.DiffuseSRV = DecalSRV;
		Draw.DiffuseColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		Draw.FirstIndex = 0;
		Draw.IndexCount = MeshBuffer->GetIndexBuffer().GetIndexCount();
		SectionDraws.push_back(Draw);
	}
	Shader = FShaderManager::Get().GetShader(EShaderType::Decal);
	Pass = ERenderPass::Decal;
	bSupportsOutline = false;
	UpdateSortKey();
}
