#pragma once

#include "Core/CoreTypes.h"
#include "Core/EngineTypes.h"
#include "Math/Matrix.h"

class AActor;
class UStaticMeshComponent;

struct FDecalPrimitiveCandidate
{
	AActor* OwnerActor = nullptr;

	UStaticMeshComponent* StaticMeshComponent = nullptr;
	UStaticMesh* StaticMesh = nullptr;
	FStaticMesh* MeshAsset = nullptr;

	FBoundingBox PrimitiveWorldAABB;

	FMatrix MeshToWorld;
	FMatrix WorldToMesh;
};

struct FDecalBroadPhaseStats
{
	int32 TotalPrimitiveCount = 0;
	int32 VisiblePrimitiveCount = 0;
	int32 StaticMeshPrimitiveCount = 0;
	int32 MeshReadyCount = 0;
	int32 TargetFilterPassedCount = 0;
	int32 BoundsOverlapCount = 0;
};