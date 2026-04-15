#include "Render/Proxy/DecalSceneProxy.h"

#include "Components/DecalComponent.h"
#include "Materials/MaterialInterface.h"
#include "Resource/ResourceManager.h"
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

	// CPU가 SAT까지 통과시킨 triangle들을 decal local 공간으로 저장해 두므로,
	// 렌더링 때는 "decal local -> world" 모델 행렬만 넘기면 원래 receiver 표면 위치에 다시 올라갑니다.
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
	LastOverlappingObjectCount = SrcMesh.IsEmpty() ? 0u : 1u;

	MeshBuffer = nullptr;
	SectionDraws.clear();

	if (SrcMesh.IsEmpty())
	{
		Shader = FShaderManager::Get().GetShader(EShaderType::Decal);
		Pass = ERenderPass::Decal;
		UpdateSortKey();
		return;
	}

	// 여기서 GPU에 올리는 메쉬는 "박스로 clip된 메쉬"가 아니라,
	// SAT를 통과한 receiver triangle을 decal local 좌표로 옮긴 결과입니다.
	// 실제 decal box 마스킹은 pixel shader가 localPos 범위 검사로 처리합니다.
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
		else if (Decal->GetDecalTextureName().IsValid())
		{
			if (const FTextureResource* Texture = FResourceManager::Get().FindTexture(Decal->GetDecalTextureName()))
			{
				Draw.DiffuseSRV = Texture->SRV;
				Draw.DiffuseColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
			}
		}

		SectionDraws.push_back(Draw);
	}

	UpdateSortKey();
}
