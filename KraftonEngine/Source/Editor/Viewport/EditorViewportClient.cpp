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

#include "Component/CameraComponent.h"
#include "Component/GizmoComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Collision/RayUtils.h"
#include "Object/Object.h"
#include "Editor/Selection/SelectionManager.h"
#include "Editor/EditorEngine.h"
#include "GameFramework/AActor.h"
#include "Viewport/Viewport.h"
#include "GameFramework/World.h"
#include "Engine/Runtime/Engine.h"
#include "ImGui/imgui.h"

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

				UE_LOG(
					"[InputTrace] Domain=%d Context=%s Consumed=1 Events=%d Hover=%d Focus=%d Capture=%d Alt=%d Ctrl=%d Shift=%d",
					static_cast<int32>(RoutedInputContext.Domain),
					ContextName,
					static_cast<int32>(RoutedInputContext.Events.size()),
					RoutedInputContext.bHovered ? 1 : 0,
					RoutedInputContext.bFocused ? 1 : 0,
					RoutedInputContext.bCaptured ? 1 : 0,
					RoutedInputContext.Frame.IsDown(VK_MENU) ? 1 : 0,
					RoutedInputContext.Frame.IsDown(VK_CONTROL) ? 1 : 0,
					RoutedInputContext.Frame.IsDown(VK_SHIFT) ? 1 : 0);
			}
			break;
		}
	}
	if (bHasEvents && !bConsumedByContext)
	{
		UE_LOG(
			"[InputTrace] Domain=%d Context=None Consumed=0 Events=%d Hover=%d Focus=%d Capture=%d Alt=%d Ctrl=%d Shift=%d",
			static_cast<int32>(RoutedInputContext.Domain),
			static_cast<int32>(RoutedInputContext.Events.size()),
			RoutedInputContext.bHovered ? 1 : 0,
			RoutedInputContext.bFocused ? 1 : 0,
			RoutedInputContext.bCaptured ? 1 : 0,
			RoutedInputContext.Frame.IsDown(VK_MENU) ? 1 : 0,
			RoutedInputContext.Frame.IsDown(VK_CONTROL) ? 1 : 0,
			RoutedInputContext.Frame.IsDown(VK_SHIFT) ? 1 : 0);
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

bool FEditorViewportClient::WantsRelativeMouseMode(const FViewportInputContext& Context, POINT& OutRestoreScreenPos) const
{
	OutRestoreScreenPos = Context.Frame.MouseScreenPos;
	if (!Camera || !Viewport)
	{
		return false;
	}

	bool bGizmoBlocksLeftRelativeDrag = Gizmo && (Gizmo->IsHolding() || Gizmo->IsPressedOnHandle());
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
	const bool bMouseOwnedByViewport = Context.bCaptured || Context.bHovered || Context.bRelativeMouseMode;
	if (!bMouseOwnedByViewport)
	{
		return false;
	}

	// ImGui가 마우스를 캡처 중이면 상대 마우스 신규 진입을 허용하지 않는다.
	// 이미 relative 모드로 들어간 상태에서만 유지를 허용.
	if (Context.bImGuiCapturedMouse && !Context.bRelativeMouseMode)
	{
		return false;
	}

	return EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::NavLookRightDown)
		|| EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::NavLookMiddleDown)
		|| EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::NavOrbitAltLeftDown)
		|| EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::NavDollyAltRightDown)
		|| EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::NavPanAltMiddleDown)
		|| bLeftRelativeDrag;
}

void FEditorViewportClient::UpdateLayoutRect()
{
	if (!LayoutWindow) return;

	const FRect& R = LayoutWindow->GetRect();
	ViewportScreenRect = R;

	// FViewport 리사이즈 요청 (슬롯 크기와 RT 크기 동기화)
	if (Viewport)
	{
		uint32 SlotW = static_cast<uint32>(R.Width);
		uint32 SlotH = static_cast<uint32>(R.Height);
		if (SlotW > 0 && SlotH > 0 && (SlotW != Viewport->GetWidth() || SlotH != Viewport->GetHeight()))
		{
			Viewport->RequestResize(SlotW, SlotH);
		}
	}
}

void FEditorViewportClient::RenderViewportImage(bool bIsActiveViewport)
{
	if (!Viewport || !Viewport->GetSRV()) return;

	const FRect& R = ViewportScreenRect;
	if (R.Width <= 0 || R.Height <= 0) return;

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	ImVec2 Min(R.X, R.Y);
	ImVec2 Max(R.X + R.Width, R.Y + R.Height);

	DrawList->AddImage((ImTextureID)Viewport->GetSRV(), Min, Max);

	// 활성 뷰포트 테두리 강조
	if (bIsActiveViewport)
	{
		DrawList->AddRect(Min, Max, IM_COL32(255, 200, 0, 200), 0.0f, 0, 2.0f);
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
