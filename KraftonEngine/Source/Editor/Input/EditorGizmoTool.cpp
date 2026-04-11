#include "Editor/Input/EditorGizmoTool.h"

#include "Editor/Viewport/EditorViewportClient.h"
#include "Component/CameraComponent.h"
#include "Component/GizmoComponent.h"
#include "Collision/RayUtils.h"
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

bool HasPointerEvent(const FViewportInputContext& Context, EInputEventType Type, EPointerButton Button)
{
	for (const FInputEvent& Event : Context.Events)
	{
		if (Event.Type == Type && Event.PointerButton == Button)
		{
			return true;
		}
	}
	return false;
}
}

FEditorGizmoTool::FEditorGizmoTool(FEditorViewportClient* InOwner)
	: Owner(InOwner)
{
}

bool FEditorGizmoTool::HandleInput(float DeltaTime)
{
	(void)DeltaTime;

	if (!Owner || !Owner->GetCamera() || !Owner->GetGizmo() || !Owner->ResolveInteractionWorld())
	{
		return false;
	}

	UCameraComponent* Camera = Owner->GetCamera();
	UGizmoComponent* Gizmo = Owner->GetGizmo();
	const FViewportInputContext& Context = Owner->GetRoutedInputContext();

	Gizmo->ApplyScreenSpaceScaling(Camera->GetWorldLocation(), Camera->IsOrthogonal(), Camera->GetOrthoWidth());
	Gizmo->UpdateAxisMask(Owner->GetRenderOptions().ViewportType);

	if (Context.bImGuiCapturedMouse && !Gizmo->IsHolding())
	{
		return false;
	}

	float LocalMouseX = static_cast<float>(Context.MouseLocalPos.x);
	float LocalMouseY = static_cast<float>(Context.MouseLocalPos.y);
	float VPWidth = Owner->GetViewport() ? static_cast<float>(Owner->GetViewport()->GetWidth()) : Owner->GetWindowWidth();
	float VPHeight = Owner->GetViewport() ? static_cast<float>(Owner->GetViewport()->GetHeight()) : Owner->GetWindowHeight();
	FRay Ray = Camera->DeprojectScreenToWorld(LocalMouseX, LocalMouseY, VPWidth, VPHeight);

	if (HasKeyEvent(Context, EInputEventType::KeyPressed, VK_LBUTTON))
	{
		FHitResult HitResult{};
		if (FRayUtils::RaycastComponent(Gizmo, Ray, HitResult))
		{
			Gizmo->SetPressedOnHandle(true);
			return true;
		}
	}
	else if (Context.Frame.bLeftDragging)
	{
		if (Gizmo->IsPressedOnHandle() && !Gizmo->IsHolding())
		{
			Gizmo->SetHolding(true);
		}

		if (Gizmo->IsHolding())
		{
			Gizmo->UpdateDrag(Ray);
			return true;
		}
	}
	else if (HasPointerEvent(Context, EInputEventType::PointerDragEnded, EPointerButton::Left))
	{
		Gizmo->DragEnd();
		return true;
	}
	else if (HasKeyEvent(Context, EInputEventType::KeyReleased, VK_LBUTTON))
	{
		if (Gizmo->IsPressedOnHandle())
		{
			Gizmo->SetPressedOnHandle(false);
			return true;
		}
	}

	return false;
}

