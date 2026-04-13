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

private:
	static bool PassTargetFilter(
		const UDecalComponent& DecalComponent,
		const UPrimitiveComponent* Primitive);
};