#include "Render/Proxy/MeshDecalSceneProxy.h"

#include "Components/MeshDecalComponent.h"
#include "Materials/MaterialInterface.h"
#include "Render/Pipeline/RenderBus.h"
#include "Render/Resource/ConstantBufferPool.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Render/Resource/ShaderManager.h"
#include "Render/Types/VertexTypes.h"
#include "Runtime/Engine.h"
#include "Texture/Texture2D.h"

#include <cstdint>

namespace
{
	constexpr float DecalArrowScreenScale = 0.12f;
	constexpr float DecalArrowMinScale = 0.01f;
	constexpr float DecalArrowInnerOpacity = 0.35f;
	const FVector4 DecalArrowColorTint(0.31f, 0.31f, 0.78f, 1.0f);

	float ComputeDecalArrowScale(const FRenderBus& Bus, const FVector& WorldLocation)
	{
		if (Bus.IsOrtho())
		{
			const float Scale = Bus.GetOrthoWidth() * DecalArrowScreenScale;
			return (Scale < DecalArrowMinScale) ? DecalArrowMinScale : Scale;
		}

		const float Distance = FVector::Distance(Bus.GetCameraPosition(), WorldLocation);
		const float Scale = Distance * DecalArrowScreenScale;
		return (Scale < DecalArrowMinScale) ? DecalArrowMinScale : Scale;
	}

	FMatrix BuildUnitRotationMatrix(const UMeshDecalComponent* MeshDecalComp)
	{
		FMatrix Rotation = MeshDecalComp->GetWorldMatrix();

		for (int32 Row = 0; Row < 3; ++Row)
		{
			FVector Axis(Rotation.M[Row][0], Rotation.M[Row][1], Rotation.M[Row][2]);
			if (Axis.Dot(Axis) > 1e-8f)
			{
				Axis.Normalize();
			}

			Rotation.M[Row][0] = Axis.X;
			Rotation.M[Row][1] = Axis.Y;
			Rotation.M[Row][2] = Axis.Z;
			Rotation.M[Row][3] = 0.0f;
		}

		Rotation.M[3][0] = 0.0f;
		Rotation.M[3][1] = 0.0f;
		Rotation.M[3][2] = 0.0f;
		Rotation.M[3][3] = 1.0f;
		return Rotation;
	}
}

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
		else if (const FTextureResource* Texture = MeshDecal->GetMeshDecalTexture())
		{
			Draw.DiffuseSRV = Texture->SRV;
			Draw.DiffuseColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		}

		SectionDraws.push_back(Draw);
	}

	UpdateSortKey();
	MaterialSortKey = static_cast<uint32>(MeshDecal->GetSortOrder() - INT32_MIN);
}

void FMeshDecalSceneProxy::CollectEntries(FRenderBus& Bus)
{
	FPrimitiveSceneProxy::CollectEntries(Bus);

	if (!bVisible || !bSelected)
	{
		return;
	}

	FOBBEntry Entry;
	Entry.OBB.LocalBox = FBoundingBox(FVector(-0.5f, -0.5f, -0.5f), FVector(0.5f, 0.5f, 0.5f));
	Entry.OBB.Transform = PerObjectConstants.Model;
	Entry.OBB.Color = FColor::Green();
	Bus.AddOBBEntry(std::move(Entry));
}

FMeshDecalArrowSceneProxy::FMeshDecalArrowSceneProxy(UMeshDecalComponent* InComponent, bool bInner)
	: FPrimitiveSceneProxy(InComponent)
	, bIsInner(bInner)
{
	bPerViewportUpdate = true;
	bNeverCull = true;
	bSkipGPUOcclusion = true;
	bShowAABB = false;
	bSupportsOutline = false;
	Pass = bInner ? ERenderPass::GizmoInner : ERenderPass::GizmoOuter;
}

void FMeshDecalArrowSceneProxy::UpdateMesh()
{
	MeshBuffer = &FMeshBufferManager::Get().GetMeshBuffer(EMeshShape::TransGizmo);
	Shader = FShaderManager::Get().GetShader(EShaderType::Gizmo);

	SectionDraws.clear();

	FMeshSectionDraw Draw = {};
	Draw.IndexCount = MeshBuffer->GetIndexBuffer().GetIndexCount();
	SectionDraws.push_back(Draw);

	UpdateSortKey();
}

void FMeshDecalArrowSceneProxy::UpdatePerViewport(const FRenderBus& Bus)
{
	UMeshDecalComponent* MeshDecalComp = GetMeshDecalComponent();
	FPrimitiveSceneProxy* MeshDecalProxy = MeshDecalComp ? MeshDecalComp->GetSceneProxy() : nullptr;
	if (!MeshDecalComp
		|| !MeshDecalProxy
		|| !MeshDecalComp->IsVisible()
		|| !MeshDecalProxy->bVisible
		|| !MeshDecalProxy->bSelected
		|| !MeshDecalProxy->bInVisibleSet
		|| !Bus.GetShowFlags().bGizmo
		|| !Bus.GetShowFlags().bDecal)
	{
		bVisible = false;
		return;
	}

	bVisible = true;

	const FVector WorldLocation = MeshDecalComp->GetWorldLocation();
	const float PerViewScale = ComputeDecalArrowScale(Bus, WorldLocation);
	const FMatrix RotationMatrix = BuildUnitRotationMatrix(MeshDecalComp);

	PerObjectConstants = FPerObjectConstants{
		FMatrix::MakeScaleMatrix(FVector(PerViewScale, PerViewScale, PerViewScale))
			* RotationMatrix
			* FMatrix::MakeTranslationMatrix(WorldLocation),
		DecalArrowColorTint
	};
	CachedWorldPos = WorldLocation;
	MarkPerObjectCBDirty();

	auto& G = ExtraCB.Bind<FGizmoConstants>(
		FConstantBufferPool::Get().GetBuffer(ECBSlot::Gizmo, sizeof(FGizmoConstants)),
		ECBSlot::Gizmo);
	G.ColorTint = DecalArrowColorTint;
	G.bIsInnerGizmo = bIsInner ? 1u : 0u;
	G.bClicking = 0u;
	G.SelectedAxis = 0xFFFFFFFFu;
	G.HoveredAxisOpacity = DecalArrowInnerOpacity;
	G.AxisMask = 0x1u;
	G.bOverrideAxisColor = 1u;
}

UMeshDecalComponent* FMeshDecalArrowSceneProxy::GetMeshDecalComponent() const
{
	return static_cast<UMeshDecalComponent*>(Owner);
}
