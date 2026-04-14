#include "Render/Proxy/DecalSceneProxy.h"
#include "Render/Pipeline/RenderBus.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInterface.h"
#include "Render/Resource/ConstantBufferPool.h"
#include "Render/Resource/ShaderManager.h"
#include "Texture/Texture2D.h"

namespace
{
    struct FDecalObjectConstants
    {
        FGPUFloat4x4 InverseDecalModel;
    };
}

FDecalSceneProxy::FDecalSceneProxy(UDecalComponent* InComponent)
    : FPrimitiveSceneProxy(InComponent)
{
	bShowAABB = false;
	bSupportsOutline = false;
}

void FDecalSceneProxy::UpdateMesh()
{
    MeshBuffer = Owner->GetMeshBuffer();
	Shader = FShaderManager::Get().GetShader(EShaderType::Decal);
	Pass = ERenderPass::Decal;
    RebuildSectionDraw();
}

void FDecalSceneProxy::UpdateMaterial()
{
    RebuildSectionDraw();
}

void FDecalSceneProxy::UpdateTransform()
{
    FPrimitiveSceneProxy::UpdateTransform();
    if (UDecalComponent* DecalComp = GetDecalComponent())
    {
        PerObjectConstants.Model = DecalComp->GetTransformIncludingDecalSize();
        CachedWorldPos = PerObjectConstants.Model.GetLocation();
        PerObjectConstants.Color = DecalComp->DecalColor.ToVector4();

        auto& DecalCB = ExtraCB.Bind<FDecalObjectConstants>(
            FConstantBufferPool::Get().GetBuffer(ECBSlot::Gizmo, sizeof(FDecalObjectConstants)),
            ECBSlot::Gizmo);
        DecalCB.InverseDecalModel = PerObjectConstants.Model.GetInverse();

        MarkPerObjectCBDirty();
    }
}

void FDecalSceneProxy::CollectEntries(FRenderBus& Bus)
{
	// 1. 부모 클래스의 기본 수집 로직 호출 (필수)
	FPrimitiveSceneProxy::CollectEntries(Bus);

	if (!bVisible)
	{
		return;
	}

	// 2. 에디터 뷰포트 판별: FShowFlags의 bGizmo가 켜져 있거나, 현재 데칼이 선택(bSelected)되었을 때만 OBB를 그립니다.
	// (ViewTypes.h 에 정의된 플래그를 활용하여 게임 모드에서는 자연스럽게 숨겨지도록 처리)
	if (bSelected)
	{
		FOBBEntry Entry;

		// 크기가 1인 로컬 단위 육면체 정의 (-0.5 ~ 0.5)
		// 데칼의 투영 범위를 나타냅니다.
		Entry.OBB.LocalBox = FBoundingBox(FVector(-0.5f, -0.5f, -0.5f), FVector(0.5f, 0.5f, 0.5f));

		// UpdateTransform()에서 캐싱해둔 최신 데칼 월드 변환 행렬 (스케일, 회전, 위치 모두 포함)
		Entry.OBB.Transform = PerObjectConstants.Model;

		// 엔진에 구현된 FColor::Green()을 사용하여 깔끔한 초록색 지정
		Entry.OBB.Color = FColor::Green();

		// 렌더 버스에 OBB 엔트리를 추가하여 LineBatcher가 그리도록 전달!
		Bus.AddOBBEntry(std::move(Entry));
	}
}

UDecalComponent* FDecalSceneProxy::GetDecalComponent() const
{
    return static_cast<UDecalComponent*>(Owner);
}

void FDecalSceneProxy::RebuildSectionDraw()
{
    SectionDraws.clear();

    FMeshSectionDraw Draw = {};
    if (UDecalComponent* DecalComp = GetDecalComponent())
    {
        if (UMaterialInterface* Material = DecalComp->GetDecalMaterial())
        {
            if (UTexture2D* DiffuseTexture = Material->GetDiffuseTexture())
            {
                Draw.DiffuseSRV = DiffuseTexture->GetSRV();
            }
            Draw.DiffuseColor = Material->GetDiffuseColor();
        }
    }

    if (MeshBuffer)
    {
        Draw.IndexCount = MeshBuffer->GetIndexBuffer().GetIndexCount();
    }

    SectionDraws.push_back(Draw);
    UpdateSortKey();
}
