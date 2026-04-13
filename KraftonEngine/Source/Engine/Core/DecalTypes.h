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

struct FDecalTriangulatedTriangle
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

struct FDecalTriangulationStats
{
	int32 InputPolygonCount = 0;
	int32 EmittedTriangleCount = 0;
	int32 SkippedTooSmallPolygonCount = 0;
};

struct FDecalUVTriangleVertex
{
	FVector Position;
	FVector2 UV;
};

struct FDecalUVTriangle
{
	AActor* OwnerActor = nullptr;
	UStaticMeshComponent* StaticMeshComponent = nullptr;

	int32 TriangleStartIndex = -1;

	FMatrix MeshToWorld;
	FMatrix WorldToMesh;
	FMatrix MeshToDecal;

	FDecalUVTriangleVertex Vertices[3];
	FVector DecalFaceNormal;
};

struct FDecalUVStats
{
	int32 InputTriangleCount = 0;
	int32 OutputTriangleCount = 0;
};

struct FDecalRenderableVertex
{
	FVector Position;
	FVector Normal;
	FVector4 Color;
	FVector2 UV;
};

struct FDecalRenderableSection
{
	uint32 FirstIndex = 0;
	uint32 IndexCount = 0;
};

struct FDecalRenderableMesh
{
	/*
		Position은 decal local 기준입니다.
		나중에 렌더 시 DecalLocalToWorld를 model matrix로 넘기면 됩니다.
	*/
	TArray<FDecalRenderableVertex> Vertices;
	TArray<uint32> Indices;

	/*
		처음에는 단일 머티리얼, 단일 section이면 충분합니다.
		나중에 SortOrder/Material 분할이 필요해지면 section을 늘리면 됩니다.
	*/
	TArray<FDecalRenderableSection> Sections;

	void Clear()
	{
		Vertices.clear();
		Indices.clear();
		Sections.clear();
	}

	bool IsEmpty() const
	{
		return Vertices.empty() || Indices.empty();
	}
};

struct FDecalRenderableMeshStats
{
	int32 InputTriangleCount = 0;
	int32 OutputVertexCount = 0;
	int32 OutputIndexCount = 0;
	int32 OutputTriangleCount = 0;
};