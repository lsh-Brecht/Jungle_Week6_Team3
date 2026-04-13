#include "Component/DecalComponent.h"

#include "Materials/MaterialInterface.h"
#include "Mesh/ObjManager.h"
#include "Object/ObjectFactory.h"
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
	/*
		canonical decal box를 "decal local space"에서 정의한다.

		왜 [-0.5, +0.5] 범위를 쓰나?
		- DecalSize를 곱하면 곧바로 실제 박스가 된다.
		- 중심이 원점이라 SAT / clipping / UV 계산이 단순해진다.
		- later stage에서 "decal local 기준"을 유지하기 쉽다.
	*/
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

	/*
		Size는 음수가 되면 의미가 꼬이므로 최소 양수로 보정한다.
		broad phase / SAT / clipping에서 "음수 축 길이"는 디버깅을 매우 어렵게 만든다.
	*/
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
		return;
	}

	TArray<FDecalPrimitiveCandidate> Candidates;
	TArray<FDecalSourceTriangle> SourceTriangles;
	TArray<FDecalLocalTriangle> LocalTriangles;
	TArray<FDecalCoarseOverlapTriangle> CoarseTriangles;
	TArray<FDecalSATTriangle> SATTriangles;
	TArray<FDecalClippedPolygon> ClippedPolygons;
	TArray<FDecalTriangulatedTriangle> Triangles;
	TArray<FDecalUVTriangle> UVTriangles;

	FDecalMeshBuilder::GatherBroadPhaseCandidates(*this, *World, Candidates, nullptr);
	FDecalMeshBuilder::GatherBruteForceTriangles(Candidates, SourceTriangles, nullptr);
	FDecalMeshBuilder::TransformTrianglesToDecalLocal(*this, SourceTriangles, LocalTriangles, nullptr);
	FDecalMeshBuilder::GatherCoarseOverlapTriangles(LocalTriangles, CoarseTriangles, nullptr);
	FDecalMeshBuilder::GatherSATOverlapTriangles(CoarseTriangles, SATTriangles, nullptr);
	FDecalMeshBuilder::ClipSATTrianglesAgainstDecalBox(SATTriangles, ClippedPolygons, nullptr);
	FDecalMeshBuilder::TriangulateClippedPolygons(ClippedPolygons, Triangles, nullptr);
	FDecalMeshBuilder::ComputeTriangleUVs(Triangles, UVTriangles, nullptr);
	FDecalMeshBuilder::BuildRenderableMesh(UVTriangles, RenderableMesh, nullptr);
}

void UDecalComponent::RebuildDecalMeshNow()
{
	BuildDecalMesh();
	ClearDecalDirty();
	MarkRenderStateDirty();
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

	if (Ar.IsLoading())
	{
		DecalSize = SanitizeDecalSize(DecalSize);
        ReloadMaterialFromPath();

        /*
            로드 직후에는 broad phase / build result가 모두 stale일 수 있으므로
            다시 빌드되도록 Dirty를 세운다.
        */
		bDecalDirty = true;
	
		MarkWorldBoundsDirty();
	}
}

void UDecalComponent::PostDuplicate()
{
	UPrimitiveComponent::PostDuplicate();

	// ??????
	ReloadMaterialFromPath();
	bDecalDirty = true;
	MarkWorldBoundsDirty();
}

void UDecalComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPrimitiveComponent::GetEditableProperties(OutProps);

	OutProps.push_back({ "Decal Size", EPropertyType::Vec3, &DecalSize });
	OutProps.push_back({ "Decal Material", EPropertyType::MaterialRef, &DecalMaterialPath });
	OutProps.push_back({ "Sort Order", EPropertyType::Int, &SortOrder });
	OutProps.push_back({ "Target Filter", EPropertyType::Int, &TargetFilter });
	OutProps.push_back({ "Draw Debug OBB", EPropertyType::Bool, &bDrawDebugOBB });
}

void UDecalComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Decal Size") == 0)
	{
		SetDecalSize(DecalSize);
		RebuildDecalMeshNow();
	}
	else if (strcmp(PropertyName, "Decal Material") == 0)
	{
		ReloadMaterialFromPath();
		MarkDecalDirty();
		RebuildDecalMeshNow();
	}
	else if (strcmp(PropertyName, "Sort Order") == 0)
	{
		SetSortOrder(SortOrder);
		RebuildDecalMeshNow();
	}
	else if (strcmp(PropertyName, "Target Filter") == 0)
	{
		SetTargetFilter(TargetFilter);
		RebuildDecalMeshNow();
	}
	else if (strcmp(PropertyName, "Draw Debug OBB") == 0)
	{
		// ?????
	}
}

void UDecalComponent::CollectEditorVisualizations(FRenderBus& RenderBus) const
{
	if (!bDrawDebugOBB || !IsVisible()) return;

	const FColor BoxColor = bDecalDirty ? FColor::Yellow() : FColor::Green();
	AddDebugOBBLines(RenderBus, BoxColor);
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

	/*
	* Size 바뀌면
	* 1. decal obb 달라지고
	* 2. 월드 aabb 바뀌고
	* 3. broad phase 결과도 바뀌고
	* 4. triangle clipping 결과도 바뀜
	* 
	* 따라서 bounds dirty + decal dirty 둘 다 세워야 함
	*/
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

	/*
		지금은 단일 Dirty로 통합 관리한다.
		이후 단계에서 render proxy가 생기면 Material dirty를 분리해도 된다.
	*/
	MarkDecalDirty();
	MarkProxyDirty(EDirtyFlag::Material);
}

void UDecalComponent::SetSortOrder(int32 Value)
{
	if (SortOrder == Value)
	{
		return;
	}

	SortOrder = Value;

	/*
		SortOrder는 geometry를 바꾸지는 않지만,
		최종 draw 순서를 바꾸는 데 중요한 값이다.

		현재는 단계 초기라 Dirty를 단순화해서 같은 플래그로 처리한다.
	*/
	MarkDecalDirty();
}

void UDecalComponent::SetTargetFilter(int32 InFilter)
{
	if (TargetFilter == InFilter)
	{
		return;
	}

	TargetFilter = InFilter;

	/*
		broad phase의 후보 primitive 집합이 달라지므로
		이후 build 결과 전체가 달라질 수 있다.
	*/
	MarkDecalDirty();
}

void UDecalComponent::MarkDecalDirty()
{
	bDecalDirty = true;

	/*
	* decal mesh가 SceneProxy에 올라간 뒤에는
	* 'geometry rebuild가 필요하다'는 신호가 프록시 쪽에도 전파되어야 함.
	*/
	MarkProxyDirty(EDirtyFlag::Mesh);
}

FTransform UDecalComponent::GetTransformIncludingDecalSize() const
{
	/*
		기존 헤더의 의도를 살린 함수.

		다만 실제 후속 단계에서는 FMatrix 쪽이 더 많이 쓰일 가능성이 크다.
		SAT / clipping / world<->decal transform은 Matrix 기반이 더 직접적이기 때문이다.
	*/
	const FMatrix WorldMatrix = GetWorldMatrix();
	const FVector CombinedScale = MultiplyComponents(WorldMatrix.GetScale(), DecalSize);

	return FTransform(
		WorldMatrix.GetLocation(),
		WorldMatrix.ToQuat(),
		CombinedScale);
}

FMatrix UDecalComponent::GetDecalLocalToWorldMatrix() const
{
	/*
		decal local의 canonical unit box([-0.5, 0.5])를
		실제 decal box로 만드는 행렬이다.

		순서가 중요한데,
		local box에 DecalSize를 먼저 적용하고,
		그 다음 component의 world transform을 적용해야 한다.

		row-vector 기준이므로:
			Local -> Scale(DecalSize) -> ComponentWorld
		즉,
			DecalLocalToWorld = Scale(DecalSize) * GetWorldMatrix()
	*/
	return FMatrix::MakeScaleMatrix(DecalSize) * GetWorldMatrix();
}

FMatrix UDecalComponent::GetWorldToDecalMatrix() const
{
	/*
		broad phase 이후 단계에서 triangle을 decal local로 보내는 데 바로 쓸 수 있는 행렬.
		non-uniform scale까지 들어갈 수 있으므로 GetInverseFast()가 아니라 GetInverse()를 쓴다.
	*/
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
	/*
		지금 단계에서는 페이드가 목적이 아니므로 의도적으로 no-op로 둔다.

		인터페이스를 남겨두는 이유:
		- 기존 헤더와 호출부 호환성 유지
		- 나중에 decal lifetime/fade 기능을 붙일 자리를 예약

		매개변수 unused 경고를 막기 위해 캐스팅만 해둔다.
	*/
	(void)StartDelay;
	(void)Duration;
	(void)DestroyOwnerAfterFade;
}

void UDecalComponent::ReloadMaterialFromPath()
{
	if (DecalMaterialPath.empty() || DecalMaterialPath == "None")
	{
		DecalMaterial = nullptr;
		return;
	}

	/*
		StaticMeshComponent와 같은 패턴:
		경로 문자열로부터 실제 runtime material 포인터를 복원한다.
	*/
	DecalMaterial = FObjManager::GetOrLoadMaterial(DecalMaterialPath);
}

void UDecalComponent::AddDebugOBBLines(FRenderBus& RenderBus, const FColor& BoxColor) const
{
	FVector Corners[8];
	GetDecalBoxCorners(Corners);

	/*
		박스 12에지.
		이 wireframe만 봐도
		- 위치
		- 회전
		- 크기
		- 비균등 scale 적용 상태
		를 한 번에 검증할 수 있다.
	*/
	for (int32 EdgeIndex = 0; EdgeIndex < 12; ++EdgeIndex)
	{
		const int32 A = GBoxEdges[EdgeIndex][0];
		const int32 B = GBoxEdges[EdgeIndex][1];
		AddDebugLine(RenderBus, Corners[A], Corners[B], BoxColor);
	}

	/*
		+X를 빨간 선으로 따로 그려준다.

		왜 필요한가?
		decal은 "박스"만 맞아도 절반은 성공이지만,
		실제로는 "어느 쪽이 투영 방향인가"를 헷갈리기 쉽다.
		forward 축을 따로 그려두면
		이후 triangle을 decal local로 보낼 때 축 해석이 맞는지 빠르게 검증할 수 있다.
	*/
	const FMatrix DecalLocalToWorld = GetDecalLocalToWorldMatrix();
	const FVector Center = DecalLocalToWorld.TransformPositionWithW(FVector(0.0f, 0.0f, 0.0f));
	const FVector ForwardTip = DecalLocalToWorld.TransformPositionWithW(FVector(0.5f, 0.0f, 0.0f));
	AddDebugLine(RenderBus, Center, ForwardTip, FColor::Red());
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

	/*
		여기서 디버깅 포인트:
		- TotalPrimitiveCount
		- StaticMeshPrimitiveCount
		- FilterPassedCount
		- BoundsOverlapCount
		- Candidates.size()

		처음엔 이 숫자들이 기대와 맞는지 보는 게 가장 중요합니다.
	*/
}
