#include "Editor/Input/EditorNavigationTool.h"

#include "Editor/Viewport/EditorViewportClient.h"
#include "Component/CameraComponent.h"
#include "Editor/Settings/EditorSettings.h"

FEditorNavigationTool::FEditorNavigationTool(FEditorViewportClient* InOwner)
	: Owner(InOwner)
{
}

bool FEditorNavigationTool::HandleInput(float DeltaTime)
{
	if (!Owner || !Owner->GetCamera())
	{
		return false;
	}

	const FViewportInputContext& Context = Owner->GetRoutedInputContext();
	if (Context.bImGuiCapturedKeyboard)
	{
		return false;
	}

	UCameraComponent* Camera = Owner->GetCamera();
	const FInputFrame& Frame = Context.Frame;
	const FCameraState& CameraState = Camera->GetCameraState();
	const bool bIsOrtho = CameraState.bIsOrthogonal;
	const bool bCtrlHeld = Frame.IsDown(VK_CONTROL);
	const bool bRButtonDown = Frame.IsDown(VK_RBUTTON);
	const bool bMButtonDown = Frame.IsDown(VK_MBUTTON);
	const float DeltaX = static_cast<float>(Frame.MouseDelta.x);
	const float DeltaY = static_cast<float>(Frame.MouseDelta.y);
	bool bConsumed = false;

	const float MoveSensitivity = Owner->GetRenderOptions().CameraMoveSensitivity;
	const FEditorSettings* Settings = Owner->GetSettings();
	const float CameraSpeed = (Settings ? Settings->CameraSpeed : 10.f) * MoveSensitivity;
	const float PanMouseScale = CameraSpeed * 0.01f;

	if (!bIsOrtho)
	{
		FVector LocalMove = FVector(0, 0, 0);
		float WorldVerticalMove = 0.0f;

		if (!bCtrlHeld && Frame.IsDown('W')) LocalMove.X += CameraSpeed;
		if (!bCtrlHeld && Frame.IsDown('A')) LocalMove.Y -= CameraSpeed;
		if (!bCtrlHeld && Frame.IsDown('S')) LocalMove.X -= CameraSpeed;
		if (!bCtrlHeld && Frame.IsDown('D')) LocalMove.Y += CameraSpeed;
		if (!bCtrlHeld && Frame.IsDown('Q')) WorldVerticalMove -= CameraSpeed;
		if (!bCtrlHeld && Frame.IsDown('E')) WorldVerticalMove += CameraSpeed;

		LocalMove *= DeltaTime;
		Camera->MoveLocal(LocalMove);
		if (LocalMove.Length() > 0.0f)
		{
			bConsumed = true;
		}

		if (WorldVerticalMove != 0.0f)
		{
			Camera->AddWorldOffset(FVector(0.0f, 0.0f, WorldVerticalMove * DeltaTime));
			bConsumed = true;
		}

		if (bMButtonDown)
		{
			Camera->MoveLocal(FVector(0.0f, -DeltaX * PanMouseScale * 0.05f, DeltaY * PanMouseScale * 0.05f));
			if (DeltaX != 0.0f || DeltaY != 0.0f)
			{
				bConsumed = true;
			}
		}

		FVector Rotation = FVector(0, 0, 0);
		const float RotateSensitivity = Owner->GetRenderOptions().CameraRotateSensitivity;
		const float AngleVelocity = (Settings ? Settings->CameraRotationSpeed : 60.f) * RotateSensitivity;
		if (Frame.IsDown(VK_UP)) Rotation.Z -= AngleVelocity;
		if (Frame.IsDown(VK_LEFT)) Rotation.Y -= AngleVelocity;
		if (Frame.IsDown(VK_DOWN)) Rotation.Z += AngleVelocity;
		if (Frame.IsDown(VK_RIGHT)) Rotation.Y += AngleVelocity;

		FVector MouseRotation = FVector(0, 0, 0);
		float MouseRotationSpeed = 0.15f * RotateSensitivity;
		if (bRButtonDown)
		{
			MouseRotation.Y += DeltaX * MouseRotationSpeed;
			MouseRotation.Z += DeltaY * MouseRotationSpeed;
		}

		Rotation *= DeltaTime;
		Camera->Rotate(Rotation.Y + MouseRotation.Y, Rotation.Z + MouseRotation.Z);
		if (Rotation.Length() > 0.0f || MouseRotation.Length() > 0.0f)
		{
			bConsumed = true;
		}
	}
	else
	{
		if (bRButtonDown)
		{
			float PanScale = CameraState.OrthoWidth * 0.002f * MoveSensitivity;
			Camera->MoveLocal(FVector(0, -DeltaX * PanScale, DeltaY * PanScale));
			if (DeltaX != 0.0f || DeltaY != 0.0f)
			{
				bConsumed = true;
			}
		}
	}

	return bConsumed;
}

