#pragma once
#include "Component/PrimitiveComponent.h"
#include "Core/DecalTypes.h"

class UMaterialInterface;
class FArchive;

enum EDecalTargetFilterBits : int32
{
	DecalTarget_None = 0,
	DecalTarget_StaticMeshComponent = 1 << 0,
	DecalTarget_AllPrimitive = 1 << 1,
};

class UDecalComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UDecalComponent, UPrimitiveComponent)
	
	UDecalComponent() = default;
	~UDecalComponent() override = default;

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;
	/*
	* EditorRenderPipeline은 선택된 Actor의 Component들에 대해
	* CollectEditorVisualizations()를 호출해주므로,
	* SceneProxy 없이도 OBB wireframe 화면에 올릴 수 있음.
	*/
	void CollectEditorVisualizations(FRenderBus& RenderBus) const override;

	void Serialize(FArchive& AR) override;
	void PostDuplicate() override;

public:
	/*
	* geometry rebuild가 필요한가 를 나타내는 flag.
	* 초기 단계 (Material / Sort / Filter / Size 변경 모두 하나의 Dirty로 묶음)
	* 나중 (GeometryDirty / MaterialDirty / SortDirty 분리)
	*/
	void MarkDecalDirty();
	void ClearDecalDirty() { bDecalDirty = false; }
	bool IsDecalDirty() const { return bDecalDirty; }

	/*
	* 지금은 실제 렌더 메시가 없으므로 SceneProxy를 만들지 않음.
	* 나중에 FDecalSceneProxy를 만들어 여기서 반환하면 됨.
	*/
	FPrimitiveSceneProxy* CreateSceneProxy() override;

	void RebuildDecalMeshNow();

	/*
	* Decal OBB를 감싸는 월드 AABB
	*/
	void UpdateWorldAABB() const override;
	/*
	* 현재는 실제 렌더링되는 프리미티브가 아니므로 outline 대상일 필요 없음.
	* 나중에 실제 렌더 mesh가 생긴 뒤에도, outline 정책은 별도로 결정하는 편이 나음.
	*/
	bool SupportsOutline() const override { return false; }
	

public:
	// ---------
	// | Decal |
	// ---------
	void SetDecalSize(const FVector& InSize);
	const FVector& GetDecalSize() const { return DecalSize; }

	void SetDecalMaterial(UMaterialInterface* NewDecalMaterial);
	UMaterialInterface* GetDecalMaterial() const { return DecalMaterial; }
	const FString& GetDecalMaterialPath() const { return DecalMaterialPath; }

	void SetSortOrder(int32 Value);
	int32 GetSortOrder() const { return SortOrder; }

	void SetTargetFilter(int32 InFilter);
	int32 GetTargetFilter() const { return TargetFilter; }

	void SetDrawDebugOBB(bool bEnable) { bDrawDebugOBB = bEnable; }
	bool IsDrawDebugOBBEnabled() const { return bDrawDebugOBB; }

	FTransform GetTransformIncludingDecalSize() const;
	FMatrix GetDecalLocalToWorldMatrix() const;
	FMatrix GetWorldToDecalMatrix() const;

	/*
	* OBB 8개 코너 / OBB를 감싸는 월드 AABB
	*/
	void GetDecalBoxCorners(FVector (&OutCorners)[8]) const;
	FBoundingBox GetDecalWorldAABB() const;

	void SetFadeOut(float StartDelay, float Duration, bool DestroyOwnerAfterFade = true);

	void DebugRunBroadPhase() const;

	const FDecalRenderableMesh& GetRenderableMesh() const { return RenderableMesh; }

private:
	void BuildDecalMesh();

	/*
	* - 저장/복제는 경로 기반
	* - 런타임 사용은 포인터 기반
	*/
	void ReloadMaterialFromPath();
	/*
	* OBB 선 12개 + forward 축 1개를 RenderBus에 넣음.
	* wireframe만 봐도 transform/size/forward 축이 바로 드러나게 만들기 위한 함수
	*/
	void AddDebugOBBLines(FRenderBus& RenderBus, const FColor& BoxColor) const;

private:
	FVector DecalSize = FVector(1.0f, 1.0f, 1.0f);
	UMaterialInterface* DecalMaterial = nullptr;
	FString DecalMaterialPath = "None";

	FDecalRenderableMesh RenderableMesh;

	int32 SortOrder = 0;

	int32 TargetFilter = DecalTarget_StaticMeshComponent;

	bool bDecalDirty = true;
	bool bDrawDebugOBB = true;
};
