#include "Editor/EditorEngine.h"

#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "Component/CameraComponent.h"
#include "GameFramework/World.h"
#include "Editor/EditorRenderPipeline.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Object/ObjectFactory.h"
#include "Mesh/ObjManager.h"
#include "Input/InputSystem.h"
#include "GameFramework/AActor.h"
#include "Viewport/GameViewportClient.h"

IMPLEMENT_CLASS(UEditorEngine, UEngine)

void UEditorEngine::Init(FWindowsWindow* InWindow)
{
	// 엔진 공통 초기화 (Renderer, D3D, 싱글턴 등)
	UEngine::Init(InWindow);

	FObjManager::ScanMeshAssets();
	FObjManager::ScanMaterialAssets();

	// 에디터 전용 초기화
	FEditorSettings::Get().LoadFromFile(FEditorSettings::GetDefaultSettingsPath());

	MainPanel.Create(Window, Renderer, this);

	// World
	if (WorldList.empty())
	{
		CreateWorldContext(EWorldType::Editor, FName("Default"));
	}
	SetActiveWorld(WorldList[0].ContextHandle);
	GetWorld()->InitWorld();

	// Selection & Gizmo
	SelectionManager.Init();
	SelectionManager.SetWorld(GetWorld());

	// 뷰포트 레이아웃 초기화 + 저장된 설정 복원
	ViewportLayout.Initialize(this, Window, Renderer, &SelectionManager);
	ViewportLayout.LoadFromSettings();

	// Editor render pipeline
	SetRenderPipeline(std::make_unique<FEditorRenderPipeline>(this, Renderer));

	InputRouter.SetOwnerWindow(Window->GetHWND());
}

void UEditorEngine::Shutdown()
{
	// 에디터 해제 (엔진보다 먼저)
	ViewportLayout.SaveToSettings();
	FEditorSettings::Get().SaveToFile(FEditorSettings::GetDefaultSettingsPath());
	CloseScene();
	SelectionManager.Shutdown();
	MainPanel.Release();

	// 뷰포트 레이아웃 해제
	ViewportLayout.Release();

	// 엔진 공통 해제 (Renderer, D3D 등)
	UEngine::Shutdown();
}

void UEditorEngine::OnWindowResized(uint32 Width, uint32 Height)
{
	UEngine::OnWindowResized(Width, Height);
	// 윈도우 리사이즈 시에는 ImGui 패널이 실제 크기를 결정하므로
	// FViewport RT는 SSplitter 레이아웃에서 지연 리사이즈로 처리됨
}

void UEditorEngine::Tick(float DeltaTime)
{
	// --- PIE 요청 처리 (프레임 경계에서 처리되도록 Tick 선두에서 소비) ---
	if (bRequestEndPlayMapQueued)
	{
		bRequestEndPlayMapQueued = false;
		EndPlayMap();
	}
	if (PlaySessionRequest.has_value())
	{
		StartQueuedPlaySessionRequest();
	}

	MainPanel.Update();

	InputRouter.SetImGuiCaptureState(
		InputSystem::Get().GetGuiInputState().bUsingMouse,
		InputSystem::Get().GetGuiInputState().bUsingKeyboard || InputSystem::Get().GetGuiInputState().bUsingTextInput);
	InputRouter.ClearTargets();

	for (FLevelEditorViewportClient* VC : ViewportLayout.GetLevelViewportClients())
	{
		if (!VC || !VC->GetViewport())
		{
			continue;
		}

		FViewportClient* ReceiverClient = VC;
		EInteractionDomain Domain = GetCurrentInteractionDomain();
		if (IsPlayingInEditor()
			&& PIEControlMode == EPIEControlMode::Possessed
			&& VC == PIEEntryViewportClient
			&& GetGameViewportClient())
		{
			ReceiverClient = GetGameViewportClient();
			Domain = EInteractionDomain::PIE;
			GetGameViewportClient()->SetViewport(VC->GetViewport());
		}

		InputRouter.RegisterTarget(
			VC->GetViewport(),
			ReceiverClient,
			Domain,
			[VC](FRect& OutRect)
			{
				const FRect& R = VC->GetViewportScreenRect();
				if (R.Width <= 0.0f || R.Height <= 0.0f)
				{
					return false;
				}
				OutRect = R;
				return true;
			},
			[this]()
			{
				return GetWorld();
			});
	}

	FViewportInputContext RoutedInputContext;
	FInteractionBinding InteractionBinding;
	if (InputRouter.Tick(RoutedInputContext, InteractionBinding))
	{
		if (HandleGlobalShortcuts(RoutedInputContext))
		{
			RoutedInputContext.bConsumed = true;
		}

		for (FLevelEditorViewportClient* VC : ViewportLayout.GetLevelViewportClients())
		{
			if (VC == InteractionBinding.ReceiverVC && VC != ViewportLayout.GetActiveViewport())
			{
				ViewportLayout.SetActiveViewport(VC);
				break;
			}
		}
	}

	for (FEditorViewportClient* VC : ViewportLayout.GetAllViewportClients())
	{
		VC->Tick(DeltaTime);
	}

	WorldTick(DeltaTime);
	Render(DeltaTime);
	SelectionManager.Tick();
}

UCameraComponent* UEditorEngine::GetCamera() const
{
	if (FLevelEditorViewportClient* ActiveVC = ViewportLayout.GetActiveViewport())
	{
		return ActiveVC->GetCamera();
	}
	return nullptr;
}

void UEditorEngine::RenderUI(float DeltaTime)
{
	MainPanel.Render(DeltaTime);
}

// ─── PIE (Play In Editor) ────────────────────────────────
// UE 패턴 요약: Request는 단일 슬롯(std::optional)에 저장만 하고 즉시 실행하지 않는다.
// 실제 StartPIE는 다음 Tick 선두의 StartQueuedPlaySessionRequest에서 일어난다.
// 이유는 UI 콜백/트랜잭션 도중 같은 불안정한 타이밍을 피하기 위함.

void UEditorEngine::RequestPlaySession(const FRequestPlaySessionParams& InParams)
{
	// 동시 요청은 UE와 동일하게 덮어쓴다 (진짜 큐 아님 — 단일 슬롯).
	PlaySessionRequest = InParams;
}

void UEditorEngine::CancelRequestPlaySession()
{
	PlaySessionRequest.reset();
}

void UEditorEngine::RequestEndPlayMap()
{
	if (!PlayInEditorSessionInfo.has_value())
	{
		return;
	}
	bRequestEndPlayMapQueued = true;
}

void UEditorEngine::StartQueuedPlaySessionRequest()
{
	if (!PlaySessionRequest.has_value())
	{
		return;
	}

	const FRequestPlaySessionParams Params = *PlaySessionRequest;
	PlaySessionRequest.reset();

	// 이미 PIE 중이면 기존 세션을 정리 후 새로 시작 (단순화).
	if (PlayInEditorSessionInfo.has_value())
	{
		EndPlayMap();
	}

	switch (Params.SessionDestination)
	{
	case EPIESessionDestination::InProcess:
		StartPlayInEditorSession(Params);
		break;
	}
}

void UEditorEngine::StartPlayInEditorSession(const FRequestPlaySessionParams& Params)
{
	// 1) 현재 에디터 월드를 복제해 PIE 월드 생성 (UE의 CreatePIEWorldByDuplication 대응).
	UWorld* EditorWorld = GetWorld();
	if (!EditorWorld)
	{
		return;
	}
	UWorld* PIEWorld = Cast<UWorld>(EditorWorld->Duplicate(nullptr));
	if (!PIEWorld)
	{
		return;
	}

	// 2) PIE WorldContext를 WorldList에 등록.
	FWorldContext Ctx;
	Ctx.WorldType = EWorldType::PIE;
	Ctx.ContextHandle = FName("PIE");
	Ctx.ContextName = "PIE";
	Ctx.World = PIEWorld;
	WorldList.push_back(Ctx);

	// 3) 세션 정보 기록 (이전 활성 핸들 포함 — EndPlayMap에서 복원).
	FPlayInEditorSessionInfo Info;
	Info.OriginalRequestParams = Params;
	Info.PIEStartTime = 0.0;
	Info.PreviousActiveWorldHandle = GetActiveWorldHandle();
	if (FLevelEditorViewportClient* ActiveVC = ViewportLayout.GetActiveViewport())
	{
		if (UCameraComponent* VCCamera = ActiveVC->GetCamera())
		{
			Info.SavedViewportCamera.Location = VCCamera->GetWorldLocation();
			Info.SavedViewportCamera.Rotation = VCCamera->GetRelativeRotation();
			Info.SavedViewportCamera.CameraState = VCCamera->GetCameraState();
			Info.SavedViewportCamera.bValid = true;
		}
	}
	PlayInEditorSessionInfo = Info;
	PIEEntryViewportClient = ViewportLayout.GetActiveViewport();
	if (PIEEntryViewportClient)
	{
		bSavedEntryViewportGizmo = PIEEntryViewportClient->GetRenderOptions().ShowFlags.bGizmo;
	}

	// 4) ActiveWorldHandle을 PIE로 전환 — 이후 GetWorld()는 PIE 월드를 반환.
	SetActiveWorld(FName("PIE"));

	// GPU Occlusion readback은 ProxyId 기반이라 월드가 갈리면 stale.
	// 이전 프레임 결과를 무효화해야 wrong-proxy hit 방지.
	if (IRenderPipeline* Pipeline = GetRenderPipeline())
	{
		Pipeline->OnSceneCleared();
	}

	// 5) 활성 뷰포트 카메라를 PIE 월드의 ActiveCamera로 설정 —
	//    UWorld::UpdateVisibleProxies가 ActiveCamera를 기준으로 frustum culling을 수행하므로
	//    이를 설정하지 않으면 PIE 월드의 VisibleProxies가 비어 있어 아무것도 렌더되지 않음.
	if (FLevelEditorViewportClient* ActiveVC = ViewportLayout.GetActiveViewport())
	{
		if (UCameraComponent* VCCamera = ActiveVC->GetCamera())
		{
			PIEWorld->SetActiveCamera(VCCamera);
		}
	}

	// 6) Selection을 PIE 월드 기준으로 재바인딩 — 에디터 액터를 가리킨 채로 두면
	//    픽킹(=PIE 월드) / outliner / outline 렌더가 모두 어긋난다.
	SelectionManager.ClearSelection();
	//SelectionManager.SetGizmoEnabled(false); //PIE가 시작되면 gizmo 비활성화
	SelectionManager.SetWorld(PIEWorld);

	if (!GetGameViewportClient())
	{
		UGameViewportClient* PIEVC = UObjectManager::Get().CreateObject<UGameViewportClient>();
		SetGameViewportClient(PIEVC);
	}
	if (UGameViewportClient* PIEVC = GetGameViewportClient())
	{
		if (PIEEntryViewportClient)
		{
			PIEVC->SetDrivingCamera(PIEEntryViewportClient->GetCamera());
			PIEVC->SetViewport(PIEEntryViewportClient->GetViewport());
		}
		PIEVC->OnBeginPIE();
	}

	// PIE 시작 직후 기본 진입 모드는 항상 Possessed로 맞춘다.
	EnterPIEPossessedMode();
	
	//이 코드와 대응되는 게 아래 EndPlayMap()에 있음.
	//MainPanel.HideEditorWindowsForPIE(); //PIE 중에는 에디터 패널을 숨김.
	//ViewportLayout.DisableWorldAxisForPIE(); //PIE 중에는 월드 축 렌더링을 비활성화.

	// 7) BeginPlay 트리거 — 모든 등록/바인딩이 끝난 다음 첫 Tick 이전에 호출.
	//    UWorld::BeginPlay가 bHasBegunPlay를 먼저 세팅하므로 BeginPlay 도중
	//    SpawnActor로 만든 신규 액터도 자동으로 BeginPlay된다.
	PIEWorld->BeginPlay();
}

void UEditorEngine::EndPlayMap()
{
	if (!PlayInEditorSessionInfo.has_value())
	{
		return;
	}

	// 활성 월드를 PIE 시작 전 핸들로 복원.
	const FName PrevHandle = PlayInEditorSessionInfo->PreviousActiveWorldHandle;
	SetActiveWorld(PrevHandle);

	// 복귀한 Editor 월드의 VisibleProxies/캐시된 카메라 상태를 강제 무효화.
	// PIE 중 Editor WorldTick이 skip되어 캐시가 PIE 시작 전 시점 그대로 남아 있고,
	// NeedsVisibleProxyRebuild()가 카메라 변화 기반이라 false를 반환하면 stale
	// VisibleProxies가 그대로 재사용되어 dangling proxy 참조로 크래시가 날 수 있다.
	//
	// 또한 Renderer::PerObjectCBPool은 ProxyId로 인덱싱되는 월드 간 공유 풀이라,
	// PIE 중 PIE 프록시가 덮어쓴 슬롯이 그대로 남아 있으면 Editor 프록시의
	// bPerObjectCBDirty=false 상태로 인해 업로드가 skip되어 PIE 마지막 transform으로
	// 렌더된다. 모든 Editor 프록시를 PerObjectCB dirty로 마킹해 재업로드 강제.
	if (UWorld* EditorWorld = GetWorld())
	{
		EditorWorld->InvalidateVisibleSet();
		EditorWorld->GetScene().MarkAllPerObjectCBDirty();

		// ActiveCamera는 PIE 시작 시 PIE 월드로 옮겨졌고 PIE 월드와 함께 파괴됐다.
		// Editor 월드의 ActiveCamera는 여전히 그 dangling 포인터를 가리킬 수 있으므로
		// 활성 뷰포트의 카메라로 다시 바인딩해 줘야 frustum culling이 정상 동작한다.
		if (FLevelEditorViewportClient* ActiveVC = ViewportLayout.GetActiveViewport())
		{
			if (UCameraComponent* VCCamera = ActiveVC->GetCamera())
			{
				if (PlayInEditorSessionInfo->SavedViewportCamera.bValid)
				{
					const FPIEViewportCameraSnapshot& SavedCamera = PlayInEditorSessionInfo->SavedViewportCamera;
					VCCamera->SetWorldLocation(SavedCamera.Location);
					VCCamera->SetRelativeRotation(SavedCamera.Rotation);
					VCCamera->SetCameraState(SavedCamera.CameraState);
				}

				EditorWorld->SetActiveCamera(VCCamera);
			}
		}
	}

	// Selection을 에디터 월드로 복원 — PIE 액터는 곧 파괴되므로 먼저 비운다.
	SelectionManager.ClearSelection();
	//SelectionManager.SetGizmoEnabled(true); //PIE가 끝나면 gizmo 활성화
	SelectionManager.SetWorld(GetWorld());
	if (PIEEntryViewportClient)
	{
		PIEEntryViewportClient->GetRenderOptions().ShowFlags.bGizmo = bSavedEntryViewportGizmo;
	}
	
	//이 코드와 대응되는 게 위의 StartPlayInEditorSession()에 있음.
	//MainPanel.RestoreEditorWindowsAfterPIE();
	//ViewportLayout.RestoreWorldAxisAfterPIE();

	// PIE WorldContext 제거 (DestroyWorldContext가 EndPlay + DestroyObject 수행).
	DestroyWorldContext(FName("PIE"));

	// PIE 월드의 프록시가 모두 파괴됐으므로 GPU Occlusion readback 무효화.
	if (IRenderPipeline* Pipeline = GetRenderPipeline())
	{
		Pipeline->OnSceneCleared();
	}

	PlayInEditorSessionInfo.reset();
	if (UGameViewportClient* PIEVC = GetGameViewportClient())
	{
		PIEVC->OnEndPIE();
		UObjectManager::Get().DestroyObject(PIEVC);
		SetGameViewportClient(nullptr);
	}
	PIEEntryViewportClient = nullptr;
	PIEControlMode = EPIEControlMode::Possessed;
}

// ─── 기존 메서드 ──────────────────────────────────────────

void UEditorEngine::ResetViewport()
{
	ViewportLayout.ResetViewport(GetWorld());
}

void UEditorEngine::CloseScene()
{
	ClearScene();
}

EInteractionDomain UEditorEngine::GetCurrentInteractionDomain() const
{
	if (!IsPlayingInEditor())
	{
		return EInteractionDomain::Editor;
	}
	return (PIEControlMode == EPIEControlMode::Possessed) ? EInteractionDomain::PIE : EInteractionDomain::EditorOnPIE;
}

bool UEditorEngine::HandleGlobalShortcuts(const FViewportInputContext& InputContext)
{
	if (!IsPlayingInEditor())
	{
		return false;
	}

	for (const FInputEvent& Event : InputContext.Events)
	{
		if (Event.Type == EInputEventType::KeyPressed && Event.Key == VK_ESCAPE)
		{
			RequestEndPlayMap();
			return true;
		}
		if (Event.Type == EInputEventType::KeyPressed && Event.Key == VK_F8)
		{
			return TogglePIEControlMode();
		}
	}

	return false;
}

bool UEditorEngine::TogglePIEControlMode()
{
	if (!IsPlayingInEditor())
	{
		return false;
	}

	if (PIEControlMode == EPIEControlMode::Possessed)
	{
		return EnterPIEEjectedMode();
	}
	return EnterPIEPossessedMode();
}

bool UEditorEngine::EnterPIEPossessedMode()
{
	if (!IsPlayingInEditor())
	{
		return false;
	}

	PIEControlMode = EPIEControlMode::Possessed;
	if (PIEEntryViewportClient)
	{
		PIEEntryViewportClient->GetRenderOptions().ShowFlags.bGizmo = false;
		if (UGameViewportClient* PIEVC = GetGameViewportClient())
		{
			PIEVC->SetDrivingCamera(PIEEntryViewportClient->GetCamera());
			PIEVC->SetViewport(PIEEntryViewportClient->GetViewport());
		}
	}
	return true;
}

bool UEditorEngine::EnterPIEEjectedMode()
{
	if (!IsPlayingInEditor())
	{
		return false;
	}

	PIEControlMode = EPIEControlMode::Ejected;
	if (PIEEntryViewportClient)
	{
		PIEEntryViewportClient->GetRenderOptions().ShowFlags.bGizmo = true;
	}
	return true;
}

void UEditorEngine::NewScene()
{
	StopPlayInEditorImmediate();
	ClearScene();
	FWorldContext& Ctx = CreateWorldContext(EWorldType::Editor, FName("NewScene"), "New Scene");
	Ctx.World->InitWorld();
	SetActiveWorld(Ctx.ContextHandle);
	SelectionManager.SetWorld(GetWorld());

	ResetViewport();
}

void UEditorEngine::ClearScene()
{
	StopPlayInEditorImmediate();
	SelectionManager.ClearSelection();
	SelectionManager.SetWorld(nullptr);

	// 씬 프록시 파괴 전 GPU Occlusion 스테이징 데이터 무효화
	if (IRenderPipeline* Pipeline = GetRenderPipeline())
		Pipeline->OnSceneCleared();

	for (FWorldContext& Ctx : WorldList)
	{
		Ctx.World->EndPlay();
		UObjectManager::Get().DestroyObject(Ctx.World);
	}

	WorldList.clear();
	ActiveWorldHandle = FName::None;

	ViewportLayout.DestroyAllCameras();
}
