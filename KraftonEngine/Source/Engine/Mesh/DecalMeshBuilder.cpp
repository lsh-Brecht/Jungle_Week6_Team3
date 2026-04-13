#include "Mesh/DecalMeshBuilder.h"

#include "Component/DecalComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Mesh/StaticMesh.h"
#include "Mesh/StaticMeshAsset.h"
#include "Object/Object.h"

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

void GatherBruteForceTriangles(
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