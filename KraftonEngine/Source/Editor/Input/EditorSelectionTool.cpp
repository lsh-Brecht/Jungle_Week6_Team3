#include "Editor/Input/EditorSelectionTool.h"

#include "Editor/Viewport/EditorViewportClient.h"
#include "Editor/Selection/SelectionManager.h"
#include "Editor/Settings/EditorSettings.h"
#include "Component/CameraComponent.h"
#include "Component/GizmoComponent.h"
#include "Collision/RayUtils.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Viewport/Viewport.h"

namespace
{
bool HasKeyEvent(const FViewportInputContext& Context, EInputEventType Type, int32 Key)
{
	for (const FInputEvent& Event : Context.Events)
	{
		if (Event.Type == Type && Event.Key == Key)
		{
			return true;
		}
	}
	return false;
}
}

FEditorSelectionTool::FEditorSelectionTool(FEditorViewportClient* InOwner)
	: Owner(InOwner)
{
}

bool FEditorSelectionTool::HandleInput(float DeltaTime)
{
	UWorld* InteractionWorld = Owner ? Owner->ResolveInteractionWorld() : nullptr;
	if (!Owner || !Owner->GetCamera() || !Owner->GetGizmo() || !InteractionWorld || !Owner->GetSelectionManager())
	{
		return false;
	}

	const FViewportInputContext& Context = Owner->GetRoutedInputContext();
	UCameraComponent* Camera = Owner->GetCamera();
	UGizmoComponent* Gizmo = Owner->GetGizmo();

	bool bConsumed = false;
	const FEditorSettings* Settings = Owner->GetSettings();
	const float ZoomSpeed = Settings ? Settings->CameraZoomSpeed : 300.f;
	const float ScrollNotches = Context.Frame.WheelNotches;
	if (ScrollNotches != 0.0f)
	{
		if (Camera->IsOrthogonal())
		{
			float NewWidth = Camera->GetOrthoWidth() - ScrollNotches * ZoomSpeed * DeltaTime;
			Camera->SetOrthoWidth(Clamp(NewWidth, 0.1f, 1000.0f));
		}
		else
		{
			Camera->MoveLocal(FVector(ScrollNotches * ZoomSpeed * 0.015f, 0.0f, 0.0f));
		}
		bConsumed = true;
	}

	if (!HasKeyEvent(Context, EInputEventType::KeyPressed, VK_LBUTTON))
	{
		return bConsumed;
	}

	float LocalMouseX = static_cast<float>(Context.MouseLocalPos.x);
	float LocalMouseY = static_cast<float>(Context.MouseLocalPos.y);
	float VPWidth = Owner->GetViewport() ? static_cast<float>(Owner->GetViewport()->GetWidth()) : Owner->GetWindowWidth();
	float VPHeight = Owner->GetViewport() ? static_cast<float>(Owner->GetViewport()->GetHeight()) : Owner->GetWindowHeight();
	FRay Ray = Camera->DeprojectScreenToWorld(LocalMouseX, LocalMouseY, VPWidth, VPHeight);

	FHitResult HitResult{};
	if (FRayUtils::RaycastComponent(Gizmo, Ray, HitResult))
	{
		return true;
	}

	AActor* BestActor = nullptr;
	InteractionWorld->RaycastPrimitives(Ray, HitResult, BestActor);
	if (!BestActor)
	{
		Owner->GetSelectionManager()->ClearSelection();
	}
	else
	{
		Owner->GetSelectionManager()->Select(BestActor);
	}

	return true;
}

