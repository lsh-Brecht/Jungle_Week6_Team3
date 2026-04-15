#include "Mesh/MeshDecalMeshBuilder.h"

#include "Components/MeshDecalComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Mesh/StaticMesh.h"
#include "Mesh/StaticMeshAsset.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float Epsilon = 1e-6f;
	constexpr float MeshDecalPushBias = 0.0005f;

	bool IsPointInsideUnitDecalBox(const FVector& P)
	{
		return P.X >= -0.5f && P.X <= 0.5f
			&& P.Y >= -0.5f && P.Y <= 0.5f
			&& P.Z >= -0.5f && P.Z <= 0.5f;
	}

	float GetAxis(const FVector& V, int32 Axis)
	{
		return (Axis == 0) ? V.X : ((Axis == 1) ? V.Y : V.Z);
	}

	void SetAxis(FVector& V, int32 Axis, float Value)
	{
		if (Axis == 0) V.X = Value;
		else if (Axis == 1) V.Y = Value;
		else V.Z = Value;
	}

	FVector ComputeNormal(const FVector& A, const FVector& B, const FVector& C)
	{
		FVector N = (B - A).Cross(C - A);
		const float Len = N.Length();
		if (Len > Epsilon)
		{
			N /= Len;
		}
		return N;
	}

	FVector2 ComputeDecalUV(const FVector& DecalLocalPos)
	{
		const float U = std::clamp(DecalLocalPos.Y + 0.5f, 0.0f, 1.0f);
		const float V = std::clamp(0.5f - DecalLocalPos.Z, 0.0f, 1.0f);
		return FVector2(U, V);
	}

	FBoundingBox ComputeTriangleAABB(const FVector& A, const FVector& B, const FVector& C)
	{
		FBoundingBox Box;
		Box.Expand(A);
		Box.Expand(B);
		Box.Expand(C);
		return Box;
	}

	FBoundingBox UnitDecalBox()
	{
		return FBoundingBox(FVector(-0.5f, -0.5f, -0.5f), FVector(0.5f, 0.5f, 0.5f));
	}

	bool IsInsidePlane(const FVector& P, int32 Axis, float PlaneValue, bool bKeepLessEqual)
	{
		const float V = GetAxis(P, Axis);
		return bKeepLessEqual ? (V <= PlaneValue) : (V >= PlaneValue);
	}

	FVector IntersectSegmentAxisPlane(const FVector& A, const FVector& B, int32 Axis, float PlaneValue)
	{
		const float AV = GetAxis(A, Axis);
		const float BV = GetAxis(B, Axis);
		const float Denom = BV - AV;
		if (std::abs(Denom) <= Epsilon)
		{
			FVector Result = A;
			SetAxis(Result, Axis, PlaneValue);
			return Result;
		}
		const float T = (PlaneValue - AV) / Denom;
		return A + (B - A) * T;
	}

	void ClipPolygonAgainstAxisPlane(
		const TArray<FVector>& InVertices,
		TArray<FVector>& OutVertices,
		int32 Axis,
		float PlaneValue,
		bool bKeepLessEqual)
	{
		OutVertices.clear();
		if (InVertices.empty())
		{
			return;
		}

		const size_t Count = InVertices.size();
		for (size_t i = 0; i < Count; ++i)
		{
			const FVector& Current = InVertices[i];
			const FVector& Prev = InVertices[(i + Count - 1) % Count];
			const bool bCurrentInside = IsInsidePlane(Current, Axis, PlaneValue, bKeepLessEqual);
			const bool bPrevInside = IsInsidePlane(Prev, Axis, PlaneValue, bKeepLessEqual);

			if (bPrevInside && bCurrentInside)
			{
				OutVertices.push_back(Current);
			}
			else if (bPrevInside && !bCurrentInside)
			{
				OutVertices.push_back(IntersectSegmentAxisPlane(Prev, Current, Axis, PlaneValue));
			}
			else if (!bPrevInside && bCurrentInside)
			{
				OutVertices.push_back(IntersectSegmentAxisPlane(Prev, Current, Axis, PlaneValue));
				OutVertices.push_back(Current);
			}
		}
	}

	void EmitPolygonAsTriangles(
		const TArray<FVector>& Poly,
		const FVector& FaceNormal,
		FMeshDecalRenderableMesh& OutMesh)
	{
		if (Poly.size() < 3)
		{
			return;
		}

		const FVector& V0 = Poly[0];
		for (size_t i = 1; i + 1 < Poly.size(); ++i)
		{
			const uint32 Base = static_cast<uint32>(OutMesh.Vertices.size());

			const FVector TriPos[3] = { V0, Poly[i], Poly[i + 1] };
			for (int32 k = 0; k < 3; ++k)
			{
				FMeshDecalRenderableVertex Vertex = {};
				Vertex.Position = TriPos[k] + FaceNormal * MeshDecalPushBias;
				Vertex.Normal = FaceNormal;
				Vertex.Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
				Vertex.UV = ComputeDecalUV(TriPos[k]);
				OutMesh.Vertices.push_back(Vertex);
			}

			OutMesh.Indices.push_back(Base + 0);
			OutMesh.Indices.push_back(Base + 1);
			OutMesh.Indices.push_back(Base + 2);
		}
	}
}

void FMeshDecalMeshBuilder::BuildRenderableMesh(const UMeshDecalComponent& MeshDecalComponent, const UWorld& World, FMeshDecalRenderableMesh& OutMesh)
{
	OutMesh.Clear();

	const FBoundingBox DecalWorldAABB = MeshDecalComponent.GetMeshDecalWorldAABB();
	if (!DecalWorldAABB.IsValid())
	{
		return;
	}

	const FBoundingBox CanonicalBox = UnitDecalBox();
	const FMatrix WorldToDecal = MeshDecalComponent.GetWorldToMeshDecalMatrix();

	for (AActor* Actor : World.GetActors())
	{
		if (!Actor || !Actor->IsVisible())
		{
			continue;
		}
		if (MeshDecalComponent.IsExcludeSameOwnerEnabled() && Actor == MeshDecalComponent.GetOwner())
		{
			continue;
		}

		for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
		{
			UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(Primitive);
			if (!StaticMeshComp || !StaticMeshComp->IsVisible())
			{
				continue;
			}
			// Engine-side "ReceivesDecal" flag is not exposed on UStaticMeshComponent yet.
			// Keep behavior stable by not filtering static meshes here.
			if (!DecalWorldAABB.IsIntersected(StaticMeshComp->GetWorldBoundingBox()))
			{
				continue;
			}

			UStaticMesh* StaticMesh = StaticMeshComp->GetStaticMesh();
			FStaticMesh* MeshAsset = StaticMesh ? StaticMesh->GetStaticMeshAsset() : nullptr;
			if (!MeshAsset || MeshAsset->Indices.size() < 3 || MeshAsset->Vertices.empty())
			{
				continue;
			}

			const FMatrix MeshToDecal = StaticMeshComp->GetWorldMatrix() * WorldToDecal;

			for (size_t Index = 0; Index + 2 < MeshAsset->Indices.size(); Index += 3)
			{
				const uint32 I0 = MeshAsset->Indices[Index + 0];
				const uint32 I1 = MeshAsset->Indices[Index + 1];
				const uint32 I2 = MeshAsset->Indices[Index + 2];
				if (I0 >= MeshAsset->Vertices.size() || I1 >= MeshAsset->Vertices.size() || I2 >= MeshAsset->Vertices.size())
				{
					continue;
				}

				const FVector P0 = MeshToDecal.TransformPositionWithW(MeshAsset->Vertices[I0].pos);
				const FVector P1 = MeshToDecal.TransformPositionWithW(MeshAsset->Vertices[I1].pos);
				const FVector P2 = MeshToDecal.TransformPositionWithW(MeshAsset->Vertices[I2].pos);

				// 빠른 reject
				const FBoundingBox TriAABB = ComputeTriangleAABB(P0, P1, P2);
				if (!TriAABB.IsIntersected(CanonicalBox))
				{
					continue;
				}
				const bool bAnyInside = IsPointInsideUnitDecalBox(P0) || IsPointInsideUnitDecalBox(P1) || IsPointInsideUnitDecalBox(P2);
				if (!bAnyInside && !MeshDecalComponent.IsLooseTriangleAcceptEnabled())
				{
					continue;
				}

				TArray<FVector> PolyA;
				TArray<FVector> PolyB;
				PolyA.reserve(12);
				PolyB.reserve(12);
				PolyA.push_back(P0);
				PolyA.push_back(P1);
				PolyA.push_back(P2);

				ClipPolygonAgainstAxisPlane(PolyA, PolyB, 0, -0.5f, false); if (PolyB.empty()) continue; PolyA.swap(PolyB);
				ClipPolygonAgainstAxisPlane(PolyA, PolyB, 0, 0.5f, true);  if (PolyB.empty()) continue; PolyA.swap(PolyB);
				ClipPolygonAgainstAxisPlane(PolyA, PolyB, 1, -0.5f, false); if (PolyB.empty()) continue; PolyA.swap(PolyB);
				ClipPolygonAgainstAxisPlane(PolyA, PolyB, 1, 0.5f, true);  if (PolyB.empty()) continue; PolyA.swap(PolyB);
				ClipPolygonAgainstAxisPlane(PolyA, PolyB, 2, -0.5f, false); if (PolyB.empty()) continue; PolyA.swap(PolyB);
				ClipPolygonAgainstAxisPlane(PolyA, PolyB, 2, 0.5f, true);  if (PolyB.empty()) continue; PolyA.swap(PolyB);

				if (PolyA.size() < 3)
				{
					continue;
				}

				const FVector FaceN = ComputeNormal(P0, P1, P2);
				EmitPolygonAsTriangles(PolyA, FaceN, OutMesh);
			}
		}
	}

	if (!OutMesh.Indices.empty())
	{
		FMeshDecalRenderableSection Section = {};
		Section.FirstIndex = 0;
		Section.IndexCount = static_cast<uint32>(OutMesh.Indices.size());
		OutMesh.Sections.push_back(Section);
	}
}
