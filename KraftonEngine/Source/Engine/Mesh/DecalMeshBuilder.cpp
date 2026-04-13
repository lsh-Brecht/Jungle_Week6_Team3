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
}

bool FDecalMeshBuilder::PassTargetFilter(
	const UDecalComponent& DecalComponent,
	const UPrimitiveComponent* Primitive)
{
	if (!Primitive) return false;

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