#pragma once

#include <windows.h>

#include <functional>

#include "Core/CoreTypes.h"

class FViewport;
class FViewportClient;
class UWorld;

enum class EInteractionDomain : uint8
{
	Editor,
	PIE,
	EditorOnPIE
};

enum class EMouseInputMode : uint8
{
	Absolute,
	Relative
};

enum class EInputEventType : uint8
{
	KeyPressed,
	KeyReleased,
	WheelScrolled,
	PointerDragStarted,
	PointerDragEnded
};

enum class EPointerButton : uint8
{
	None,
	Left,
	Right,
	Middle
};

struct FInputEvent
{
	EInputEventType Type = EInputEventType::KeyPressed;
	int32 Key = 0;
	EPointerButton PointerButton = EPointerButton::None;
	POINT MouseScreenPos = { 0, 0 };
	POINT MouseDelta = { 0, 0 };
	float WheelNotches = 0.0f;
};

struct FInputFrame
{
	uint64 FrameNumber = 0;
	HWND SourceWindow = nullptr;
	EMouseInputMode MouseInputMode = EMouseInputMode::Absolute;

	POINT MouseScreenPos = { 0, 0 };
	POINT MouseDelta = { 0, 0 };
	float WheelNotches = 0.0f;

	bool KeyDown[256] = {};
	bool bLeftDragging = false;
	bool bRightDragging = false;
	POINT LeftDragVector = { 0, 0 };
	POINT RightDragVector = { 0, 0 };

	bool IsDown(int32 VK) const { return KeyDown[VK]; }
};

struct FInteractionBinding
{
	FViewportClient* ReceiverVC = nullptr;
	UWorld* TargetWorld = nullptr;
	EInteractionDomain Domain = EInteractionDomain::Editor;
};

struct FViewportInputContext
{
	FInputFrame Frame;
	TArray<FInputEvent> Events;

	FViewport* TargetViewport = nullptr;
	FViewportClient* TargetClient = nullptr;
	UWorld* TargetWorld = nullptr;
	EInteractionDomain Domain = EInteractionDomain::Editor;

	POINT MouseClientPos = { 0, 0 };
	POINT MouseLocalPos = { 0, 0 };
	POINT MouseLocalDelta = { 0, 0 };

	bool bHovered = false;
	bool bFocused = false;
	bool bCaptured = false;
	bool bImGuiCapturedMouse = false;
	bool bImGuiCapturedKeyboard = false;
	bool bRelativeMouseMode = false;
	bool bConsumed = false;
};

