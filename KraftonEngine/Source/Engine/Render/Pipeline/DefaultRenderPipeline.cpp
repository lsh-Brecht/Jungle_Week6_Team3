#include "DefaultRenderPipeline.h"

#include "Renderer.h"
#include "Engine/Runtime/Engine.h"
#include "Component/CameraComponent.h"
#include "GameFramework/World.h"

FDefaultRenderPipeline::FDefaultRenderPipeline(UEngine* InEngine, FRenderer& InRenderer)
	: Engine(InEngine)
{
}

FDefaultRenderPipeline::~FDefaultRenderPipeline()
{
}

void FDefaultRenderPipeline::Execute(float DeltaTime, FRenderer& Renderer)
{
	Bus.Clear();

	UWorld* World = Engine->GetWorld();
	UCameraComponent* Camera = World ? World->GetActiveCamera() : nullptr;
	if (Camera)
	{
		FShowFlags ShowFlags;
		EViewMode ViewMode = EViewMode::Lit;

		Bus.SetCameraInfo(Camera);
		Bus.SetRenderSettings(ViewMode, ShowFlags);
		const D3D11_VIEWPORT& Viewport = Renderer.GetFD3DDevice().GetViewport();
		Bus.SetViewportTargets(
			Viewport.Width,
			Viewport.Height,
			Renderer.GetFD3DDevice().GetFrameBufferRTV(),
			Renderer.GetFD3DDevice().GetDepthStencilView(),
			Renderer.GetFD3DDevice().GetStencilSRV(),
			Renderer.GetFD3DDevice().GetGBufferAlbedoRTV(),
			Renderer.GetFD3DDevice().GetGBufferNormalRTV(),
			Renderer.GetFD3DDevice().GetGBufferAlbedoSRV(),
			Renderer.GetFD3DDevice().GetGBufferNormalSRV());

		Collector.CollectWorld(World, Bus);
		Collector.CollectDebugDraw(World->GetDebugDrawQueue(), Bus);
	}

	Renderer.PrepareBatchers(Bus);
	Renderer.BeginFrame();
	Renderer.Render(Bus);
	Renderer.EndFrame();
}
