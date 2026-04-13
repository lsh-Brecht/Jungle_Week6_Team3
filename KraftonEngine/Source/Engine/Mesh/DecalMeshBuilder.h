#pragma once

#include "Core/ClassTypes.h"
#include "Core/DecalTypes.h"

class UDecalComponent;
class UWorld;
class UPrimitiveComponent;

class FDecalMeshBuilder
{
public:
	static void GatherBroadPhaseCandidates(
		const UDecalComponent& DecalComponent,
		const UWorld& World,
		TArray<FDecalPrimitiveCandidate>& OutCandidates,
		FDecalBroadPhaseStats* OutStats = nullptr);

	static void GatherBruteForceTriangles(
		const TArray<FDecalPrimitiveCandidate>& Candidates,
		TArray<FDecalSourceTriangle>& OutTriangles,
		FDecalTriangleGatherStats* OutStats = nullptr);

	static void GatherBVHFilteredTriangles(
		const UDecalComponent& DecalComponent,
		const TArray<FDecalPrimitiveCandidate>& Candidates,
		TArray<FDecalSourceTriangle>& OutTriangles,
		FDecalTriangleGatherStats* OutStats = nullptr);

	static void TransformTrianglesToDecalLocal(
		const UDecalComponent& DecalComponent,
		const TArray<FDecalSourceTriangle>& SourceTriangles,
		TArray<FDecalLocalTriangle>& OutLocalTriangles,
		FDecalTransformStats* OutStats = nullptr);

	static void GatherCoarseOverlapTriangles(
		const TArray<FDecalLocalTriangle>& LocalTriangles,
		TArray<FDecalCoarseOverlapTriangle>& OutAcceptedTriangles,
		FDecalCoarseOverlapStats* OutStats = nullptr);

	static void GatherSATOverlapTriangles(
		const TArray<FDecalCoarseOverlapTriangle>& CoarseTriangles,
		TArray<FDecalSATTriangle>& OutSATTriangles,
		FDecalSATStats* OutStats = nullptr);

	static void ClipSATTrianglesAgainstDecalBox(
		const TArray<FDecalSATTriangle>& SATTriangles,
		TArray<FDecalClippedPolygon>& OutPolygons,
		FDecalClipStats* OutStats = nullptr);

	static void TriangulateClippedPolygons(
		const TArray<FDecalClippedPolygon>& ClippedPolygons,
		TArray<FDecalTriangulatedTriangle>& OutTriangles,
		FDecalTriangulationStats* OutStats = nullptr);

	static void ComputeTriangleUVs(
		const TArray<FDecalTriangulatedTriangle>& InTriangles,
		TArray<FDecalUVTriangle>& OutUVTriangles,
		FDecalUVStats* OutStats = nullptr);

	static void BuildRenderableMesh(
		const TArray<FDecalUVTriangle>& UVTriangles,
		FDecalRenderableMesh& OutMesh,
		FDecalRenderableMeshStats* OutStats = nullptr);

private:
	static bool PassTargetFilter(
		const UDecalComponent& DecalComponent,
		const UStaticMeshComponent* StaticMeshComponent);
};
