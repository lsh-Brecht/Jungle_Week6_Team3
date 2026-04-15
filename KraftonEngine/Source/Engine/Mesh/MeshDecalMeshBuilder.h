#pragma once

#include "Core/MeshDecalTypes.h"

class UMeshDecalComponent;
class UWorld;

class FMeshDecalMeshBuilder
{
public:
	static void BuildRenderableMesh(const UMeshDecalComponent& MeshDecalComponent, const UWorld& World, FMeshDecalRenderableMesh& OutMesh);
};

