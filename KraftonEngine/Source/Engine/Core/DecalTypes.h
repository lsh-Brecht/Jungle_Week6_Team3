#pragma once

#include "Core/CoreTypes.h"
#include "Core/EngineTypes.h"
#include "Math/Matrix.h"

class AActor;
class UStaticMesh;
class UStaticMeshComponent;
struct FStaticMesh;

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

struct FDecalSourceTriangle
{
	AActor* OwnerActor = nullptr;
	UStaticMeshComponent* StaticMeshComponent = nullptr;

	int32 TriangleStartIndex = -1;

	uint32 Indices[3] = { 0, 0, 0 };

	// 4단계에서 decal local로 변환하기 전 원본 triangle
	FVector LocalPositions[3];
	FVector LocalNormals[3];

	// 나중 단계에서 바로 쓰기 좋게 같이 들고 갑니다.
	FMatrix MeshToWorld;
	FMatrix WorldToMesh;
};

struct FDecalTriangleGatherStats
{
	int32 CandidatePrimitiveCount = 0;
	int32 GatheredTriangleCount = 0;
	int32 SkippedInvalidTriangleCount = 0;
};