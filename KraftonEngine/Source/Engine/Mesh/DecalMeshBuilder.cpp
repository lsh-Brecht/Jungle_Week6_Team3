#include "Mesh/DecalMeshBuilder.h"

#include "Component/DecalComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Mesh/StaticMesh.h"
#include "Mesh/StaticMeshAsset.h"
#include "Object/Object.h"

namespace
{
	FVector ComputeTriangleNormal(const FVector& A, const FVector& B, const FVector& C)
	{
		const FVector Edge01 = B - A;
		const FVector Edge02 = C - A;

		FVector N = Edge01.Cross(Edge02);
		const float Len = N.Length();
		if (Len > 0.000001f)
		{
			N /= Len;
		}
		return N;
	}

	FBoundingBox MakeTriangleAABB(const FVector& A, const FVector& B, const FVector& C)
	{
		FBoundingBox Bounds;
		Bounds.Expand(A);
		Bounds.Expand(B);
		Bounds.Expand(C);
		return Bounds;
	}

	FBoundingBox GetCanonicalDecalBoxBounds()
	{
		return FBoundingBox(
			FVector(-0.5f, -0.5f, -0.5f),
			FVector(0.5f, 0.5f, 0.5f));
	}

	bool IsPointInsideCanonicalDecalBox(const FVector& P)
	{
		return
			P.X >= -0.5f && P.X <= 0.5f &&
			P.Y >= -0.5f && P.Y <= 0.5f &&
			P.Z >= -0.5f && P.Z <= 0.5f;
	}

	float GetAxisComponent(const FVector& V, int32 Axis)
	{
		return Axis == 0 ? V.X : (Axis == 1 ? V.Y : V.Z);
	}

	float ComputeBoxProjectionRadius(const FVector& Axis, const FVector& HalfExtent)
	{
		return
			std::abs(Axis.X) * HalfExtent.X +
			std::abs(Axis.Y) * HalfExtent.Y +
			std::abs(Axis.Z) * HalfExtent.Z;
	}

	void ProjectTriangleOntoAxis(
		const FVector& Axis,
		const FVector& P0,
		const FVector& P1,
		const FVector& P2,
		float& OutMin,
		float& OutMax)
	{
		const float D0 = P0.Dot(Axis);
		const float D1 = P1.Dot(Axis);
		const float D2 = P2.Dot(Axis);

		OutMin = (std::min)(D0, (std::min)(D1, D2));
		OutMax = (std::max)(D0, (std::max)(D1, D2));
	}

	bool IntersectsOnAxis(
		const FVector& Axis,
		const FVector& P0,
		const FVector& P1,
		const FVector& P2,
		const FVector& HalfExtent)
	{
		/*
			cross axis 중에는 길이가 0에 가까운 축이 나올 수 있습니다.
			그런 축은 separating axis로 의미가 없으므로 통과 처리합니다.
		*/
		const float AxisLenSq = Axis.Dot(Axis);
		if (AxisLenSq <= 0.0000001f)
		{
			return true;
		}

		float TriMin = 0.0f;
		float TriMax = 0.0f;
		ProjectTriangleOntoAxis(Axis, P0, P1, P2, TriMin, TriMax);

		const float BoxRadius = ComputeBoxProjectionRadius(Axis, HalfExtent);

		return !(TriMax < -BoxRadius || TriMin > BoxRadius);
	}

	bool IntersectsOnBoxAxes(
		const FVector& P0,
		const FVector& P1,
		const FVector& P2,
		const FVector& HalfExtent)
	{
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			const float A0 = GetAxisComponent(P0, Axis);
			const float A1 = GetAxisComponent(P1, Axis);
			const float A2 = GetAxisComponent(P2, Axis);

			const float TriMin = (std::min)(A0, (std::min)(A1, A2));
			const float TriMax = (std::max)(A0, (std::max)(A1, A2));
			const float Ext = GetAxisComponent(HalfExtent, Axis);

			if (TriMax < -Ext || TriMin > Ext)
			{
				return false;
			}
		}

		return true;
	}

	bool IntersectsOnTriangleNormal(
		const FVector& P0,
		const FVector& P1,
		const FVector& P2,
		const FVector& HalfExtent)
	{
		const FVector E0 = P1 - P0;
		const FVector E1 = P2 - P0;
		const FVector TriangleNormal = E0.Cross(E1);

		return IntersectsOnAxis(TriangleNormal, P0, P1, P2, HalfExtent);
	}

	bool IntersectsOnEdgeCrossAxes(
		const FVector& P0,
		const FVector& P1,
		const FVector& P2,
		const FVector& HalfExtent)
	{
		const FVector E0 = P1 - P0;
		const FVector E1 = P2 - P1;
		const FVector E2 = P0 - P2;

		const FVector BoxAxes[3] =
		{
			FVector(1.0f, 0.0f, 0.0f),
			FVector(0.0f, 1.0f, 0.0f),
			FVector(0.0f, 0.0f, 1.0f)
		};

		const FVector Edges[3] = { E0, E1, E2 };

		for (int32 EdgeIndex = 0; EdgeIndex < 3; ++EdgeIndex)
		{
			for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
			{
				const FVector TestAxis = Edges[EdgeIndex].Cross(BoxAxes[AxisIndex]);

				if (!IntersectsOnAxis(TestAxis, P0, P1, P2, HalfExtent))
				{
					return false;
				}
			}
		}

		return true;
	}

	bool NearlyEqualFloat(float A, float B, float Epsilon = 0.00001f)
	{
		return std::abs(A - B) <= Epsilon;
	}

	void SetAxisComponent(FVector& V, int32 Axis, float Value)
	{
		if (Axis == 0) V.X = Value;
		else if (Axis == 1) V.Y = Value;
		else V.Z = Value;
	}

	bool IsInsidePlane(const FVector& P, int32 Axis, float PlaneValue, bool bKeepLessEqual)
	{
		const float Value = GetAxisComponent(P, Axis);
		return bKeepLessEqual ? (Value <= PlaneValue) : (Value >= PlaneValue);
	}

	FVector IntersectSegmentWithAxisPlane(
		const FVector& A,
		const FVector& B,
		int32 Axis,
		float PlaneValue)
	{
		const float AV = GetAxisComponent(A, Axis);
		const float BV = GetAxisComponent(B, Axis);
		const float Denom = (BV - AV);

		// 거의 평행한 경우는 A를 기반으로 plane 위로 보정
		if (std::abs(Denom) <= 0.000001f)
		{
			FVector Result = A;
			SetAxisComponent(Result, Axis, PlaneValue);
			return Result;
		}

		const float T = (PlaneValue - AV) / Denom;
		return A + (B - A) * T;
	}

	void ClipPolygonAgainstAxisPlane(
		const TArray<FVector>& InVertices,
		TArray<FVector>& OutVertices,
		int32 Axis,
		float PlaneValue,
		bool bKeepLessEqual)
	{
		OutVertices.clear();

		if (InVertices.empty())
		{
			return;
		}

		const size_t VertexCount = InVertices.size();

		for (size_t i = 0; i < VertexCount; ++i)
		{
			const FVector& Current = InVertices[i];
			const FVector& Prev = InVertices[(i + VertexCount - 1) % VertexCount];

			const bool bCurrentInside = IsInsidePlane(Current, Axis, PlaneValue, bKeepLessEqual);
			const bool bPrevInside = IsInsidePlane(Prev, Axis, PlaneValue, bKeepLessEqual);

			if (bPrevInside && bCurrentInside)
			{
				// inside -> inside
				OutVertices.push_back(Current);
			}
			else if (bPrevInside && !bCurrentInside)
			{
				// inside -> outside
				OutVertices.push_back(IntersectSegmentWithAxisPlane(Prev, Current, Axis, PlaneValue));
			}
			else if (!bPrevInside && bCurrentInside)
			{
				// outside -> inside
				OutVertices.push_back(IntersectSegmentWithAxisPlane(Prev, Current, Axis, PlaneValue));
				OutVertices.push_back(Current);
			}
			else
			{
				// outside -> outside
			}
		}
	}

	float ComputePolygonAreaEstimate(const TArray<FVector>& Vertices, const FVector& FaceNormal)
	{
		if (Vertices.size() < 3)
		{
			return 0.0f;
		}

		FVector Accum(0.0f, 0.0f, 0.0f);
		for (size_t i = 0; i < Vertices.size(); ++i)
		{
			const FVector& A = Vertices[i];
			const FVector& B = Vertices[(i + 1) % Vertices.size()];
			Accum += A.Cross(B);
		}

		return std::abs(Accum.Dot(FaceNormal)) * 0.5f;
	}

	void RemoveNearDuplicateVertices(TArray<FVector>& Vertices, float Epsilon = 0.00001f)
	{
		if (Vertices.size() <= 1)
		{
			return;
		}

		TArray<FVector> Cleaned;
		Cleaned.reserve(Vertices.size());

		for (const FVector& V : Vertices)
		{
			bool bDuplicate = false;
			for (const FVector& Existing : Cleaned)
			{
				const FVector Delta = V - Existing;
				if (Delta.Dot(Delta) <= (Epsilon * Epsilon))
				{
					bDuplicate = true;
					break;
				}
			}

			if (!bDuplicate)
			{
				Cleaned.push_back(V);
			}
		}

		// 첫/끝 정점이 사실상 같은 경우도 제거
		if (Cleaned.size() >= 2)
		{
			const FVector Delta = Cleaned.front() - Cleaned.back();
			if (Delta.Dot(Delta) <= (Epsilon * Epsilon))
			{
				Cleaned.pop_back();
			}
		}

		Vertices = std::move(Cleaned);
	}

	float Clamp01(float Value)
	{
		if (Value < 0.0f) return 0.0f;
		if (Value > 1.0f) return 1.0f;
		return Value;
	}

	FVector2 ComputeDecalUVFromLocalPosition(const FVector& DecalLocalPosition)
	{
		/*
			decal local 기준:
			X = projection depth
			Y = horizontal
			Z = vertical

			UV는 YZ를 사용합니다.
		*/
		const float U = DecalLocalPosition.Y + 0.5f;
		const float V = 0.5f - DecalLocalPosition.Z;

		return FVector2(Clamp01(U), Clamp01(V));
	}
}

bool FDecalMeshBuilder::PassTargetFilter(
	const UDecalComponent& DecalComponent,
	const UStaticMeshComponent* StaticMeshComponent)
{
	if (!StaticMeshComponent) return false;

	const int32 Filter = DecalComponent.GetTargetFilter();

	if ((Filter & DecalTarget_StaticMeshComponent) != 0)
	{
		return true;
	}

	return false;
}

void FDecalMeshBuilder::GatherBroadPhaseCandidates(
	const UDecalComponent& DecalComponent,
	const UWorld& World,
	TArray<FDecalPrimitiveCandidate>& OutCandidates,
	FDecalBroadPhaseStats* OutStats)
{
	OutCandidates.clear();

	FDecalBroadPhaseStats LocalStats;

	const FBoundingBox DecalWorldAABB = DecalComponent.GetDecalWorldAABB();

	for (AActor* Actor : World.GetActors())
	{
		if (!Actor || !Actor->IsVisible())
		{
			continue;
		}

		for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
		{
			++LocalStats.TotalPrimitiveCount;

			if (!Primitive || !Primitive->IsVisible())
			{
				continue;
			}

			++LocalStats.VisiblePrimitiveCount;

			if (Primitive == &DecalComponent)
			{
				continue;
			}

			UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Primitive);
			if (!StaticMeshComponent)
			{
				continue;
			}

			++LocalStats.StaticMeshPrimitiveCount;

			if (!PassTargetFilter(DecalComponent, StaticMeshComponent))
			{
				continue;
			}

			++LocalStats.TargetFilterPassedCount;

			UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
			if (!StaticMesh)
			{
				continue;
			}

			FStaticMesh* MeshAsset = StaticMesh->GetStaticMeshAsset();
			if (!MeshAsset || MeshAsset->Vertices.empty() || MeshAsset->Indices.size() < 3)
			{
				continue;
			}

			++LocalStats.MeshReadyCount;

			const FBoundingBox PrimitiveWorldAABB = StaticMeshComponent->GetWorldBoundingBox();
			if (!PrimitiveWorldAABB.IsValid())
			{
				continue;
			}

			if (!DecalWorldAABB.IsIntersected(PrimitiveWorldAABB))
			{
				continue;
			}

			++LocalStats.BoundsOverlapCount;

			FDecalPrimitiveCandidate Candidate;
			Candidate.OwnerActor = Actor;
			Candidate.StaticMeshComponent = StaticMeshComponent;
			Candidate.StaticMesh = StaticMesh;
			Candidate.MeshAsset = MeshAsset;
			Candidate.PrimitiveWorldAABB = PrimitiveWorldAABB;
			Candidate.MeshToWorld = StaticMeshComponent->GetWorldMatrix();
			Candidate.WorldToMesh = StaticMeshComponent->GetWorldInverseMatrix();

			OutCandidates.push_back(Candidate);
		}
	}

	if (OutStats)
	{
		*OutStats = LocalStats;
	}
}

void FDecalMeshBuilder::GatherBruteForceTriangles(
	const TArray<FDecalPrimitiveCandidate>& Candidates,
	TArray<FDecalSourceTriangle>& OutTriangles,
	FDecalTriangleGatherStats* OutStats)
{
	OutTriangles.clear();

	FDecalTriangleGatherStats LocalStats;
	LocalStats.CandidatePrimitiveCount = static_cast<int32>(Candidates.size());

	for (const FDecalPrimitiveCandidate& Candidate : Candidates)
	{
		if(!Candidate.MeshAsset)
		{
			continue;
		}

		const TArray<FNormalVertex>& Vertices = Candidate.MeshAsset->Vertices;
		const TArray<uint32>& Indices = Candidate.MeshAsset->Indices;

		if (Vertices.empty() || Indices.size() < 3)
		{
			continue;
		}

		for (size_t Index = 0; Index + 2 < Indices.size(); Index += 3)
		{
			const uint32 I0 = Indices[Index + 0];
			const uint32 I1 = Indices[Index + 1];
			const uint32 I2 = Indices[Index + 2];
		
			if (I0 >= Vertices.size() || I1 >= Vertices.size() || I2 >= Vertices.size())
			{
				++LocalStats.SkippedInvalidTriangleCount;
				continue;
			}

			FDecalSourceTriangle Triangle;
			Triangle.OwnerActor = Candidate.OwnerActor;
			Triangle.StaticMeshComponent = Candidate.StaticMeshComponent;
			Triangle.TriangleStartIndex = static_cast<int32>(Index);

			Triangle.Indices[0] = I0;
			Triangle.Indices[1] = I1;
			Triangle.Indices[2] = I2;
		
			Triangle.LocalPositions[0] = Vertices[I0].pos;
			Triangle.LocalPositions[1] = Vertices[I1].pos;
			Triangle.LocalPositions[2] = Vertices[I2].pos;
		
			Triangle.LocalNormals[0] = Vertices[I0].normal;
			Triangle.LocalNormals[1] = Vertices[I1].normal;
			Triangle.LocalNormals[2] = Vertices[I2].normal;

			Triangle.MeshToWorld = Candidate.MeshToWorld;
			Triangle.WorldToMesh = Candidate.WorldToMesh;

			OutTriangles.push_back(Triangle);
			++LocalStats.GatheredTriangleCount;
		}
	}

	if (OutStats)
	{
		*OutStats = LocalStats;
	}
}

void FDecalMeshBuilder::TransformTrianglesToDecalLocal(
	const UDecalComponent& DecalComponent,
	const TArray<FDecalSourceTriangle>& SourceTriangles,
	TArray<FDecalLocalTriangle>& OutLocalTriangles,
	FDecalTransformStats* OutStats)
{
	OutLocalTriangles.clear();

	FDecalTransformStats LocalStats;
	LocalStats.SourceTriangleCount = static_cast<int32>(SourceTriangles.size());

	const FMatrix WorldToDecal = DecalComponent.GetWorldToDecalMatrix();

	for (const FDecalSourceTriangle& SourceTriangle : SourceTriangles)
	{
		const FMatrix MeshToDecal = SourceTriangle.MeshToWorld * WorldToDecal;

		FDecalLocalTriangle LocalTriangle;
		LocalTriangle.OwnerActor = SourceTriangle.OwnerActor;
		LocalTriangle.StaticMeshComponent = SourceTriangle.StaticMeshComponent;
		LocalTriangle.TriangleStartIndex = SourceTriangle.TriangleStartIndex;
		LocalTriangle.MeshToWorld = SourceTriangle.MeshToWorld;
		LocalTriangle.WorldToMesh = SourceTriangle.WorldToMesh;
		LocalTriangle.MeshToDecal = MeshToDecal;

		LocalTriangle.DecalPositions[0] = MeshToDecal.TransformPositionWithW(SourceTriangle.LocalPositions[0]);
		LocalTriangle.DecalPositions[1] = MeshToDecal.TransformPositionWithW(SourceTriangle.LocalPositions[1]);
		LocalTriangle.DecalPositions[2] = MeshToDecal.TransformPositionWithW(SourceTriangle.LocalPositions[2]);

		LocalTriangle.DecalFaceNormal = ComputeTriangleNormal(
			LocalTriangle.DecalPositions[0],
			LocalTriangle.DecalPositions[1],
			LocalTriangle.DecalPositions[2]);

		OutLocalTriangles.push_back(LocalTriangle);
		++LocalStats.TransformedTriangleCount;
	}

	if (OutStats)
	{
		*OutStats = LocalStats;
	}
}

void FDecalMeshBuilder::GatherCoarseOverlapTriangles(
	const TArray<FDecalLocalTriangle>& LocalTriangles,
	TArray<FDecalCoarseOverlapTriangle>& OutAcceptedTriangles,
	FDecalCoarseOverlapStats* OutStats)
{
	OutAcceptedTriangles.clear();

	FDecalCoarseOverlapStats LocalStats;
	LocalStats.InputTriangleCount = static_cast<int32>(LocalTriangles.size());

	const FBoundingBox DecalBoxBounds = GetCanonicalDecalBoxBounds();

	for (const FDecalLocalTriangle& LocalTriangle : LocalTriangles)
	{
		const FVector& P0 = LocalTriangle.DecalPositions[0];
		const FVector& P1 = LocalTriangle.DecalPositions[1];
		const FVector& P2 = LocalTriangle.DecalPositions[2];

		const FBoundingBox TriangleAABB = MakeTriangleAABB(P0, P1, P2);

		if (!TriangleAABB.IsIntersected(DecalBoxBounds))
		{
			continue;
		}

		++LocalStats.TriangleAABBOverlapCount;

		const bool bInside0 = IsPointInsideCanonicalDecalBox(P0);
		const bool bInside1 = IsPointInsideCanonicalDecalBox(P1);
		const bool bInside2 = IsPointInsideCanonicalDecalBox(P2);
	
		const bool bAnyVertexInside = bInside0 || bInside1 || bInside2;
		if (bAnyVertexInside)
		{
			++LocalStats.AnyVertexInsideCount;
		}

		/*
			임시 coarse 규칙:
			- triangle AABB가 decal box와 겹치면 일단 근처로 본다.
			- vertex 하나라도 box 안이면 더 강한 힌트.
			- 지금 단계에서는 둘 중 하나만 만족해도 accept.

			즉, SAT 전이므로 false positive를 허용한다.
		*/
		FDecalCoarseOverlapTriangle Accepted;
		Accepted.OwnerActor = LocalTriangle.OwnerActor;
		Accepted.StaticMeshComponent = LocalTriangle.StaticMeshComponent;
		Accepted.TriangleStartIndex = LocalTriangle.TriangleStartIndex;
		Accepted.MeshToWorld = LocalTriangle.MeshToWorld;
		Accepted.WorldToMesh = LocalTriangle.WorldToMesh;
		Accepted.MeshToDecal = LocalTriangle.MeshToDecal;
		Accepted.DecalPositions[0] = P0;
		Accepted.DecalPositions[1] = P1;
		Accepted.DecalPositions[2] = P2;
		Accepted.DecalFaceNormal = LocalTriangle.DecalFaceNormal;
		Accepted.DecalLocalTriangleAABB = TriangleAABB;
		Accepted.bAnyVertexInsideBox = bAnyVertexInside;

		OutAcceptedTriangles.push_back(Accepted);
		++LocalStats.CoarseAcceptedCount;
	}

	if (OutStats)
	{
		*OutStats = LocalStats;
	}
}	

void FDecalMeshBuilder::GatherSATOverlapTriangles(
	const TArray<FDecalCoarseOverlapTriangle>& CoarseTriangles,
	TArray<FDecalSATTriangle>& OutSATTriangles,
	FDecalSATStats* OutStats)
{
	OutSATTriangles.clear();

	FDecalSATStats LocalStats;
	LocalStats.InputTriangleCount = static_cast<int32>(CoarseTriangles.size());

	const FVector CanonicalHalfExtent(0.5f, 0.5f, 0.5f);

	for (const FDecalCoarseOverlapTriangle& CoarseTriangle : CoarseTriangles)
	{
		const FVector& P0 = CoarseTriangle.DecalPositions[0];
		const FVector& P1 = CoarseTriangle.DecalPositions[1];
		const FVector& P2 = CoarseTriangle.DecalPositions[2];

		if (!IntersectsOnBoxAxes(P0, P1, P2, CanonicalHalfExtent))
		{
			++LocalStats.RejectedByBoxAxisCount;
			continue;
		}
		++LocalStats.PassedBoxAxisCount;

		if (!IntersectsOnTriangleNormal(P0, P1, P2, CanonicalHalfExtent))
		{
			++LocalStats.RejectedByTriangleNormalCount;
			continue;
		}
		++LocalStats.PassedTriangleNormalCount;

		if (!IntersectsOnEdgeCrossAxes(P0, P1, P2, CanonicalHalfExtent))
		{
			++LocalStats.RejectedByEdgeCrossAxisCount;
			continue;
		}
		++LocalStats.PassedEdgeCrossAxisCount;

		FDecalSATTriangle SATTriangle;
		SATTriangle.OwnerActor = CoarseTriangle.OwnerActor;
		SATTriangle.StaticMeshComponent = CoarseTriangle.StaticMeshComponent;
		SATTriangle.TriangleStartIndex = CoarseTriangle.TriangleStartIndex;
		SATTriangle.MeshToWorld = CoarseTriangle.MeshToWorld;
		SATTriangle.WorldToMesh = CoarseTriangle.WorldToMesh;
		SATTriangle.MeshToDecal = CoarseTriangle.MeshToDecal;
		SATTriangle.DecalPositions[0] = P0;
		SATTriangle.DecalPositions[1] = P1;
		SATTriangle.DecalPositions[2] = P2;
		SATTriangle.DecalFaceNormal = CoarseTriangle.DecalFaceNormal;
		SATTriangle.DecalLocalTriangleAABB = CoarseTriangle.DecalLocalTriangleAABB;
		SATTriangle.bAnyVertexInsideBox = CoarseTriangle.bAnyVertexInsideBox;

		OutSATTriangles.push_back(SATTriangle);
		++LocalStats.PassedSATCount;
	}

	if (OutStats)
	{
		*OutStats = LocalStats;
	}
}

void FDecalMeshBuilder::ClipSATTrianglesAgainstDecalBox(
	const TArray<FDecalSATTriangle>& SATTriangles,
	TArray<FDecalClippedPolygon>& OutPolygons,
	FDecalClipStats* OutStats)
{
	OutPolygons.clear();

	FDecalClipStats LocalStats;
	LocalStats.InputTriangleCount = static_cast<int32>(SATTriangles.size());

	for (const FDecalSATTriangle& SATTriangle : SATTriangles)
	{
		TArray<FVector> WorkingA;
		TArray<FVector> WorkingB;

		WorkingA.reserve(12);
		WorkingB.reserve(12);

		// triangle을 polygon으로 시작
		WorkingA.push_back(SATTriangle.DecalPositions[0]);
		WorkingA.push_back(SATTriangle.DecalPositions[1]);
		WorkingA.push_back(SATTriangle.DecalPositions[2]);

		// 6개 평면에 대해 순차 clip
		ClipPolygonAgainstAxisPlane(WorkingA, WorkingB, 0, -0.5f, false); // X >= -0.5
		if (WorkingB.empty()) { ++LocalStats.RejectedEmptyPolygonCount; continue; }
		WorkingA.swap(WorkingB);

		ClipPolygonAgainstAxisPlane(WorkingA, WorkingB, 0, 0.5f, true);  // X <= 0.5
		if (WorkingB.empty()) { ++LocalStats.RejectedEmptyPolygonCount; continue; }
		WorkingA.swap(WorkingB);

		ClipPolygonAgainstAxisPlane(WorkingA, WorkingB, 1, -0.5f, false); // Y >= -0.5
		if (WorkingB.empty()) { ++LocalStats.RejectedEmptyPolygonCount; continue; }
		WorkingA.swap(WorkingB);

		ClipPolygonAgainstAxisPlane(WorkingA, WorkingB, 1, 0.5f, true);  // Y <= 0.5
		if (WorkingB.empty()) { ++LocalStats.RejectedEmptyPolygonCount; continue; }
		WorkingA.swap(WorkingB);

		ClipPolygonAgainstAxisPlane(WorkingA, WorkingB, 2, -0.5f, false); // Z >= -0.5
		if (WorkingB.empty()) { ++LocalStats.RejectedEmptyPolygonCount; continue; }
		WorkingA.swap(WorkingB);

		ClipPolygonAgainstAxisPlane(WorkingA, WorkingB, 2, 0.5f, true);  // Z <= 0.5
		if (WorkingB.empty()) { ++LocalStats.RejectedEmptyPolygonCount; continue; }
		WorkingA.swap(WorkingB);

		RemoveNearDuplicateVertices(WorkingA);

		if (WorkingA.size() < 3)
		{
			++LocalStats.RejectedDegeneratePolygonCount;
			continue;
		}

		const float Area = ComputePolygonAreaEstimate(WorkingA, SATTriangle.DecalFaceNormal);
		if (Area <= 0.000001f)
		{
			++LocalStats.RejectedDegeneratePolygonCount;
			continue;
		}

		FDecalClippedPolygon Polygon;
		Polygon.OwnerActor = SATTriangle.OwnerActor;
		Polygon.StaticMeshComponent = SATTriangle.StaticMeshComponent;
		Polygon.TriangleStartIndex = SATTriangle.TriangleStartIndex;
		Polygon.MeshToWorld = SATTriangle.MeshToWorld;
		Polygon.WorldToMesh = SATTriangle.WorldToMesh;
		Polygon.MeshToDecal = SATTriangle.MeshToDecal;
		Polygon.DecalFaceNormal = SATTriangle.DecalFaceNormal;
		Polygon.DecalPositions = std::move(WorkingA);

		LocalStats.TotalOutputVertexCount += static_cast<int32>(Polygon.DecalPositions.size());
		++LocalStats.EmittedPolygonCount;

		OutPolygons.push_back(std::move(Polygon));
	}

	if (OutStats)
	{
		*OutStats = LocalStats;
	}
}

void FDecalMeshBuilder::TriangulateClippedPolygons(
	const TArray<FDecalClippedPolygon>& ClippedPolygons,
	TArray<FDecalTriangulatedTriangle>& OutTriangles,
	FDecalTriangulationStats* OutStats)
{
	OutTriangles.clear();

	FDecalTriangulationStats LocalStats;
	LocalStats.InputPolygonCount = static_cast<int32>(ClippedPolygons.size());

	for (const FDecalClippedPolygon& Polygon : ClippedPolygons)
	{
		const size_t VertexCount = Polygon.DecalPositions.size();
		if (VertexCount < 3)
		{
			++LocalStats.SkippedTooSmallPolygonCount;
			continue;
		}

		/*
			convex polygon fan triangulation:
			(V0, V1, V2), (V0, V2, V3), ...
		*/
		const FVector& V0 = Polygon.DecalPositions[0];

		for (size_t i = 1; i + 1 < VertexCount; ++i)
		{
			FDecalTriangulatedTriangle Triangle;
			Triangle.OwnerActor = Polygon.OwnerActor;
			Triangle.StaticMeshComponent = Polygon.StaticMeshComponent;
			Triangle.TriangleStartIndex = Polygon.TriangleStartIndex;
			Triangle.MeshToWorld = Polygon.MeshToWorld;
			Triangle.WorldToMesh = Polygon.WorldToMesh;
			Triangle.MeshToDecal = Polygon.MeshToDecal;
			Triangle.DecalFaceNormal = Polygon.DecalFaceNormal;

			Triangle.DecalPositions[0] = V0;
			Triangle.DecalPositions[1] = Polygon.DecalPositions[i];
			Triangle.DecalPositions[2] = Polygon.DecalPositions[i + 1];

			OutTriangles.push_back(Triangle);
			++LocalStats.EmittedTriangleCount;
		}
	}

	if (OutStats)
	{
		*OutStats = LocalStats;
	}
}

void FDecalMeshBuilder::ComputeTriangleUVs(
	const TArray<FDecalTriangulatedTriangle>& InTriangles,
	TArray<FDecalUVTriangle>& OutUVTriangles,
	FDecalUVStats* OutStats)
{
	OutUVTriangles.clear();

	FDecalUVStats LocalStats;
	LocalStats.InputTriangleCount = static_cast<int32>(InTriangles.size());

	for (const FDecalTriangulatedTriangle& InTriangle : InTriangles)
	{
		FDecalUVTriangle OutTriangle;
		OutTriangle.OwnerActor = InTriangle.OwnerActor;
		OutTriangle.StaticMeshComponent = InTriangle.StaticMeshComponent;
		OutTriangle.TriangleStartIndex = InTriangle.TriangleStartIndex;
		OutTriangle.MeshToWorld = InTriangle.MeshToWorld;
		OutTriangle.WorldToMesh = InTriangle.WorldToMesh;
		OutTriangle.MeshToDecal = InTriangle.MeshToDecal;
		OutTriangle.DecalFaceNormal = InTriangle.DecalFaceNormal;

		for (int32 i = 0; i < 3; ++i)
		{
			OutTriangle.Vertices[i].Position = InTriangle.DecalPositions[i];
			OutTriangle.Vertices[i].UV = ComputeDecalUVFromLocalPosition(InTriangle.DecalPositions[i]);
		}

		OutUVTriangles.push_back(OutTriangle);
		++LocalStats.OutputTriangleCount;
	}

	if (OutStats)
	{
		*OutStats = LocalStats;
	}
}

void FDecalMeshBuilder::BuildRenderableMesh(
	const TArray<FDecalUVTriangle>& UVTriangles,
	FDecalRenderableMesh& OutMesh,
	FDecalRenderableMeshStats* OutStats)
{
	OutMesh.Clear();

	FDecalRenderableMeshStats LocalStats;
	LocalStats.InputTriangleCount = static_cast<int32>(UVTriangles.size());

	if (UVTriangles.empty())
	{
		if (OutStats)
		{
			*OutStats = LocalStats;
		}
		return;
	}

	/*
		지금 단계에서는 triangle마다 정점을 공유하지 않고 그대로 밀어넣습니다.

		왜 인덱스 최적화를 지금 안 하나?
		- decal clipping 결과는 정점 공유율이 높지 않을 수 있음
		- 디버깅 단계에서는 "단순하고 확실한 구조"가 더 중요
		- 나중에 필요하면 vertex welding 최적화 가능
	*/
	OutMesh.Vertices.reserve(UVTriangles.size() * 3);
	OutMesh.Indices.reserve(UVTriangles.size() * 3);

	FDecalRenderableSection Section;
	Section.FirstIndex = 0;

	for (const FDecalUVTriangle& UVTriangle : UVTriangles)
	{
		const uint32 BaseVertexIndex = static_cast<uint32>(OutMesh.Vertices.size());

		for (int32 i = 0; i < 3; ++i)
		{
			FDecalRenderableVertex Vertex;
			Vertex.Position = UVTriangle.Vertices[i].Position;
			Vertex.Normal = UVTriangle.DecalFaceNormal;
			Vertex.Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
			Vertex.UV = UVTriangle.Vertices[i].UV;

			OutMesh.Vertices.push_back(Vertex);
		}

		OutMesh.Indices.push_back(BaseVertexIndex + 0);
		OutMesh.Indices.push_back(BaseVertexIndex + 1);
		OutMesh.Indices.push_back(BaseVertexIndex + 2);
	}

	Section.IndexCount = static_cast<uint32>(OutMesh.Indices.size());
	OutMesh.Sections.push_back(Section);

	LocalStats.OutputVertexCount = static_cast<int32>(OutMesh.Vertices.size());
	LocalStats.OutputIndexCount = static_cast<int32>(OutMesh.Indices.size());
	LocalStats.OutputTriangleCount = static_cast<int32>(OutMesh.Indices.size() / 3);

	if (OutStats)
	{
		*OutStats = LocalStats;
	}
}