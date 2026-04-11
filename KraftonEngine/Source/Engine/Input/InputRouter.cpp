#include "Engine/Input/InputRouter.h"

#include "Engine/Input/InputSystem.h"
#include "UI/SWindow.h"
#include "Viewport/ViewportClient.h"
#include <algorithm>
#include <utility>

namespace
{
bool IsMouseButtonKey(int32 VK)
{
	return (VK == VK_LBUTTON) || (VK == VK_RBUTTON) || (VK == VK_MBUTTON)
		|| (VK == VK_XBUTTON1) || (VK == VK_XBUTTON2);
}

EPointerButton ToPointerButton(int32 VK)
{
	switch (VK)
	{
	case VK_LBUTTON: return EPointerButton::Left;
	case VK_RBUTTON: return EPointerButton::Right;
	case VK_MBUTTON: return EPointerButton::Middle;
	default: return EPointerButton::None;
	}
}
}

void FInputRouter::SetImGuiCaptureState(bool bCaptureMouse, bool bCaptureKeyboard)
{
	bImGuiCaptureMouse = bCaptureMouse;
	bImGuiCaptureKeyboard = bCaptureKeyboard;
}

void FInputRouter::ClearTargets()
{
	Targets.clear();
	HoveredViewport = nullptr;
}

void FInputRouter::RegisterTarget(
	FViewport* InViewport,
	FViewportClient* InClient,
	EInteractionDomain InDomain,
	FRectProvider InRectProvider,
	FWorldResolver InWorldResolver)
{
	if (!InViewport || !InClient || !InRectProvider)
	{
		return;
	}

	FTargetEntry Entry;
	Entry.Viewport = InViewport;
	Entry.Client = InClient;
	Entry.Domain = InDomain;
	Entry.RectProvider = std::move(InRectProvider);
	Entry.WorldResolver = std::move(InWorldResolver);
	Targets.push_back(std::move(Entry));
}

bool FInputRouter::Tick(FViewportInputContext& OutContext, FInteractionBinding& OutBinding)
{
	InputSystem::Get().Tick();
	InputSystem& Input = InputSystem::Get();

	if (!OwnerWindow || Targets.empty())
	{
		HoveredViewport = nullptr;
		FocusedViewport = nullptr;
		CapturedViewport = nullptr;
		return false;
	}

	if (GetForegroundWindow() != OwnerWindow)
	{
		HoveredViewport = nullptr;
		FocusedViewport = nullptr;
		CapturedViewport = nullptr;
		return false;
	}

	POINT MouseScreenPos = Input.GetMousePos();
	POINT MouseClientPos = MouseScreenPos;
	ScreenToClient(OwnerWindow, &MouseClientPos);

	FTargetEntry* HoveredEntry = nullptr;
	FRect HoveredRect = {};
	for (FTargetEntry& Entry : Targets)
	{
		FRect Rect = {};
		if (!Entry.RectProvider(Rect))
		{
			continue;
		}
		if (IsPointInRect(MouseClientPos, Rect))
		{
			HoveredEntry = &Entry;
			HoveredRect = Rect;
			break;
		}
	}

	HoveredViewport = HoveredEntry ? HoveredEntry->Viewport : nullptr;

	bool bAnyPointerPressed =
		Input.GetKeyDown(VK_LBUTTON)
		|| Input.GetKeyDown(VK_RBUTTON)
		|| Input.GetKeyDown(VK_MBUTTON);

	bool bAnyPointerDown =
		Input.GetKey(VK_LBUTTON)
		|| Input.GetKey(VK_RBUTTON)
		|| Input.GetKey(VK_MBUTTON)
		|| Input.GetLeftDragging()
		|| Input.GetRightDragging();

	bool bFocusedStillValid = false;
	bool bCapturedStillValid = false;
	for (const FTargetEntry& Entry : Targets)
	{
		bFocusedStillValid |= (FocusedViewport == Entry.Viewport);
		bCapturedStillValid |= (CapturedViewport == Entry.Viewport);
	}

	if (!bFocusedStillValid)
	{
		FocusedViewport = nullptr;
	}
	if (!bCapturedStillValid)
	{
		CapturedViewport = nullptr;
	}

	if (bImGuiCaptureMouse && !CapturedViewport && !HoveredEntry)
	{
		bAnyPointerPressed = false;
		bAnyPointerDown = false;
	}

	if (bAnyPointerPressed && HoveredEntry)
	{
		FocusedViewport = HoveredEntry->Viewport;
		CapturedViewport = HoveredEntry->Viewport;
	}

	if (!bAnyPointerDown)
	{
		CapturedViewport = nullptr;
	}

	FTargetEntry* TargetEntry = nullptr;
	FRect TargetRect = {};
	if (CapturedViewport)
	{
		TargetEntry = FindEntryByViewport(CapturedViewport, TargetRect);
	}
	if (!TargetEntry && HoveredEntry)
	{
		TargetEntry = HoveredEntry;
		TargetRect = HoveredRect;
	}
	if (!TargetEntry && FocusedViewport)
	{
		TargetEntry = FindEntryByViewport(FocusedViewport, TargetRect);
	}
	if (!TargetEntry)
	{
		return false;
	}

	OutContext = {};
	OutBinding = {};

	FInputFrame Frame;
	Frame.FrameNumber = ++InputFrameCounter;
	Frame.SourceWindow = OwnerWindow;
	Frame.MouseInputMode = EMouseInputMode::Absolute;
	Frame.MouseScreenPos = MouseScreenPos;
	Frame.MouseDelta = { Input.MouseDeltaX(), Input.MouseDeltaY() };
	Frame.WheelNotches = Input.GetScrollNotches();
	Frame.bLeftDragging = Input.GetLeftDragging();
	Frame.bRightDragging = Input.GetRightDragging();
	Frame.LeftDragVector = Input.GetLeftDragVector();
	Frame.RightDragVector = Input.GetRightDragVector();

	for (int32 VK = 0; VK < 256; ++VK)
	{
		Frame.KeyDown[VK] = Input.GetKey(VK);
	}

	if (bImGuiCaptureKeyboard)
	{
		for (int32 VK = 0; VK < 256; ++VK)
		{
			if (!IsMouseButtonKey(VK))
			{
				Frame.KeyDown[VK] = false;
			}
		}
	}

	OutContext.Frame = Frame;
	OutContext.TargetViewport = TargetEntry->Viewport;
	OutContext.TargetClient = TargetEntry->Client;
	OutContext.Domain = TargetEntry->Domain;
	OutContext.TargetWorld = TargetEntry->WorldResolver ? TargetEntry->WorldResolver() : nullptr;
	OutContext.MouseClientPos = MouseClientPos;
	OutContext.MouseLocalPos =
	{
		MouseClientPos.x - static_cast<LONG>(TargetRect.X),
		MouseClientPos.y - static_cast<LONG>(TargetRect.Y)
	};
	OutContext.MouseLocalDelta = Frame.MouseDelta;
	OutContext.bHovered = (HoveredViewport == TargetEntry->Viewport);
	OutContext.bFocused = (FocusedViewport == TargetEntry->Viewport);
	OutContext.bCaptured = (CapturedViewport == TargetEntry->Viewport);
	OutContext.bImGuiCapturedMouse = bImGuiCaptureMouse;
	OutContext.bImGuiCapturedKeyboard = bImGuiCaptureKeyboard;

	for (int32 VK = 0; VK < 256; ++VK)
	{
		if (Input.GetKeyDown(VK))
		{
			FInputEvent E{};
			E.Type = EInputEventType::KeyPressed;
			E.Key = VK;
			E.PointerButton = ToPointerButton(VK);
			E.MouseScreenPos = MouseScreenPos;
			E.MouseDelta = Frame.MouseDelta;
			OutContext.Events.push_back(E);
		}
		if (Input.GetKeyUp(VK))
		{
			FInputEvent E{};
			E.Type = EInputEventType::KeyReleased;
			E.Key = VK;
			E.PointerButton = ToPointerButton(VK);
			E.MouseScreenPos = MouseScreenPos;
			E.MouseDelta = Frame.MouseDelta;
			OutContext.Events.push_back(E);
		}
	}

	if (Frame.WheelNotches != 0.0f)
	{
		FInputEvent E{};
		E.Type = EInputEventType::WheelScrolled;
		E.MouseScreenPos = MouseScreenPos;
		E.WheelNotches = Frame.WheelNotches;
		OutContext.Events.push_back(E);
	}

	if (Input.GetLeftDragStart())
	{
		FInputEvent E{};
		E.Type = EInputEventType::PointerDragStarted;
		E.PointerButton = EPointerButton::Left;
		E.MouseScreenPos = MouseScreenPos;
		E.MouseDelta = Frame.LeftDragVector;
		OutContext.Events.push_back(E);
	}
	if (Input.GetLeftDragEnd())
	{
		FInputEvent E{};
		E.Type = EInputEventType::PointerDragEnded;
		E.PointerButton = EPointerButton::Left;
		E.MouseScreenPos = MouseScreenPos;
		E.MouseDelta = Frame.LeftDragVector;
		OutContext.Events.push_back(E);
	}
	if (Input.GetRightDragStart())
	{
		FInputEvent E{};
		E.Type = EInputEventType::PointerDragStarted;
		E.PointerButton = EPointerButton::Right;
		E.MouseScreenPos = MouseScreenPos;
		E.MouseDelta = Frame.RightDragVector;
		OutContext.Events.push_back(E);
	}
	if (Input.GetRightDragEnd())
	{
		FInputEvent E{};
		E.Type = EInputEventType::PointerDragEnded;
		E.PointerButton = EPointerButton::Right;
		E.MouseScreenPos = MouseScreenPos;
		E.MouseDelta = Frame.RightDragVector;
		OutContext.Events.push_back(E);
	}

	if (bImGuiCaptureKeyboard || (bImGuiCaptureMouse && !OutContext.bCaptured && !OutContext.bHovered))
	{
		OutContext.Events.erase(
			std::remove_if(
				OutContext.Events.begin(),
				OutContext.Events.end(),
				[](const FInputEvent& Event)
				{
					if (Event.Type == EInputEventType::WheelScrolled)
					{
						return true;
					}
					if (Event.Type == EInputEventType::PointerDragStarted || Event.Type == EInputEventType::PointerDragEnded)
					{
						return true;
					}
					return !IsMouseButtonKey(Event.Key);
				}),
			OutContext.Events.end());
	}

	OutBinding.ReceiverVC = TargetEntry->Client;
	OutBinding.TargetWorld = OutContext.TargetWorld;
	OutBinding.Domain = OutContext.Domain;

	OutContext.bConsumed = TargetEntry->Client->ProcessInput(OutContext);
	return true;
}

bool FInputRouter::IsPointInRect(const POINT& Point, const FRect& Rect)
{
	return Point.x >= Rect.X
		&& Point.x < (Rect.X + Rect.Width)
		&& Point.y >= Rect.Y
		&& Point.y < (Rect.Y + Rect.Height);
}

FInputRouter::FTargetEntry* FInputRouter::FindEntryByViewport(FViewport* InViewport, FRect& OutRect)
{
	for (FTargetEntry& Entry : Targets)
	{
		if (Entry.Viewport != InViewport)
		{
			continue;
		}
		if (Entry.RectProvider(OutRect))
		{
			return &Entry;
		}
		return nullptr;
	}
	return nullptr;
}
