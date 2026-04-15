#include "Components/DecalComponent.h"

#include "Materials/MaterialInterface.h"
#include "Mesh/ObjManager.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"
#include "Resource/ResourceManager.h"
#include "Render/Pipeline/RenderBus.h"
#include "Render/Pipeline/RenderConstants.h"
#include "Serialization/Archive.h"
#include "Core/DecalTypes.h"
#include "Mesh/DecalMeshBuilder.h"
#include "Render/Proxy/DecalSceneProxy.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
	static constexpr const char* GDecalTextureMaterialPrefix = "Texture:";

	static const FVector GDecalLocalCorners[8] =
	{
		FVector(-0.5f, -0.5f, -0.5f),
		FVector(0.5f, -0.5f, -0.5f),
		FVector(0.5f,  0.5f, -0.5f),
		FVector(-0.5f,  0.5f, -0.5f),

		FVector(-0.5f, -0.5f,  0.5f),
		FVector(0.5f, -0.5f,  0.5f),
		FVector(0.5f,  0.5f,  0.5f),
		FVector(-0.5f,  0.5f,  0.5f),
	};

	static constexpr int32 GBoxEdges[12][2] =
	{
		{0,1}, {1,2}, {2,3}, {3,0},
		{4,5}, {5,6}, {6,7}, {7,4},
		{0,4}, {1,5}, {2,6}, {3,7}
	};

	FVector SanitizeDecalSize(const FVector& InSize)
	{
		return FVector(
			std::max(std::abs(InSize.X), 0.001f),
			std::max(std::abs(InSize.Y), 0.001f),
			std::max(std::abs(InSize.Z), 0.001f));
	}

	void AddDebugLine(FRenderBus& RenderBus, const FVector& Start, const FVector& End, const FColor& Color)
	{
		FDebugLineEntry Entry;
		Entry.Start = Start;
		Entry.End = End;
		Entry.Color = Color;
		RenderBus.AddDebugLineEntry(std::move(Entry));
	}

	FVector MultiplyComponents(const FVector& A, const FVector& B)
	{
		return FVector(A.X * B.X, A.Y * B.Y, A.Z * B.Z);
	}

	bool NearlyEqual(float A, float B, float Epsilon = 0.0001f)
	{
		return std::abs(A - B) <= Epsilon;
	}

	bool IsTextureMaterialPath(const FString& Path)
	{
		return Path.rfind(GDecalTextureMaterialPrefix, 0) == 0;
	}

	FString GetTextureNameFromMaterialPath(const FString& Path)
	{
		return IsTextureMaterialPath(Path)
			? Path.substr(std::strlen(GDecalTextureMaterialPrefix))
			: FString();
	}

	FString MakeTextureMaterialPath(const FName& TextureName)
	{
		return FString(GDecalTextureMaterialPrefix) + TextureName.ToString();
	}
}

IMPLEMENT_CLASS(UDecalComponent, UPrimitiveComponent)

FPrimitiveSceneProxy* UDecalComponent::CreateSceneProxy()
{
	if (bDecalDirty)
	{
		BuildDecalMesh();
		ClearDecalDirty();
	}

	return new FDecalSceneProxy(this);
}

void UDecalComponent::BuildDecalMesh()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		RenderableMesh.Clear();
		DebugReceiverTriangles.clear();
		DebugClippedTriangles.clear();
		return;
	}

	TArray<FDecalPrimitiveCandidate> Candidates;
	TArray<FDecalPrimitiveCandidate> SATFilteredCandidates;

	/*
		현재 decal 경로:
		1) 월드 AABB로 1차 후보 mesh 수집
		2) decal OBB vs mesh AABB SAT로 실제 겹치는 mesh만 남김
		3) 통과한 mesh 전체를 decal local 공간으로 GPU에 업로드
		4) pixel shader에서 localPos를 검사해 decal box 밖 픽셀 discard
		5) localPos(YZ)로 UV를 계산해 texture를 샘플링

		즉 SAT는 "primitive bounds 필터"에만 쓰고,
		삼각형 clip은 하지 않는 projection 스타일 경로입니다.
	*/
	FDecalMeshBuilder::GatherBroadPhaseCandidates(*this, *World, Candidates, nullptr);
	FDecalMeshBuilder::FilterPrimitiveCandidatesByDecalOBBSAT(*this, Candidates, SATFilteredCandidates);
	FDecalMeshBuilder::BuildProjectedRenderableMeshFromCandidates(*this, SATFilteredCandidates, RenderableMesh);

	DebugReceiverTriangles.clear();
	DebugClippedTriangles.clear();
}

void UDecalComponent::RebuildDecalMeshNow()
{
	BuildDecalMesh();
	ClearDecalDirty();
	MarkRenderStateDirty();
}

void UDecalComponent::EnsureDecalMeshBuilt()
{
	if (!bDecalDirty)
	{
		return;
	}

	BuildDecalMesh();
	ClearDecalDirty();
}

void UDecalComponent::OnTransformDirty()
{
	UPrimitiveComponent::OnTransformDirty();
	MarkDecalDirty();
}

void UDecalComponent::UpdateWorldAABB() const
{
	const FBoundingBox WorldBounds = GetDecalWorldAABB();

	WorldAABBMinLocation = WorldBounds.Min;
	WorldAABBMaxLocation = WorldBounds.Max;

	bWorldAABBDirty = false;
	bHasValidWorldAABB = WorldBounds.IsValid();
}

void UDecalComponent::Serialize(FArchive& Ar)
{
	UPrimitiveComponent::Serialize(Ar);

	if (Ar.IsSaving())
	{
		DecalMaterialPath = DecalMaterial ? DecalMaterial->GetAssetPathFileName() : "None";
	}

	Ar << DecalSize;
	Ar << DecalMaterialPath;
	Ar << SortOrder;
	Ar << TargetFilter;
	Ar << bDrawDebugOBB;
	Ar << bDrawDebugReceiverTriangles;
	Ar << DebugTriangleDrawLimit;

	if (Ar.IsLoading())
	{
		DecalSize = SanitizeDecalSize(DecalSize);
		ReloadMaterialFromPath();
		bDecalDirty = true;
		SyncTargetFilterOptionsFromMask();
		MarkWorldBoundsDirty();
	}
}

void UDecalComponent::PostDuplicate()
{
	UPrimitiveComponent::PostDuplicate();

	ReloadMaterialFromPath();
	bDecalDirty = true;
	SyncTargetFilterOptionsFromMask();
	MarkWorldBoundsDirty();
}

void UDecalComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPrimitiveComponent::GetEditableProperties(OutProps);

	OutProps.push_back({ "Decal Size", EPropertyType::Vec3, &DecalSize });
	OutProps.push_back({ "Decal Material", EPropertyType::MaterialRef, &DecalMaterialPath });
	OutProps.push_back({ "Sort Order", EPropertyType::Int, &SortOrder });
	OutProps.push_back({ "Target Static Mesh", EPropertyType::Bool, &bTargetStaticMeshComponent });
	OutProps.push_back({ "Target Receives Decal Only", EPropertyType::Bool, &bTargetReceivesDecalOnly });
	OutProps.push_back({ "Exclude Same Owner", EPropertyType::Bool, &bExcludeSameOwner });
	OutProps.push_back({ "Draw Debug OBB", EPropertyType::Bool, &bDrawDebugOBB });
	OutProps.push_back({ "Draw Debug Receiver Triangles", EPropertyType::Bool, &bDrawDebugReceiverTriangles });
	OutProps.push_back({ "Debug Triangle Draw Limit", EPropertyType::Int, &DebugTriangleDrawLimit });
}

void UDecalComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Decal Size") == 0)
	{
		DecalSize = SanitizeDecalSize(DecalSize);
		MarkWorldBoundsDirty();
		MarkDecalDirty();
	}
	else if (strcmp(PropertyName, "Decal Material") == 0)
	{
		ReloadMaterialFromPath();
		MarkDecalDirty();
	}
	else if (strcmp(PropertyName, "Sort Order") == 0)
	{
		MarkProxyDirty(EDirtyFlag::Material);
	}
	else if (strcmp(PropertyName, "Target Static Mesh") == 0 ||
		strcmp(PropertyName, "Target Receives Decal Only") == 0 ||
		strcmp(PropertyName, "Exclude Same Owner") == 0)
	{
		SyncTargetFilterMaskFromOptions();
		MarkDecalDirty();
	}
	else if (strcmp(PropertyName, "Debug Triangle Draw Limit") == 0)
	{
		DebugTriangleDrawLimit = std::max(DebugTriangleDrawLimit, 0);
	}
}

void UDecalComponent::CollectEditorVisualizations(FRenderBus& RenderBus) const
{
	const bool bShouldDrawOBB = bDrawDebugOBB;
	const bool bShouldDrawReceiverTriangles = bDrawDebugReceiverTriangles;
	if ((!bShouldDrawOBB && !bShouldDrawReceiverTriangles) || !IsVisible())
	{
		return;
	}

	if (bDecalDirty)
	{
		const_cast<UDecalComponent*>(this)->EnsureDecalMeshBuilt();
	}

	if (bShouldDrawOBB)
	{
		const FColor BoxColor = bDecalDirty ? FColor::Yellow() : FColor::Green();
		AddDebugOBBLines(RenderBus, BoxColor);
	}

	if (bShouldDrawReceiverTriangles)
	{
		AddDebugReceiverTriangleLines(RenderBus, FColor(96, 255, 96, 255));
		AddDebugClippedTriangleLines(RenderBus, FColor::White());
	}
}

void UDecalComponent::SetDecalSize(const FVector& InSize)
{
	const FVector NewSize = SanitizeDecalSize(InSize);

	if (NearlyEqual(DecalSize.X, NewSize.X) &&
		NearlyEqual(DecalSize.Y, NewSize.Y) &&
		NearlyEqual(DecalSize.Z, NewSize.Z))
	{
		return;
	}

	DecalSize = NewSize;
	MarkWorldBoundsDirty();
	MarkDecalDirty();
}

void UDecalComponent::SetDecalMaterial(UMaterialInterface* NewDecalMaterial)
{
	if (DecalMaterial == NewDecalMaterial)
	{
		return;
	}

	DecalMaterial = NewDecalMaterial;
	DecalMaterialPath = DecalMaterial ? DecalMaterial->GetAssetPathFileName() : "None";
	DecalTextureName = FName();

	MarkDecalDirty();
	MarkProxyDirty(EDirtyFlag::Material);
}

void UDecalComponent::SetDecalTexture(const FName& TextureName)
{
	DecalMaterial = nullptr;
	DecalTextureName = TextureName;
	DecalMaterialPath = TextureName.IsValid() ? MakeTextureMaterialPath(TextureName) : "None";

	MarkDecalDirty();
	MarkProxyDirty(EDirtyFlag::Material);
}

bool UDecalComponent::FitSizeToTextureAspect()
{
	if (!DecalTextureName.IsValid())
	{
		return false;
	}

	const FTextureResource* Texture = FResourceManager::Get().FindTexture(DecalTextureName);
	if (!Texture || Texture->Width == 0 || Texture->Height == 0)
	{
		return false;
	}

	if (DecalSize.Y <= 0.0f)
	{
		DecalSize.Y = 1.0f;
	}

	const FVector WorldScale = GetWorldMatrix().GetScale();
	const float SafeWorldScaleY = (std::abs(WorldScale.Y) > 0.001f) ? std::abs(WorldScale.Y) : 1.0f;
	const float SafeWorldScaleZ = (std::abs(WorldScale.Z) > 0.001f) ? std::abs(WorldScale.Z) : 1.0f;
	const float TextureAspectYZ = static_cast<float>(Texture->Height) / static_cast<float>(Texture->Width);

	DecalSize.Z = (DecalSize.Y * SafeWorldScaleY * TextureAspectYZ) / SafeWorldScaleZ;
	MarkWorldBoundsDirty();
	MarkDecalDirty();
	return true;
}

void UDecalComponent::SetSortOrder(int32 Value)
{
	if (SortOrder == Value)
	{
		return;
	}

	SortOrder = Value;
	MarkProxyDirty(EDirtyFlag::Material);
}

void UDecalComponent::SetTargetFilter(int32 InFilter)
{
	if (TargetFilter == InFilter)
	{
		return;
	}

	TargetFilter = InFilter;
	SyncTargetFilterOptionsFromMask();
	MarkDecalDirty();
}

void UDecalComponent::SyncTargetFilterMaskFromOptions()
{
	TargetFilter = DecalTarget_None;

	if (bTargetStaticMeshComponent)
	{
		TargetFilter |= DecalTarget_StaticMeshComponent;
	}
	if (bTargetReceivesDecalOnly)
	{
		TargetFilter |= DecalTarget_ReceivesDecalOnly;
	}
	if (bExcludeSameOwner)
	{
		TargetFilter |= DecalTarget_ExcludeSameOwner;
	}
}

void UDecalComponent::SyncTargetFilterOptionsFromMask()
{
	bTargetStaticMeshComponent = (TargetFilter & DecalTarget_StaticMeshComponent) != 0;
	bTargetReceivesDecalOnly = (TargetFilter & DecalTarget_ReceivesDecalOnly) != 0;
	bExcludeSameOwner = (TargetFilter & DecalTarget_ExcludeSameOwner) != 0;
}

void UDecalComponent::MarkDecalDirty()
{
	bDecalDirty = true;
	MarkProxyDirty(EDirtyFlag::Mesh);
}

FTransform UDecalComponent::GetTransformIncludingDecalSize() const
{
	const FMatrix WorldMatrix = GetWorldMatrix();
	const FVector CombinedScale = MultiplyComponents(WorldMatrix.GetScale(), DecalSize);

	return FTransform(
		WorldMatrix.GetLocation(),
		WorldMatrix.ToQuat(),
		CombinedScale);
}

FMatrix UDecalComponent::GetDecalLocalToWorldMatrix() const
{
	return FMatrix::MakeScaleMatrix(DecalSize) * GetWorldMatrix();
}

FMatrix UDecalComponent::GetWorldToDecalMatrix() const
{
	return GetDecalLocalToWorldMatrix().GetInverse();
}

void UDecalComponent::GetDecalBoxCorners(FVector(&OutCorners)[8]) const
{
	const FMatrix DecalLocalToWorld = GetDecalLocalToWorldMatrix();

	for (int32 i = 0; i < 8; ++i)
	{
		OutCorners[i] = DecalLocalToWorld.TransformPositionWithW(GDecalLocalCorners[i]);
	}
}

FBoundingBox UDecalComponent::GetDecalWorldAABB() const
{
	FVector Corners[8];
	GetDecalBoxCorners(Corners);

	FBoundingBox Bounds;
	for (int32 i = 0; i < 8; ++i)
	{
		Bounds.Expand(Corners[i]);
	}

	return Bounds;
}

void UDecalComponent::SetFadeOut(float StartDelay, float Duration, bool DestroyOwnerAfterFade)
{
	(void)StartDelay;
	(void)Duration;
	(void)DestroyOwnerAfterFade;
}

void UDecalComponent::ReloadMaterialFromPath()
{
	if (DecalMaterialPath.empty() || DecalMaterialPath == "None")
	{
		DecalMaterial = nullptr;
		DecalTextureName = FName();
		return;
	}

	if (IsTextureMaterialPath(DecalMaterialPath))
	{
		DecalMaterial = nullptr;
		DecalTextureName = FName(GetTextureNameFromMaterialPath(DecalMaterialPath));
		return;
	}

	DecalMaterial = FObjManager::GetOrLoadMaterial(DecalMaterialPath);
	DecalTextureName = FName();
}

void UDecalComponent::AddDebugOBBLines(FRenderBus& RenderBus, const FColor& BoxColor) const
{
	FVector Corners[8];
	GetDecalBoxCorners(Corners);

	for (int32 EdgeIndex = 0; EdgeIndex < 12; ++EdgeIndex)
	{
		const int32 A = GBoxEdges[EdgeIndex][0];
		const int32 B = GBoxEdges[EdgeIndex][1];
		AddDebugLine(RenderBus, Corners[A], Corners[B], BoxColor);
	}

	const FMatrix DecalLocalToWorld = GetDecalLocalToWorldMatrix();
	const FVector Center = DecalLocalToWorld.TransformPositionWithW(FVector(0.0f, 0.0f, 0.0f));
	const FVector ForwardTip = DecalLocalToWorld.TransformPositionWithW(FVector(0.5f, 0.0f, 0.0f));
	AddDebugLine(RenderBus, Center, ForwardTip, FColor::Red());
}

void UDecalComponent::AddDebugReceiverTriangleLines(FRenderBus& RenderBus, const FColor& TriangleColor) const
{
	if (DebugReceiverTriangles.empty() || DebugTriangleDrawLimit <= 0)
	{
		return;
	}

	const FMatrix DecalLocalToWorld = GetDecalLocalToWorldMatrix();
	const int32 TriangleCountToDraw = std::min<int32>(static_cast<int32>(DebugReceiverTriangles.size()), DebugTriangleDrawLimit);

	for (int32 TriangleIndex = 0; TriangleIndex < TriangleCountToDraw; ++TriangleIndex)
	{
		const FDecalSATTriangle& Triangle = DebugReceiverTriangles[TriangleIndex];

		const FVector WorldP0 = DecalLocalToWorld.TransformPositionWithW(Triangle.DecalPositions[0]);
		const FVector WorldP1 = DecalLocalToWorld.TransformPositionWithW(Triangle.DecalPositions[1]);
		const FVector WorldP2 = DecalLocalToWorld.TransformPositionWithW(Triangle.DecalPositions[2]);

		AddDebugLine(RenderBus, WorldP0, WorldP1, TriangleColor);
		AddDebugLine(RenderBus, WorldP1, WorldP2, TriangleColor);
		AddDebugLine(RenderBus, WorldP2, WorldP0, TriangleColor);
	}
}

void UDecalComponent::AddDebugClippedTriangleLines(FRenderBus& RenderBus, const FColor& TriangleColor) const
{
	if (DebugClippedTriangles.empty() || DebugTriangleDrawLimit <= 0)
	{
		return;
	}

	const FMatrix DecalLocalToWorld = GetDecalLocalToWorldMatrix();
	const int32 TriangleCountToDraw = std::min<int32>(static_cast<int32>(DebugClippedTriangles.size()), DebugTriangleDrawLimit);

	for (int32 TriangleIndex = 0; TriangleIndex < TriangleCountToDraw; ++TriangleIndex)
	{
		const FDecalTriangulatedTriangle& Triangle = DebugClippedTriangles[TriangleIndex];

		const FVector WorldP0 = DecalLocalToWorld.TransformPositionWithW(Triangle.DecalPositions[0]);
		const FVector WorldP1 = DecalLocalToWorld.TransformPositionWithW(Triangle.DecalPositions[1]);
		const FVector WorldP2 = DecalLocalToWorld.TransformPositionWithW(Triangle.DecalPositions[2]);

		AddDebugLine(RenderBus, WorldP0, WorldP1, TriangleColor);
		AddDebugLine(RenderBus, WorldP1, WorldP2, TriangleColor);
		AddDebugLine(RenderBus, WorldP2, WorldP0, TriangleColor);
	}
}

void UDecalComponent::DebugRunBroadPhase() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<FDecalPrimitiveCandidate> Candidates;
	FDecalBroadPhaseStats Stats;
	FDecalMeshBuilder::GatherBroadPhaseCandidates(*this, *World, Candidates, &Stats);
}
