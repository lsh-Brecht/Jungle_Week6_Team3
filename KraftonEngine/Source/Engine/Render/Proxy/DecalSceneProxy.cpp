#include "Render/Proxy/DecalSceneProxy.h"

#include "Component/DecalComponent.h"
#include "Materials/MaterialInterface.h"
#include "Render/Resource/ShaderManager.h"
#include "Render/Types/VertexTypes.h"
#include "Runtime/Engine.h"
#include "Texture/Texture2D.h"

FDecalSceneProxy::FDecalSceneProxy(UDecalComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
}

UDecalComponent* FDecalSceneProxy::GetDecalComponent() const
{
	return static_cast<UDecalComponent*>(Owner);
}

void FDecalSceneProxy::UpdateTransform()
{
	UDecalComponent* Decal = GetDecalComponent();

	// RenderableMesh의 Position이 decal local 기준이므로
	// 모델 행렬은 decal local -> world 여야 합니다.
	PerObjectConstants = FPerObjectConstants::FromWorldMatrix(Decal->GetDecalLocalToWorldMatrix());
	CachedWorldPos = PerObjectConstants.Model.GetLocation();
	CachedBounds = Decal->GetWorldBoundingBox();
	MarkPerObjectCBDirty();
}

void FDecalSceneProxy::UpdateMaterial()
{
	UpdateSortKey();
}

void FDecalSceneProxy::UpdateMesh()
{
	UDecalComponent* Decal = GetDecalComponent();
	Decal->EnsureDecalMeshBuilt();
	const FDecalRenderableMesh& SrcMesh = Decal->GetRenderableMesh();

	MeshBuffer = nullptr;
	SectionDraws.clear();

	if (SrcMesh.IsEmpty())
	{
		Shader = FShaderManager::Get().GetShader(EShaderType::Decal);
		Pass = ERenderPass::Decal;
		UpdateSortKey();
		return;
	}

	TMeshData<FVertexPNCT> MeshData;
	MeshData.Vertices.reserve(SrcMesh.Vertices.size());
	MeshData.Indices = SrcMesh.Indices;

	for (const FDecalRenderableVertex& Src : SrcMesh.Vertices)
	{
		FVertexPNCT Dst;
		Dst.Position = Src.Position;
		Dst.Normal = Src.Normal;
		Dst.Color = Src.Color;
		Dst.UV = Src.UV;
		MeshData.Vertices.push_back(Dst);
	}

	OwnedMeshBuffer.Create(GEngine->GetRenderer().GetFD3DDevice().GetDevice(), MeshData);
	MeshBuffer = &OwnedMeshBuffer;

	Shader = FShaderManager::Get().GetShader(EShaderType::Decal);
	Pass = ERenderPass::Decal;

	for (const FDecalRenderableSection& SrcSection : SrcMesh.Sections)
	{
		FMeshSectionDraw Draw;
		Draw.FirstIndex = SrcSection.FirstIndex;
		Draw.IndexCount = SrcSection.IndexCount;

		if (UMaterialInterface* Mat = Decal->GetDecalMaterial())
		{
			if (UTexture2D* Diffuse = Mat->GetDiffuseTexture())
			{
				Draw.DiffuseSRV = Diffuse->GetSRV();
			}
			Draw.DiffuseColor = Mat->GetDiffuseColor();
		}

		SectionDraws.push_back(Draw);
	}

	UpdateSortKey();
}
