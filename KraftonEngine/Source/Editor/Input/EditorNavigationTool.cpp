#include "Editor/Input/EditorNavigationTool.h"

#include "Component/CameraComponent.h"
#include "Editor/Input/EditorViewportInputMapping.h"
#include "Editor/Input/EditorViewportInputUtils.h"
#include "Editor/Selection/SelectionManager.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "GameFramework/AActor.h"
#include "Math/MathUtils.h"
#include "Component/GizmoComponent.h"

#include <cmath>

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
	if (Context.bImGuiCapturedMouse && !Context.bRelativeMouseMode)
	{
		return false;
	}

	const bool bGizmoDragging =
		Owner->GetGizmo()
		&& (Owner->GetGizmo()->IsHolding() || Owner->GetGizmo()->IsPressedOnHandle());
	const bool bAltOrbit = EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::NavOrbitAltLeftDown);
	const bool bAltDolly = EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::NavDollyAltRightDown);
	const bool bAltPan = EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::NavPanAltMiddleDown);
	const bool bRightLook = EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::NavLookRightDown);
	const bool bMiddlePan = EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::NavLookMiddleDown);
	const bool bLeftDragLook = EditorViewportInputUtils::IsLeftNavigationDragActive(Context) && !bGizmoDragging;
	const bool bAnyMouseNav = bAltOrbit || bAltDolly || bAltPan || bRightLook || bMiddlePan || bLeftDragLook;
	const bool bKeyboardBlocked = Context.bImGuiCapturedKeyboard && !bAnyMouseNav;

	UCameraComponent* Camera = Owner->GetCamera();
	if (!bCameraTargetInitialized)
	{
		SyncCameraTargetFromCurrent();
	}
	const FCameraState& CameraState = Camera->GetCameraState();
	const bool bIsOrtho = CameraState.bIsOrthogonal;
	const float MoveSensitivity = Owner->GetRenderOptions().CameraMoveSensitivity;
	const float RotateSensitivity = Owner->GetRenderOptions().CameraRotateSensitivity;
	const float CameraSpeed = GetEffectiveCameraSpeed() * MoveSensitivity;
	const float DeltaX = static_cast<float>(Context.Frame.MouseDelta.x);
	const float DeltaY = static_cast<float>(Context.Frame.MouseDelta.y);
	const float ScrollNotches = Context.Frame.WheelNotches;

	bool bConsumed = false;

	if (bRightLook && ScrollNotches != 0.0f)
	{
		AdjustRuntimeCameraSpeed(ScrollNotches);
		bConsumed = true;
	}

	float DollyInputY = 0.0f;
	if (bAltDolly)
	{
		DollyInputY += DeltaY;
	}
	if (ScrollNotches != 0.0f && !bRightLook && EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::NavWheelScroll))
	{
		constexpr float WheelToDolly = 50.0f;
		DollyInputY += -ScrollNotches * WheelToDolly;
	}

	if (!bIsOrtho)
	{
		FVector Move = FVector(0.0f, 0.0f, 0.0f);
		const bool bAllowFlyMove = (bRightLook || bAltOrbit || bLeftDragLook) && !bKeyboardBlocked;
		if (bAllowFlyMove)
		{
			if (EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::NavMoveForward)) Move.X += CameraSpeed;
			if (EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::NavMoveLeft)) Move.Y -= CameraSpeed;
			if (EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::NavMoveBackward)) Move.X -= CameraSpeed;
			if (EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::NavMoveRight)) Move.Y += CameraSpeed;
			if (EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::NavMoveDown)) Move.Z -= CameraSpeed;
			if (EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::NavMoveUp)) Move.Z += CameraSpeed;
		}
		Move *= DeltaTime;
		if (Move.Length() > 0.0f)
		{
			AddCameraMoveInputLocal(Move);
			bConsumed = true;
		}

		if (bMiddlePan || bAltPan)
		{
			const float PanScale = 0.03f * MoveSensitivity;
			AddCameraMoveInputLocal(FVector(0.0f, DeltaX * PanScale, -DeltaY * PanScale));
			if (DeltaX != 0.0f || DeltaY != 0.0f)
			{
				bConsumed = true;
			}
		}

		if (DollyInputY != 0.0f)
		{
			const float DollyScale = 0.001f * CameraSpeed;
			AddCameraMoveInputLocal(FVector(-DollyInputY * DollyScale, 0.0f, 0.0f));
			bConsumed = true;
		}

		if (bAltOrbit)
		{
			FVector Pivot = Camera->GetWorldLocation() + Camera->GetForwardVector() * 10.0f;
			if (Owner->GetSelectionManager())
			{
				if (AActor* Primary = Owner->GetSelectionManager()->GetPrimarySelection())
				{
					Pivot = Primary->GetActorLocation();
				}
			}
			OrbitCameraAroundPivot(Pivot, DeltaX, DeltaY, RotateSensitivity);
			bConsumed = true;
		}
		else
		{
			float DeltaYaw = 0.0f;
			float DeltaPitch = 0.0f;

			const float AngleVelocity = (Owner->GetSettings() ? Owner->GetSettings()->CameraRotationSpeed : 60.0f) * RotateSensitivity;
			if (!bKeyboardBlocked)
			{
				if (EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::NavRotateUp)) DeltaPitch -= AngleVelocity * DeltaTime;
				if (EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::NavRotateDown)) DeltaPitch += AngleVelocity * DeltaTime;
				if (EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::NavRotateLeft)) DeltaYaw -= AngleVelocity * DeltaTime;
				if (EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::NavRotateRight)) DeltaYaw += AngleVelocity * DeltaTime;
			}

			if ((bRightLook || bLeftDragLook) && !bAltDolly && !bAltPan)
			{
				const float MouseLookScale = 0.15f * RotateSensitivity;
				DeltaYaw += DeltaX * MouseLookScale;
				DeltaPitch += DeltaY * MouseLookScale;
			}

			if (DeltaYaw != 0.0f || DeltaPitch != 0.0f)
			{
				AddCameraRotateInput(DeltaYaw, DeltaPitch);
				bConsumed = true;
			}
		}
	}
	else
	{
		if (bMiddlePan || bAltPan || bRightLook)
		{
			const float PanScale = CameraState.OrthoWidth * 0.002f * MoveSensitivity;
			Camera->MoveLocal(FVector(0.0f, -DeltaX * PanScale, DeltaY * PanScale));
			if (DeltaX != 0.0f || DeltaY != 0.0f)
			{
				bConsumed = true;
			}
		}

		if (DollyInputY != 0.0f)
		{
			const float NextWidth = Camera->GetOrthoWidth() + DollyInputY * CameraState.OrthoWidth * 0.003f;
			Camera->SetOrthoWidth(Clamp(NextWidth, 0.1f, 5000.0f));
			bConsumed = true;
		}

		if (!bConsumed && ScrollNotches != 0.0f)
		{
			const float ZoomSpeed = Owner->GetSettings() ? Owner->GetSettings()->CameraZoomSpeed : 300.0f;
			const float NextWidth = Camera->GetOrthoWidth() - ScrollNotches * ZoomSpeed * DeltaTime;
			Camera->SetOrthoWidth(Clamp(NextWidth, 0.1f, 5000.0f));
			bConsumed = true;
		}
	}

	Camera->SetWorldLocation(CameraTargetLocation);
	Camera->SetRelativeRotation(CameraTargetRotation);
	return bConsumed;
}

void FEditorNavigationTool::FocusOnTarget(const FVector& Target, float DesiredDistance)
{
	if (!Owner || !Owner->GetCamera())
	{
		return;
	}

	SyncCameraTargetFromCurrent();

	UCameraComponent* Camera = Owner->GetCamera();
	if (Camera->IsOrthogonal())
	{
		const FVector Right = CameraTargetRotation.GetRightVector().Normalized();
		const FVector Up = CameraTargetRotation.GetUpVector().Normalized();
		const FVector ToTarget = Target - CameraTargetLocation;
		CameraTargetLocation = CameraTargetLocation + Right * ToTarget.Dot(Right) + Up * ToTarget.Dot(Up);
		Camera->SetWorldLocation(CameraTargetLocation);
		Camera->SetRelativeRotation(CameraTargetRotation);
		return;
	}

	FVector ToCamera = CameraTargetLocation - Target;
	float Distance = ToCamera.Length();
	if (Distance < 1.0f)
	{
		ToCamera = CameraTargetRotation.GetForwardVector() * -1.0f;
		Distance = 10.0f;
	}
	if (DesiredDistance > 0.0f)
	{
		Distance = DesiredDistance;
	}

	Distance = Clamp(Distance, 3.0f, 400.0f);
	const FVector Direction = ToCamera.Normalized();
	CameraTargetLocation = Target + Direction * Distance;
	CameraTargetRotation = MakeLookAtRotation(CameraTargetLocation, Target);
	Camera->SetWorldLocation(CameraTargetLocation);
	Camera->SetRelativeRotation(CameraTargetRotation);
}

void FEditorNavigationTool::SyncCameraTargetFromCurrent()
{
	if (!Owner || !Owner->GetCamera())
	{
		return;
	}

	CameraTargetLocation = Owner->GetCamera()->GetWorldLocation();
	CameraTargetRotation = Owner->GetCamera()->GetRelativeRotation();
	bCameraTargetInitialized = true;
}

FRotator FEditorNavigationTool::MakeLookAtRotation(const FVector& From, const FVector& To) const
{
	const FVector Diff = (To - From).Normalized();
	FRotator LookRotation = FRotator();
	LookRotation.Pitch = -asinf(Diff.Z) * RAD_TO_DEG;
	if (std::fabs(Diff.Z) < 0.999f)
	{
		LookRotation.Yaw = atan2f(Diff.Y, Diff.X) * RAD_TO_DEG;
	}
	LookRotation.Roll = 0.0f;
	return LookRotation;
}

void FEditorNavigationTool::AddCameraMoveInputLocal(const FVector& DeltaLocal)
{
	if (!bCameraTargetInitialized)
	{
		SyncCameraTargetFromCurrent();
	}
	const FVector Forward = CameraTargetRotation.GetForwardVector().Normalized();
	const FVector Right = CameraTargetRotation.GetRightVector().Normalized();
	const FVector Up = CameraTargetRotation.GetUpVector().Normalized();
	CameraTargetLocation = CameraTargetLocation + Forward * DeltaLocal.X + Right * DeltaLocal.Y + Up * DeltaLocal.Z;
}

void FEditorNavigationTool::AddCameraRotateInput(float DeltaYaw, float DeltaPitch)
{
	if (!bCameraTargetInitialized)
	{
		SyncCameraTargetFromCurrent();
	}

	constexpr float PseudoScale = 0.65f;
	CameraTargetRotation.Yaw += DeltaYaw * PseudoScale;
	CameraTargetRotation.Pitch = Clamp(CameraTargetRotation.Pitch + DeltaPitch * PseudoScale, -89.9f, 89.9f);
	CameraTargetRotation.Roll = 0.0f;
}

void FEditorNavigationTool::OrbitCameraAroundPivot(const FVector& Pivot, float DeltaMouseX, float DeltaMouseY, float OrbitSensitivity)
{
	if (!bCameraTargetInitialized)
	{
		SyncCameraTargetFromCurrent();
	}

	const FVector Offset = CameraTargetLocation - Pivot;
	float Radius = Offset.Length();
	if (Radius < 1.0f)
	{
		Radius = 10.0f;
	}

	float Azimuth = atan2f(Offset.Y, Offset.X);
	float Elevation = asinf(Clamp(Offset.Z / Radius, -1.0f, 1.0f));
	const float OrbitSpeed = 0.0035f * OrbitSensitivity;
	Azimuth += DeltaMouseX * OrbitSpeed;
	Elevation = Clamp(Elevation + DeltaMouseY * OrbitSpeed, -1.4f, 1.4f);

	const float CosElevation = cosf(Elevation);
	FVector NextOffset = FVector(
		Radius * CosElevation * cosf(Azimuth),
		Radius * CosElevation * sinf(Azimuth),
		Radius * sinf(Elevation));

	CameraTargetLocation = Pivot + NextOffset;
	CameraTargetRotation = MakeLookAtRotation(CameraTargetLocation, Pivot);
}

void FEditorNavigationTool::AdjustRuntimeCameraSpeed(float WheelNotches)
{
	if (WheelNotches == 0.0f)
	{
		return;
	}

	const float BaseSpeed = Owner && Owner->GetSettings() ? Owner->GetSettings()->CameraSpeed : FEditorSettings::Get().CameraSpeed;
	const float SafeBase = BaseSpeed > 0.0001f ? BaseSpeed : 0.0001f;
	const float MinMultiplier = 0.1f / SafeBase;
	const float MaxMultiplier = 32.0f / SafeBase;
	const float Step = 0.1f;
	RuntimeCameraSpeedMultiplier = Clamp(RuntimeCameraSpeedMultiplier + WheelNotches * Step, MinMultiplier, MaxMultiplier);
}

void FEditorNavigationTool::SetRuntimeCameraSpeedMultiplier(float InMultiplier)
{
	const float BaseSpeed = Owner && Owner->GetSettings() ? Owner->GetSettings()->CameraSpeed : FEditorSettings::Get().CameraSpeed;
	const float SafeBase = BaseSpeed > 0.0001f ? BaseSpeed : 0.0001f;
	const float MinMultiplier = GetMinCameraSpeedValue() / SafeBase;
	const float MaxMultiplier = GetMaxCameraSpeedValue() / SafeBase;
	RuntimeCameraSpeedMultiplier = Clamp(InMultiplier, MinMultiplier, MaxMultiplier);
}

float FEditorNavigationTool::GetEffectiveCameraSpeed() const
{
	const float BaseSpeed = Owner && Owner->GetSettings() ? Owner->GetSettings()->CameraSpeed : FEditorSettings::Get().CameraSpeed;
	return BaseSpeed * RuntimeCameraSpeedMultiplier;
}
