#include "Render/Proxy/MeshDecalSceneProxy.h"

#include "Components/MeshDecalComponent.h"
#include "Materials/MaterialInterface.h"
#include "Render/Resource/ShaderManager.h"
#include "Render/Types/VertexTypes.h"
#include "Runtime/Engine.h"
#include "Texture/Texture2D.h"

#include <cstdint>

FMeshDecalSceneProxy::FMeshDecalSceneProxy(UMeshDecalComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	bSupportsOutline = false;
	bShowAABB = false;
}

UMeshDecalComponent* FMeshDecalSceneProxy::GetMeshDecalComponent() const
{
	return static_cast<UMeshDecalComponent*>(Owner);
}

void FMeshDecalSceneProxy::UpdateTransform()
{
	UMeshDecalComponent* MeshDecal = GetMeshDecalComponent();
	PerObjectConstants = FPerObjectConstants::FromWorldMatrix(MeshDecal->GetMeshDecalLocalToWorldMatrix());
	CachedWorldPos = PerObjectConstants.Model.GetLocation();
	CachedBounds = MeshDecal->GetWorldBoundingBox();
	MarkPerObjectCBDirty();
}

void FMeshDecalSceneProxy::UpdateMaterial()
{
	UMeshDecalComponent* MeshDecal = GetMeshDecalComponent();
	UpdateSortKey();
	if (MeshDecal)
	{
		MaterialSortKey = static_cast<uint32>(MeshDecal->GetSortOrder() - INT32_MIN);
	}
}

void FMeshDecalSceneProxy::UpdateMesh()
{
	UMeshDecalComponent* MeshDecal = GetMeshDecalComponent();
	MeshDecal->EnsureMeshDecalMeshBuilt();
	const FMeshDecalRenderableMesh& SrcMesh = MeshDecal->GetRenderableMesh();

	MeshBuffer = nullptr;
	SectionDraws.clear();

	Shader = FShaderManager::Get().GetShader(EShaderType::MeshDecal);
	Pass = ERenderPass::MeshDecal;

	if (SrcMesh.IsEmpty())
	{
		UpdateSortKey();
		return;
	}

	TMeshData<FVertexPNCT> MeshData;
	MeshData.Vertices.reserve(SrcMesh.Vertices.size());
	MeshData.Indices = SrcMesh.Indices;

	for (const FMeshDecalRenderableVertex& Src : SrcMesh.Vertices)
	{
		FVertexPNCT Dst = {};
		Dst.Position = Src.Position;
		Dst.Normal = Src.Normal;
		Dst.Color = Src.Color;
		Dst.UV = Src.UV;
		MeshData.Vertices.push_back(Dst);
	}

	OwnedMeshBuffer.Create(GEngine->GetRenderer().GetFD3DDevice().GetDevice(), MeshData);
	MeshBuffer = &OwnedMeshBuffer;

	for (const FMeshDecalRenderableSection& SrcSection : SrcMesh.Sections)
	{
		FMeshSectionDraw Draw = {};
		Draw.FirstIndex = SrcSection.FirstIndex;
		Draw.IndexCount = SrcSection.IndexCount;

		if (UMaterialInterface* Material = MeshDecal->GetMeshDecalMaterial())
		{
			if (UTexture2D* Diffuse = Material->GetDiffuseTexture())
			{
				Draw.DiffuseSRV = Diffuse->GetSRV();
			}
			Draw.DiffuseColor = Material->GetDiffuseColor();
		}

		SectionDraws.push_back(Draw);
	}

	UpdateSortKey();
	MaterialSortKey = static_cast<uint32>(MeshDecal->GetSortOrder() - INT32_MIN);
}
