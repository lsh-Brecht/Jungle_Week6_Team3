#include "Editor/Input/EditorSelectionTool.h"

#include <algorithm>
#include <cmath>

#include "Editor/Input/EditorViewportInputMapping.h"
#include "Editor/Input/EditorViewportInputUtils.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "Editor/Selection/SelectionManager.h"
#include "Component/CameraComponent.h"
#include "Component/GizmoComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Math/Matrix.h"
#include "Math/MathUtils.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Viewport/Viewport.h"

FEditorSelectionTool::FEditorSelectionTool(FEditorViewportClient* InOwner)
	: Owner(InOwner)
{
}

bool FEditorSelectionTool::HandleInput(float DeltaTime)
{
	(void)DeltaTime;
	UWorld* InteractionWorld = Owner ? Owner->ResolveInteractionWorld() : nullptr;
	if (!Owner || !Owner->GetCamera() || !Owner->GetGizmo() || !InteractionWorld || !Owner->GetSelectionManager())
	{
		return false;
	}

	const FViewportInputContext& Context = Owner->GetRoutedInputContext();
	if (EditorViewportInputUtils::IsInViewportToolbarDeadZone(Context))
	{
		return false;
	}
	if (Context.bImGuiCapturedMouse && !Context.bCaptured && !Context.bHovered)
	{
		return false;
	}

	auto GetLocalFromScreenEvent = [this](const POINT& InScreenPos, POINT& OutLocal) -> bool
	{
		if (!Owner)
		{
			return false;
		}

		POINT ClientPos = InScreenPos;
		if (Owner->Window)
		{
			ScreenToClient(Owner->Window->GetHWND(), &ClientPos);
		}

		const FRect& ViewRect = Owner->GetViewportScreenRect();
		if (ViewRect.Width <= 0.0f || ViewRect.Height <= 0.0f)
		{
			return false;
		}

		OutLocal.x = static_cast<LONG>(Clamp(
			static_cast<float>(ClientPos.x) - ViewRect.X,
			0.0f,
			(std::max)(0.0f, ViewRect.Width - 1.0f)));
		OutLocal.y = static_cast<LONG>(Clamp(
			static_cast<float>(ClientPos.y) - ViewRect.Y,
			0.0f,
			(std::max)(0.0f, ViewRect.Height - 1.0f)));
		return true;
	};

	for (const FInputEvent& Event : Context.Events)
	{
		if (Event.Type == EInputEventType::KeyPressed && Event.Key == VK_LBUTTON)
		{
			POINT PressLocal = Context.MouseLocalPos;
			if (GetLocalFromScreenEvent(Event.MouseScreenPos, PressLocal))
			{
				PendingSelectionPressLocal = PressLocal;
			}
			else
			{
				PendingSelectionPressLocal = Context.MouseLocalPos;
			}
			bHasPendingSelectionPress = true;
			break;
		}
	}

	bool bBoxAdditive = false;
	const bool bBoxSelectDown = IsBoxSelectionChordDown(bBoxAdditive);
	if (bBoxSelectDown)
	{
		const POINT CurrentLocal = Context.MouseLocalPos;
		if (!bSelectionMarqueeActive)
		{
			BeginSelectionMarquee(CurrentLocal, bBoxAdditive);
		}
		else
		{
			UpdateSelectionMarquee(CurrentLocal);
		}

		if (Context.WasPointerDragStarted(EPointerButton::Left))
		{
			bHasPendingSelectionPress = false;
			return true;
		}
	}

	if (bSelectionMarqueeActive)
	{
		const bool bBoxReleased =
			Context.WasPointerDragEnded(EPointerButton::Left) || !bBoxSelectDown;
		if (bBoxReleased)
		{
			ApplySelectionMarquee(bSelectionMarqueeAdditive);
			EndSelectionMarquee();
			bHasPendingSelectionPress = false;
			return true;
		}
		return true;
	}

	const bool bSelectionClickTriggered =
		EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::SelectPrimaryReleased)
		|| EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::SelectToggleReleased)
		|| EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::SelectAddReleased);
	if (!bSelectionClickTriggered)
	{
		return false;
	}
	const bool bLeftNavigationDragActive = EditorViewportInputUtils::IsLeftNavigationDragActive(Context);
	if (bLeftNavigationDragActive)
	{
		bHasPendingSelectionPress = false;
		return false;
	}
	if (Context.WasPointerDragEnded(EPointerButton::Left))
	{
		bHasPendingSelectionPress = false;
		return false;
	}

	POINT ClickLocal = Context.MouseLocalPos;
	if (bHasPendingSelectionPress)
	{
		ClickLocal = PendingSelectionPressLocal;
	}
	float LocalMouseX = static_cast<float>(ClickLocal.x);
	float LocalMouseY = static_cast<float>(ClickLocal.y);
	float VPWidth = Owner->GetViewport() ? static_cast<float>(Owner->GetViewport()->GetWidth()) : Owner->GetWindowWidth();
	float VPHeight = Owner->GetViewport() ? static_cast<float>(Owner->GetViewport()->GetHeight()) : Owner->GetWindowHeight();
	const FRay Ray = Owner->GetCamera()->DeprojectScreenToWorld(LocalMouseX, LocalMouseY, VPWidth, VPHeight);
	HandleSelectionClick(Ray);
	bHasPendingSelectionPress = false;
	return true;
}

bool FEditorSelectionTool::IsBoxSelectionChordDown(bool& bOutAdditive) const
{
	bOutAdditive = false;
	if (!Owner)
	{
		return false;
	}

	const FViewportInputContext& Context = Owner->GetRoutedInputContext();
	if (EditorViewportInputMapping::IsTriggered(
		Context,
		EditorViewportInputMapping::EEditorViewportAction::BoxSelectAdditiveDown))
	{
		bOutAdditive = true;
		return true;
	}

	return EditorViewportInputMapping::IsTriggered(
		Context,
		EditorViewportInputMapping::EEditorViewportAction::BoxSelectReplaceDown);
}

void FEditorSelectionTool::HandleSelectionClick(const FRay& Ray)
{
	if (!Owner || !Owner->GetSelectionManager())
	{
		return;
	}

	UWorld* InteractionWorld = Owner->ResolveInteractionWorld();
	if (!InteractionWorld)
	{
		return;
	}

	const FViewportInputContext& Context = Owner->GetRoutedInputContext();
	const bool bCtrlHeld = Context.Frame.IsCtrlDown();
	const bool bShiftHeld = Context.Frame.IsShiftDown();

	FHitResult HitResult{};
	AActor* BestActor = nullptr;
	InteractionWorld->RaycastPrimitives(Ray, HitResult, BestActor);
	if (!BestActor)
	{
		if (!bCtrlHeld && !bShiftHeld)
		{
			Owner->GetSelectionManager()->ClearSelection();
		}
		return;
	}

	if (bCtrlHeld)
	{
		Owner->GetSelectionManager()->ToggleSelect(BestActor);
	}
	else if (bShiftHeld)
	{
		Owner->GetSelectionManager()->AddSelect(BestActor);
	}
	else
	{
		Owner->GetSelectionManager()->Select(BestActor);
	}
}

bool FEditorSelectionTool::TryProjectActorToViewportLocal(AActor* Actor, float& OutX, float& OutY) const
{
	if (!Owner || !Owner->GetCamera() || !Owner->GetViewport() || !Actor)
	{
		return false;
	}

	const FVector WorldPos = Actor->GetActorLocation();
	const FMatrix ViewProjection = Owner->GetCamera()->GetViewMatrix() * Owner->GetCamera()->GetProjectionMatrix();

	const float ClipX = WorldPos.X * ViewProjection.M[0][0] + WorldPos.Y * ViewProjection.M[1][0] + WorldPos.Z * ViewProjection.M[2][0] + ViewProjection.M[3][0];
	const float ClipY = WorldPos.X * ViewProjection.M[0][1] + WorldPos.Y * ViewProjection.M[1][1] + WorldPos.Z * ViewProjection.M[2][1] + ViewProjection.M[3][1];
	const float ClipZ = WorldPos.X * ViewProjection.M[0][2] + WorldPos.Y * ViewProjection.M[1][2] + WorldPos.Z * ViewProjection.M[2][2] + ViewProjection.M[3][2];
	const float ClipW = WorldPos.X * ViewProjection.M[0][3] + WorldPos.Y * ViewProjection.M[1][3] + WorldPos.Z * ViewProjection.M[2][3] + ViewProjection.M[3][3];
	if (std::abs(ClipW) < 1e-6f)
	{
		return false;
	}

	const float InvW = 1.0f / ClipW;
	const float NdcX = ClipX * InvW;
	const float NdcY = ClipY * InvW;
	const float NdcZ = ClipZ * InvW;
	if (NdcX < -1.0f || NdcX > 1.0f || NdcY < -1.0f || NdcY > 1.0f || NdcZ < 0.0f || NdcZ > 1.0f)
	{
		return false;
	}

	const float Width = static_cast<float>(Owner->GetViewport()->GetWidth());
	const float Height = static_cast<float>(Owner->GetViewport()->GetHeight());
	if (Width <= 0.0f || Height <= 0.0f)
	{
		return false;
	}

	OutX = (NdcX * 0.5f + 0.5f) * Width;
	OutY = (1.0f - (NdcY * 0.5f + 0.5f)) * Height;
	return true;
}

void FEditorSelectionTool::ApplySelectionMarquee(bool bAdditive)
{
	if (!Owner || !Owner->GetSelectionManager() || !Owner->ResolveInteractionWorld() || !bSelectionMarqueeActive)
	{
		return;
	}

	const POINT& Start = SelectionMarqueeStartLocal;
	const POINT& Current = SelectionMarqueeCurrentLocal;
	const float Left = static_cast<float>((std::min)(Start.x, Current.x));
	const float Right = static_cast<float>((std::max)(Start.x, Current.x));
	const float Top = static_cast<float>((std::min)(Start.y, Current.y));
	const float Bottom = static_cast<float>((std::max)(Start.y, Current.y));
	if ((Right - Left) < 2.0f || (Bottom - Top) < 2.0f)
	{
		return;
	}

	TArray<AActor*> Hits;
	const TArray<AActor*>& Actors = Owner->ResolveInteractionWorld()->GetActors();
	for (AActor* Actor : Actors)
	{
		if (!Actor || !Actor->IsVisible())
		{
			continue;
		}

		float ScreenX = 0.0f;
		float ScreenY = 0.0f;
		if (!TryProjectActorToViewportLocal(Actor, ScreenX, ScreenY))
		{
			continue;
		}

		if (ScreenX >= Left && ScreenX <= Right && ScreenY >= Top && ScreenY <= Bottom)
		{
			Hits.push_back(Actor);
		}
	}

	if (!bAdditive)
	{
		Owner->GetSelectionManager()->ClearSelection();
	}

	for (AActor* HitActor : Hits)
	{
		Owner->GetSelectionManager()->AddSelect(HitActor);
	}
}

bool FEditorSelectionTool::GetSelectionMarquee(POINT& OutStart, POINT& OutCurrent, bool& bOutAdditive) const
{
	if (!bSelectionMarqueeActive)
	{
		OutStart = { 0, 0 };
		OutCurrent = { 0, 0 };
		bOutAdditive = false;
		return false;
	}

	OutStart = SelectionMarqueeStartLocal;
	OutCurrent = SelectionMarqueeCurrentLocal;
	bOutAdditive = bSelectionMarqueeAdditive;
	return true;
}

void FEditorSelectionTool::BeginSelectionMarquee(const POINT& InLocalStart, bool bInAdditive)
{
	bSelectionMarqueeActive = true;
	bSelectionMarqueeAdditive = bInAdditive;
	SelectionMarqueeStartLocal = InLocalStart;
	SelectionMarqueeCurrentLocal = InLocalStart;
}

void FEditorSelectionTool::UpdateSelectionMarquee(const POINT& InLocalCurrent)
{
	if (!bSelectionMarqueeActive)
	{
		return;
	}

	SelectionMarqueeCurrentLocal = InLocalCurrent;
}

void FEditorSelectionTool::EndSelectionMarquee()
{
	bSelectionMarqueeActive = false;
	bSelectionMarqueeAdditive = false;
}

