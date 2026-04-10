#include "RenderBus.h"
#include "Component/CameraComponent.h"
#include "Viewport/Viewport.h"

void FRenderBus::Clear()
{
	for (uint32 i = 0; i < (uint32)ERenderPass::MAX; ++i)
	{
		ProxyQueues[i].clear();
	}

	FontEntries.clear();
	OverlayFontEntries.clear();
	SubUVEntries.clear();
	BillboardEntries.clear();
	AABBEntries.clear();
	GridEntries.clear();
	DebugLineEntries.clear();

	ViewportRTV = nullptr;
	ViewportDSV = nullptr;
	ViewportStencilSRV = nullptr;
	ViewportGBufferAlbedoRTV = nullptr;
	ViewportGBufferNormalRTV = nullptr;
	ViewportGBufferAlbedoSRV = nullptr;
	ViewportGBufferNormalSRV = nullptr;
}

void FRenderBus::AddProxy(ERenderPass Pass, const FPrimitiveSceneProxy* Proxy)
{
	ProxyQueues[(uint32)Pass].push_back(Proxy);
}

const TArray<const FPrimitiveSceneProxy*>& FRenderBus::GetProxies(ERenderPass Pass) const
{
	return ProxyQueues[(uint32)Pass];
}

void FRenderBus::AddFontEntry(FFontEntry&& Entry)
{
	FontEntries.push_back(std::move(Entry));
}

void FRenderBus::AddOverlayFontEntry(FFontEntry&& Entry)
{
	OverlayFontEntries.push_back(std::move(Entry));
}

void FRenderBus::AddSubUVEntry(FSubUVEntry&& Entry)
{
	SubUVEntries.push_back(std::move(Entry));
}

void FRenderBus::AddBillboardEntry(FBillboardEntry&& Entry)
{
	BillboardEntries.push_back(std::move(Entry));
}

void FRenderBus::AddAABBEntry(FAABBEntry&& Entry)
{
	AABBEntries.push_back(std::move(Entry));
}

void FRenderBus::AddGridEntry(FGridEntry&& Entry)
{
	GridEntries.push_back(std::move(Entry));
}

void FRenderBus::AddDebugLineEntry(FDebugLineEntry&& Entry)
{
	DebugLineEntries.push_back(std::move(Entry));
}

void FRenderBus::SetCameraInfo(const UCameraComponent* Camera)
{
	View = Camera->GetViewMatrix();
	Proj = Camera->GetProjectionMatrix();
	CameraPosition = Camera->GetWorldLocation();
	CameraForward = Camera->GetForwardVector();
	CameraRight = Camera->GetRightVector();
	CameraUp = Camera->GetUpVector();
	bIsOrtho = Camera->IsOrthogonal();
	OrthoWidth = Camera->GetOrthoWidth();
}

void FRenderBus::SetViewportInfo(const FViewport* VP)
{
	viewportWidth = static_cast<float>(VP->GetWidth());
	viewportHeight = static_cast<float>(VP->GetHeight());
	ViewportRTV = VP->GetRTV();
	ViewportDSV = VP->GetDSV();
	ViewportStencilSRV = VP->GetStencilSRV();
	ViewportGBufferAlbedoRTV = VP->GetGBufferAlbedoRTV();
	ViewportGBufferNormalRTV = VP->GetGBufferNormalRTV();
	ViewportGBufferAlbedoSRV = VP->GetGBufferAlbedoSRV();
	ViewportGBufferNormalSRV = VP->GetGBufferNormalSRV();
}

void FRenderBus::SetRenderSettings(const EViewMode NewViewMode, const FShowFlags NewShowFlags)
{
	ViewMode = NewViewMode;
	ShowFlags = NewShowFlags;
}

void FRenderBus::SetViewportTargets(
	float InWidth,
	float InHeight,
	ID3D11RenderTargetView* InRTV,
	ID3D11DepthStencilView* InDSV,
	ID3D11ShaderResourceView* InStencilSRV,
	ID3D11RenderTargetView* InGBufferAlbedoRTV,
	ID3D11RenderTargetView* InGBufferNormalRTV,
	ID3D11ShaderResourceView* InGBufferAlbedoSRV,
	ID3D11ShaderResourceView* InGBufferNormalSRV)
{
	viewportWidth = InWidth;
	viewportHeight = InHeight;
	ViewportRTV = InRTV;
	ViewportDSV = InDSV;
	ViewportStencilSRV = InStencilSRV;
	ViewportGBufferAlbedoRTV = InGBufferAlbedoRTV;
	ViewportGBufferNormalRTV = InGBufferNormalRTV;
	ViewportGBufferAlbedoSRV = InGBufferAlbedoSRV;
	ViewportGBufferNormalSRV = InGBufferNormalSRV;
}

void FRenderBus::SetViewportSize(float InWidth, float InHeight)
{
	viewportWidth = InWidth;
	viewportHeight = InHeight;
}
