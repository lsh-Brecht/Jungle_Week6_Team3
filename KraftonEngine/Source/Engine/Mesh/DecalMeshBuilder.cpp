#include "Mesh/DecalMeshBuilder.h"

#include "Component/DecalComponent.h"
#include "Component/StaticMeshComponent.h"
#include "GameFramework/World.h"

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