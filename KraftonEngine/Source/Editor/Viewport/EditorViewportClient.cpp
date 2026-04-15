#include "Editor/Viewport/EditorViewportClient.h"

#include <algorithm>

#include "Editor/UI/EditorConsoleWidget.h"
#include "Editor/Input/EditorViewportController.h"
#include "Editor/Input/EditorViewportInputMapping.h"
#include "Editor/Input/EditorViewportInputUtils.h"
#include "Editor/Subsystem/OverlayStatSystem.h"
#include "Editor/Settings/EditorSettings.h"
#include "Engine/Profiling/PlatformTime.h"
#include "Engine/Runtime/WindowsWindow.h"

#include "Components/CameraComponent.h"
#include "Components/GizmoComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Collision/RayUtils.h"
#include "Object/Object.h"
#include "Editor/Selection/SelectionManager.h"
#include "Editor/EditorEngine.h"
#include "GameFramework/AActor.h"
#include "Render/Proxy/FScene.h"
#include "Viewport/Viewport.h"
#include "GameFramework/World.h"
#include "Engine/Runtime/Engine.h"
#include "ImGui/imgui.h"
#include <cmath>

UWorld* FEditorViewportClient::GetWorld() const
{
	return GEngine ? GEngine->GetWorld() : nullptr;
}

UWorld* FEditorViewportClient::ResolveInteractionWorld() const
{
	if (bHasRoutedInputContext && RoutedInputContext.TargetWorld)
	{
		return RoutedInputContext.TargetWorld;
	}
	return GetWorld();
}

void FEditorViewportClient::Initialize(FWindowsWindow* InWindow)
{
	Window = InWindow;
}

void FEditorViewportClient::CreateCamera()
{
	DestroyCamera();
	Camera = UObjectManager::Get().CreateObject<UCameraComponent>();
}

void FEditorViewportClient::DestroyCamera()
{
	if (Camera)
	{
		UObjectManager::Get().DestroyObject(Camera);
		Camera = nullptr;
	}
}

void FEditorViewportClient::ResetCamera()
{
	if (!Camera || !Settings) return;
	Camera->SetWorldLocation(Settings->InitViewPos);
	Camera->LookAt(Settings->InitLookAt);
	SyncNavigationCameraTargetFromCurrent();
}

void FEditorViewportClient::SyncNavigationCameraTargetFromCurrent()
{
	FEditorViewportController* Controller = GetInputController();
	if (!Controller)
	{
		return;
	}

	if (FEditorNavigationTool* NavigationTool = Controller->GetNavigationTool())
	{
		NavigationTool->SyncTargetFromCameraImmediate();
	}
}

void FEditorViewportClient::SetViewportType(ELevelViewportType NewType)
{
	if (!Camera) return;

	RenderOptions.ViewportType = NewType;

	if (NewType == ELevelViewportType::Perspective)
	{
		Camera->SetOrthographic(false);
		return;
	}

	// FreeOrthographic: 현재 카메라 위치/회전 유지, 투영만 Ortho로 전환
	if (NewType == ELevelViewportType::FreeOrthographic)
	{
		Camera->SetOrthographic(true);
		return;
	}

	// 고정 방향 Orthographic: 카메라를 프리셋 방향으로 설정
	Camera->SetOrthographic(true);

	constexpr float OrthoDistance = 50.0f;
	FVector Position = FVector(0, 0, 0);
	FVector Rotation = FVector(0, 0, 0); // (Roll, Pitch, Yaw)

	switch (NewType)
	{
	case ELevelViewportType::Top:
		Position = FVector(0, 0, OrthoDistance);
		Rotation = FVector(0, 90.0f, 0);	// Pitch down (positive pitch = look -Z)
		break;
	case ELevelViewportType::Bottom:
		Position = FVector(0, 0, -OrthoDistance);
		Rotation = FVector(0, -90.0f, 0);	// Pitch up (negative pitch = look +Z)
		break;
	case ELevelViewportType::Front:
		Position = FVector(OrthoDistance, 0, 0);
		Rotation = FVector(0, 0, 180.0f);	// Yaw to look -X
		break;
	case ELevelViewportType::Back:
		Position = FVector(-OrthoDistance, 0, 0);
		Rotation = FVector(0, 0, 0.0f);		// Yaw to look +X
		break;
	case ELevelViewportType::Left:
		Position = FVector(0, -OrthoDistance, 0);
		Rotation = FVector(0, 0, 90.0f);	// Yaw to look +Y
		break;
	case ELevelViewportType::Right:
		Position = FVector(0, OrthoDistance, 0);
		Rotation = FVector(0, 0, -90.0f);	// Yaw to look -Y
		break;
	default:
		break;
	}

	Camera->SetRelativeLocation(Position);
	Camera->SetRelativeRotation(Rotation);
}

void FEditorViewportClient::SetViewportSize(float InWidth, float InHeight)
{
	if (InWidth > 0.0f)
	{
		WindowWidth = InWidth;
	}

	if (InHeight > 0.0f)
	{
		WindowHeight = InHeight;
	}

	if (Camera)
	{
		Camera->OnResize(static_cast<int32>(WindowWidth), static_cast<int32>(WindowHeight));
	}
}

void FEditorViewportClient::Tick(float DeltaTime)
{
	if (bPIEOutlineFlashActive)
	{
		PIEOutlineFlashElapsed += DeltaTime;
		const float TotalDuration = PIEOutlineFlashHoldDuration + PIEOutlineFlashFadeDuration;
		if (PIEOutlineFlashElapsed >= TotalDuration)
		{
			bPIEOutlineFlashActive = false;
			PIEOutlineFlashElapsed = 0.0f;
		}
	}

	if (!bIsActive || !bHasRoutedInputContext)
	{
		return;
	}

	GetInputController();
	EnsureInputContextStack();
	const bool bHasEvents = !RoutedInputContext.Events.empty();
	bool bConsumedByContext = false;
	for (IEditorViewportInputContext* Context : InputContextStack)
	{
		if (Context && Context->HandleInput(DeltaTime))
		{
			bConsumedByContext = true;
			if (bHasEvents)
			{
				const char* ContextName = "Unknown";
				if (Context == CommandInputContext.get()) { ContextName = "Command"; }
				else if (Context == GizmoInputContext.get()) { ContextName = "Gizmo"; }
				else if (Context == SelectionInputContext.get()) { ContextName = "Selection"; }
				else if (Context == NavigationInputContext.get()) { ContextName = "Navigation"; }

				// UE_LOG(
				// 	"[InputTrace] Domain=%d Context=%s Consumed=1 Events=%d Hover=%d Focus=%d Capture=%d Alt=%d Ctrl=%d Shift=%d",
				// 	static_cast<int32>(RoutedInputContext.Domain),
				// 	ContextName,
				// 	static_cast<int32>(RoutedInputContext.Events.size()),
				// 	RoutedInputContext.bHovered ? 1 : 0,
				// 	RoutedInputContext.bFocused ? 1 : 0,
				// 	RoutedInputContext.bCaptured ? 1 : 0,
				// 	RoutedInputContext.Frame.IsDown(VK_MENU) ? 1 : 0,
				// 	RoutedInputContext.Frame.IsDown(VK_CONTROL) ? 1 : 0,
				// 	RoutedInputContext.Frame.IsDown(VK_SHIFT) ? 1 : 0);
			}
			break;
		}
	}
	if (bHasEvents && !bConsumedByContext)
	{
		// UE_LOG(
		// 	"[InputTrace] Domain=%d Context=None Consumed=0 Events=%d Hover=%d Focus=%d Capture=%d Alt=%d Ctrl=%d Shift=%d",
		// 	static_cast<int32>(RoutedInputContext.Domain),
		// 	static_cast<int32>(RoutedInputContext.Events.size()),
		// 	RoutedInputContext.bHovered ? 1 : 0,
		// 	RoutedInputContext.bFocused ? 1 : 0,
		// 	RoutedInputContext.bCaptured ? 1 : 0,
		// 	RoutedInputContext.Frame.IsDown(VK_MENU) ? 1 : 0,
		// 	RoutedInputContext.Frame.IsDown(VK_CONTROL) ? 1 : 0,
		// 	RoutedInputContext.Frame.IsDown(VK_SHIFT) ? 1 : 0);
	}

	bHasRoutedInputContext = false;
}

FEditorViewportController* FEditorViewportClient::GetInputController()
{
	if (!InputController)
	{
		InputController = std::make_unique<FEditorViewportController>(this);
	}
	return InputController.get();
}

bool FEditorViewportClient::SetInteractionMode(EEditorViewportModeType InModeType)
{
	FEditorViewportController* Controller = GetInputController();
	return Controller ? Controller->SetMode(InModeType) : false;
}

EEditorViewportModeType FEditorViewportClient::GetInteractionMode() const
{
	if (!InputController)
	{
		return EEditorViewportModeType::Select;
	}
	return InputController->GetMode();
}

bool FEditorViewportClient::CycleInteractionMode()
{
	FEditorViewportController* Controller = GetInputController();
	return Controller ? Controller->CycleMode() : false;
}

void FEditorViewportClient::EnsureInputContextStack()
{
	if (bInputContextStackInitialized)
	{
		return;
	}

	CommandInputContext = std::make_unique<FEditorViewportCommandContext>(this);
	GizmoInputContext = std::make_unique<FEditorViewportGizmoContext>(this);
	SelectionInputContext = std::make_unique<FEditorViewportSelectionContext>(this);
	NavigationInputContext = std::make_unique<FEditorViewportNavigationContext>(this);

	InputContextStack.clear();
	InputContextStack.push_back(CommandInputContext.get());
	InputContextStack.push_back(GizmoInputContext.get());
	InputContextStack.push_back(SelectionInputContext.get());
	InputContextStack.push_back(NavigationInputContext.get());

	std::sort(
		InputContextStack.begin(),
		InputContextStack.end(),
		[](IEditorViewportInputContext* Lhs, IEditorViewportInputContext* Rhs)
		{
			if (!Lhs || !Rhs)
			{
				return Lhs != nullptr;
			}
			return Lhs->GetPriority() > Rhs->GetPriority();
		});

	bInputContextStackInitialized = true;
}

bool FEditorViewportClient::HandleCommandInput(float DeltaTime)
{
	FEditorViewportController* Controller = GetInputController();
	return Controller ? Controller->HandleViewportCommandInput(DeltaTime) : false;
}

bool FEditorViewportClient::HandleNavigationInput(float DeltaTime)
{
	FEditorViewportController* Controller = GetInputController();
	return Controller ? Controller->HandleNavigationInput(DeltaTime) : false;
}

bool FEditorViewportClient::HandleGizmoInput(float DeltaTime)
{
	FEditorViewportController* Controller = GetInputController();
	return Controller ? Controller->HandleGizmoInput(DeltaTime) : false;
}

bool FEditorViewportClient::HandleSelectionInput(float DeltaTime)
{
	FEditorViewportController* Controller = GetInputController();
	return Controller ? Controller->HandleSelectionInput(DeltaTime) : false;
}

bool FEditorViewportClient::ProcessInput(FViewportInputContext& Context)
{
	RoutedInputContext = Context;
	bHasRoutedInputContext = true;
	return false;
}

bool FEditorViewportClient::ConvertScreenToViewportLocal(const POINT& InScreenPos, POINT& OutLocal, bool bClampToViewport) const
{
	const FRect& ViewRect = GetViewportScreenRect();
	if (ViewRect.Width <= 0.0f || ViewRect.Height <= 0.0f)
	{
		return false;
	}

	POINT ClientPos = InScreenPos;
	if (Window)
	{
		ScreenToClient(Window->GetHWND(), &ClientPos);
	}

	const float LocalX = static_cast<float>(ClientPos.x) - ViewRect.X;
	const float LocalY = static_cast<float>(ClientPos.y) - ViewRect.Y;
	if (!bClampToViewport)
	{
		OutLocal.x = static_cast<LONG>(LocalX);
		OutLocal.y = static_cast<LONG>(LocalY);
		return true;
	}

	OutLocal.x = static_cast<LONG>(Clamp(LocalX, 0.0f, (std::max)(0.0f, ViewRect.Width - 1.0f)));
	OutLocal.y = static_cast<LONG>(Clamp(LocalY, 0.0f, (std::max)(0.0f, ViewRect.Height - 1.0f)));
	return true;
}

bool FEditorViewportClient::PickActorByIdAtLocalPoint(const POINT& InLocalPoint, AActor*& OutActor) const
{
	OutActor = nullptr;
	UWorld* InteractionWorld = ResolveInteractionWorld();
	if (!InteractionWorld || !Viewport || !GEngine)
	{
		return false;
	}

	ID3D11DeviceContext* Context = GEngine->GetRenderer().GetFD3DDevice().GetDeviceContext();
	if (!Context)
	{
		return false;
	}

	uint32 PickedId = 0;
	const uint32 PixelX = (InLocalPoint.x >= 0) ? static_cast<uint32>(InLocalPoint.x) : 0u;
	const uint32 PixelY = (InLocalPoint.y >= 0) ? static_cast<uint32>(InLocalPoint.y) : 0u;
	if (!Viewport->ReadIdPickAt(PixelX, PixelY, Context, PickedId) || PickedId == 0u)
	{
		return false;
	}

	const uint32 ProxyId = PickedId - 1u;
	const TArray<FPrimitiveSceneProxy*>& Proxies = InteractionWorld->GetScene().GetAllProxies();
	if (ProxyId >= static_cast<uint32>(Proxies.size()))
	{
		return false;
	}

	FPrimitiveSceneProxy* Proxy = Proxies[ProxyId];
	if (!Proxy || !Proxy->Owner || !Proxy->Owner->GetOwner())
	{
		return false;
	}

	AActor* PickedActor = Proxy->Owner->GetOwner();
	if (!PickedActor->IsVisible() || PickedActor->GetWorld() != InteractionWorld)
	{
		return false;
	}

	OutActor = PickedActor;
	return true;
}

bool FEditorViewportClient::WantsRelativeMouseMode(const FViewportInputContext& Context, POINT& OutRestoreScreenPos) const
{
	OutRestoreScreenPos = Context.Frame.MouseScreenPos;
	if (!Camera || !Viewport)
	{
		return false;
	}
	// ImGui가 마우스를 캡처 중인 동안에는 상대마우스 모드 진입/유지를 금지한다.
	// (UI 클릭/드래그는 정상 동작하되, 커서 중앙 고정/복귀로 인한 간섭을 막는다.)
	if (Context.bImGuiCapturedMouse)
	{
		return false;
	}
	if (EditorViewportInputUtils::IsInViewportToolbarDeadZone(Context))
	{
		return false;
	}

	bool bGizmoBlocksLeftRelativeDrag = Gizmo && (Gizmo->IsHolding() || Gizmo->IsPressedOnHandle() || Gizmo->IsHovered());
	const bool bGizmoInteractionActive = Gizmo && (Gizmo->IsHolding() || Gizmo->IsPressedOnHandle());
	if (bGizmoInteractionActive)
	{
		return false;
	}
	const bool bLeftLookChord = Context.Frame.IsDown(VK_LBUTTON) && !Context.Frame.IsAltDown() && !Context.Frame.IsCtrlDown();

	// LMB 네비게이션 relative 유지 중에는 gizmo hover 재검사로 모드가 흔들리지 않도록 유지 우선.
	if (Context.bRelativeMouseMode && bLeftLookChord)
	{
		return true;
	}

	if (!bGizmoBlocksLeftRelativeDrag
		&& Gizmo
		&& Context.Frame.IsDown(VK_LBUTTON))
	{
		const float LocalMouseX = static_cast<float>(Context.MouseLocalPos.x);
		const float LocalMouseY = static_cast<float>(Context.MouseLocalPos.y);
		const float VPWidth = static_cast<float>(Viewport->GetWidth());
		const float VPHeight = static_cast<float>(Viewport->GetHeight());
		const FRay MouseRay = Camera->DeprojectScreenToWorld(LocalMouseX, LocalMouseY, VPWidth, VPHeight);
		FHitResult GizmoHit{};
		bGizmoBlocksLeftRelativeDrag = Gizmo->LineTraceComponent(MouseRay, GizmoHit);
	}

	const bool bLeftRelativeDrag = EditorViewportInputUtils::IsLeftNavigationDragActive(Context) && !bGizmoBlocksLeftRelativeDrag;
	const bool bRightHeld = EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::NavLookRightDown);
	const bool bRightLookDrag = bRightHeld
		&& (Context.Frame.bRightDragging || Context.WasPointerDragStarted(EPointerButton::Right) || Context.bRelativeMouseMode);
	const bool bMouseOwnedByViewport = Context.bCaptured || Context.bHovered || Context.bRelativeMouseMode;
	if (!bMouseOwnedByViewport)
	{
		return false;
	}

	return bRightLookDrag
		|| EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::NavLookMiddleDown)
		|| EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::NavOrbitAltLeftDown)
		|| EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::NavDollyAltRightDown)
		|| EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::NavPanAltMiddleDown)
		|| bLeftRelativeDrag;
}

bool FEditorViewportClient::WantsAbsoluteMouseClip(const FViewportInputContext& Context, RECT& OutClipScreenRect) const
{
	OutClipScreenRect = { 0, 0, 0, 0 };
	if (!Gizmo || !Context.Frame.IsDown(VK_LBUTTON))
	{
		return false;
	}

	// Gizmo 드래그(또는 핸들 press 직후) 중에는 커서가 viewport 밖으로 빠져나가
	// drag state가 흔들리는 것을 방지하기 위해 viewport rect로 커서를 제한한다.
	if (!(Gizmo->IsHolding() || Gizmo->IsPressedOnHandle()))
	{
		return false;
	}

	const FRect& R = ViewportScreenRect;
	if (R.Width <= 1.0f || R.Height <= 1.0f || !Window)
	{
		return false;
	}

	POINT TopLeft =
	{
		static_cast<LONG>(R.X),
		static_cast<LONG>(R.Y)
	};
	POINT BottomRight =
	{
		static_cast<LONG>(R.X + R.Width),
		static_cast<LONG>(R.Y + R.Height)
	};
	POINT ClientTopLeft = { 0, 0 };
	::ClientToScreen(Window->GetHWND(), &TopLeft);
	::ClientToScreen(Window->GetHWND(), &BottomRight);
	::ClientToScreen(Window->GetHWND(), &ClientTopLeft);

	OutClipScreenRect.left = TopLeft.x;
	OutClipScreenRect.top =
		(TopLeft.y + EditorViewportInputUtils::PaneToolbarHeight > ClientTopLeft.y)
		? (TopLeft.y + EditorViewportInputUtils::PaneToolbarHeight)
		: ClientTopLeft.y;
	OutClipScreenRect.right = BottomRight.x;
	OutClipScreenRect.bottom = BottomRight.y;
	return true;
}

void FEditorViewportClient::UpdateLayoutRect()
{
	if (!LayoutWindow) return;

	const FRect& R = LayoutWindow->GetRect();
	ViewportScreenRect = R;

	// FViewport 리사이즈 요청 (슬롯 크기와 RT 크기 동기화)
	if (Viewport)
	{
		const float SafeWidth = (R.Width > 1.0f) ? R.Width : 1.0f;
		const float SafeHeight = (R.Height > 1.0f) ? R.Height : 1.0f;
		uint32 SlotW = static_cast<uint32>(std::lround(SafeWidth));
		uint32 SlotH = static_cast<uint32>(std::lround(SafeHeight));
		if (SlotW > 0 && SlotH > 0 && (SlotW != Viewport->GetWidth() || SlotH != Viewport->GetHeight()))
		{
			Viewport->RequestResize(SlotW, SlotH);
		}
	}
}

void FEditorViewportClient::RenderViewportImage(bool bIsActiveViewport, bool bDrawActiveOutline)
{
	if (!Viewport || !Viewport->GetSRV()) return;

	const FRect& R = ViewportScreenRect;
	if (R.Width <= 0 || R.Height <= 0) return;

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	ImVec2 Min(R.X, R.Y);
	ImVec2 Max(R.X + R.Width, R.Y + R.Height);
	constexpr float ToolbarBorderOffsetY = 34.0f;
	const ImVec2 OutlineMin(R.X, R.Y + ToolbarBorderOffsetY);

	DrawList->AddImage((ImTextureID)Viewport->GetSRV(), Min, Max);

	// 활성 뷰포트 테두리 강조
	if (bIsActiveViewport && bDrawActiveOutline)
	{
		DrawList->AddRect(OutlineMin, Max, IM_COL32(255, 200, 0, 200), 0.0f, 0, 2.0f);
	}

	if (bPIEOutlineFlashActive && PIEOutlineFlashFadeDuration > 0.0f)
	{
		float Alpha01 = 1.0f;
		if (PIEOutlineFlashElapsed > PIEOutlineFlashHoldDuration)
		{
			const float FadeElapsed = PIEOutlineFlashElapsed - PIEOutlineFlashHoldDuration;
			Alpha01 = 1.0f - Clamp(FadeElapsed / PIEOutlineFlashFadeDuration, 0.0f, 1.0f);
		}
		const int32 Alpha = static_cast<int32>(Alpha01 * 255.0f);
		DrawList->AddRect(OutlineMin, Max, IM_COL32(80, 255, 120, Alpha), 0.0f, 0, 3.0f);
	}

	POINT MarqueeStart = { 0, 0 };
	POINT MarqueeCurrent = { 0, 0 };
	bool bMarqueeAdditive = false;
	const bool bHasMarquee = InputController && InputController->GetSelectionMarquee(MarqueeStart, MarqueeCurrent, bMarqueeAdditive);
	if (bHasMarquee)
	{
		const float StartX = R.X + static_cast<float>(MarqueeStart.x);
		const float StartY = R.Y + static_cast<float>(MarqueeStart.y);
		const float CurrentX = R.X + static_cast<float>(MarqueeCurrent.x);
		const float CurrentY = R.Y + static_cast<float>(MarqueeCurrent.y);
		const float Left = (std::min)(StartX, CurrentX);
		const float Top = (std::min)(StartY, CurrentY);
		const float Right = (std::max)(StartX, CurrentX);
		const float Bottom = (std::max)(StartY, CurrentY);
		DrawList->AddRectFilled(ImVec2(Left, Top), ImVec2(Right, Bottom), IM_COL32(255, 255, 255, 48));
		DrawList->AddRect(ImVec2(Left, Top), ImVec2(Right, Bottom), IM_COL32(255, 255, 255, 210), 0.0f, 0, 1.5f);
	}
}

void FEditorViewportClient::TriggerPIEStartOutlineFlash(float HoldSeconds, float FadeSeconds)
{
	PIEOutlineFlashHoldDuration = HoldSeconds > 0.0f ? HoldSeconds : 1.0f;
	PIEOutlineFlashFadeDuration = FadeSeconds > 0.0f ? FadeSeconds : 2.0f;
	PIEOutlineFlashElapsed = 0.0f;
	bPIEOutlineFlashActive = true;
}

void FEditorViewportClient::ClearPIEStartOutlineFlash()
{
	bPIEOutlineFlashActive = false;
	PIEOutlineFlashElapsed = 0.0f;
}
