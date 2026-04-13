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

struct FDecalLocalTriangle
{
	AActor* OwnerActor = nullptr;
	UStaticMeshComponent* StaticMeshComponent = nullptr;

	int32 TriangleStartIndex = -1;

	FMatrix MeshToWorld;
	FMatrix WorldToMesh;
	FMatrix MeshToDecal;

	FVector DecalPositions[3];

	FVector DecalFaceNormal;
};

struct FDecalTransformStats
{
	int32 SourceTriangleCount = 0;
	int32 TransformedTriangleCount = 0;
};

struct FDecalCoarseOverlapTriangle
{
	AActor* OwnerActor = nullptr;
	UStaticMeshComponent* StaticMeshComponent = nullptr;

	int32 TriangleStartIndex = -1;

	FMatrix MeshToWorld;
	FMatrix WorldToMesh;
	FMatrix MeshToDecal;

	FVector DecalPositions[3];
	FVector DecalFaceNormal;

	// 디버그용
	FBoundingBox DecalLocalTriangleAABB;
	bool bAnyVertexInsideBox = false;
};

struct FDecalCoarseOverlapStats
{
	int32 InputTriangleCount = 0;
	int32 TriangleAABBOverlapCount = 0;
	int32 AnyVertexInsideCount = 0;
	int32 CoarseAcceptedCount = 0;
};

struct FDecalSATTriangle
{
	AActor* OwnerActor = nullptr;
	UStaticMeshComponent* StaticMeshComponent = nullptr;

	int32 TriangleStartIndex = -1;

	FMatrix MeshToWorld;
	FMatrix WorldToMesh;
	FMatrix MeshToDecal;

	FVector DecalPositions[3];
	FVector DecalFaceNormal;

	FBoundingBox DecalLocalTriangleAABB;
	bool bAnyVertexInsideBox = false;
};

struct FDecalSATStats
{
	int32 InputTriangleCount = 0;

	int32 PassedBoxAxisCount = 0;
	int32 PassedTriangleNormalCount = 0;
	int32 PassedEdgeCrossAxisCount = 0;

	int32 RejectedByBoxAxisCount = 0;
	int32 RejectedByTriangleNormalCount = 0;
	int32 RejectedByEdgeCrossAxisCount = 0;

	int32 PassedSATCount = 0;
};

struct FDecalClippedPolygon
{
	AActor* OwnerActor = nullptr;
	UStaticMeshComponent* StaticMeshComponent = nullptr;

	int32 TriangleStartIndex = -1;

	FMatrix MeshToWorld;
	FMatrix WorldToMesh;
	FMatrix MeshToDecal;

	// decal local 기준으로 clip된 convex polygon 정점들
	TArray<FVector> DecalPositions;

	FVector DecalFaceNormal;
};

struct FDecalClipStats
{
	int32 InputTriangleCount = 0;
	int32 EmittedPolygonCount = 0;
	int32 RejectedEmptyPolygonCount = 0;
	int32 RejectedDegeneratePolygonCount = 0;
	int32 TotalOutputVertexCount = 0;
};