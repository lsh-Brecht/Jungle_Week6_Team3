#include "Engine/Input/InputSystem.h"

#include <cmath>

void InputSystem::Tick()
{
    if (OwnerHWnd && GetForegroundWindow() != OwnerHWnd)
    {
        for (int i = 0; i < 256; ++i)
        {
            PrevStates[i] = CurrentStates[i];
            CurrentStates[i] = false;
        }

        bLeftDragJustStarted = false;
        bRightDragJustStarted = false;
        bLeftDragJustEnded = bLeftDragging;
        bRightDragJustEnded = bRightDragging;
        bLeftDragging = false;
        bRightDragging = false;
        bLeftDragCandidate = false;
        bRightDragCandidate = false;

        PrevScrollDelta = ScrollDelta;
        ScrollDelta = 0;
        FrameMouseDeltaX = 0;
        FrameMouseDeltaY = 0;
        RawMouseDeltaAccumX = 0;
        RawMouseDeltaAccumY = 0;

        GetCursorPos(&MousePos);
        PrevMousePos = MousePos;
        return;
    }

    for (int i = 0; i < 256; ++i)
    {
        PrevStates[i] = CurrentStates[i];
        CurrentStates[i] = (GetAsyncKeyState(i) & 0x8000) != 0;
    }

    bLeftDragJustStarted = false;
    bRightDragJustStarted = false;
    bLeftDragJustEnded = false;
    bRightDragJustEnded = false;

    PrevScrollDelta = ScrollDelta;
    ScrollDelta = 0;

    PrevMousePos = MousePos;
    GetCursorPos(&MousePos);

    FrameMouseDeltaX = MousePos.x - PrevMousePos.x;
    FrameMouseDeltaY = MousePos.y - PrevMousePos.y;
    if (bUseRawMouse)
    {
        FrameMouseDeltaX = RawMouseDeltaAccumX;
        FrameMouseDeltaY = RawMouseDeltaAccumY;
    }
    RawMouseDeltaAccumX = 0;
    RawMouseDeltaAccumY = 0;

    if (GetKeyDown(VK_LBUTTON))
    {
        bLeftDragCandidate = true;
        LeftMouseDownPos = MousePos;
    }
    if (GetKeyDown(VK_RBUTTON))
    {
        bRightDragCandidate = true;
        RightMouseDownPos = MousePos;
    }

    if (!bLeftDragging && IsDraggingLeft())
    {
        FilterDragThreshold(bLeftDragCandidate, bLeftDragging, bLeftDragJustStarted, LeftMouseDownPos, LeftDragStartPos);
    }
    else if (GetKeyUp(VK_LBUTTON))
    {
        if (bLeftDragging)
        {
            bLeftDragJustEnded = true;
        }
        bLeftDragging = false;
        bLeftDragCandidate = false;
    }

    if (!bRightDragging && IsDraggingRight())
    {
        FilterDragThreshold(bRightDragCandidate, bRightDragging, bRightDragJustStarted, RightMouseDownPos, RightDragStartPos);
    }
    else if (GetKeyUp(VK_RBUTTON))
    {
        if (bRightDragging)
        {
            bRightDragJustEnded = true;
        }
        bRightDragging = false;
        bRightDragCandidate = false;
    }
}

void InputSystem::FilterDragThreshold(
    bool& bCandidate, bool& bDragging, bool& bJustStarted,
    const POINT& MouseDownPos, POINT& DragStartPos)
{
    if (bCandidate && !bDragging)
    {
        const int DX = MousePos.x - MouseDownPos.x;
        const int DY = MousePos.y - MouseDownPos.y;
        const int DistSq = DX * DX + DY * DY;

        if (DistSq >= DRAG_THRESHOLD * DRAG_THRESHOLD)
        {
            bJustStarted = true;
            bDragging = true;
            DragStartPos = MouseDownPos;
        }
    }
}

void InputSystem::AddRawMouseDelta(int DeltaX, int DeltaY)
{
    RawMouseDeltaAccumX += DeltaX;
    RawMouseDeltaAccumY += DeltaY;
}

POINT InputSystem::GetLeftDragVector() const
{
    POINT V;
    V.x = MousePos.x - LeftDragStartPos.x;
    V.y = MousePos.y - LeftDragStartPos.y;
    return V;
}

POINT InputSystem::GetRightDragVector() const
{
    POINT V;
    V.x = MousePos.x - RightDragStartPos.x;
    V.y = MousePos.y - RightDragStartPos.y;
    return V;
}

float InputSystem::GetLeftDragDistance() const
{
    const POINT V = GetLeftDragVector();
    return std::sqrt(static_cast<float>(V.x * V.x + V.y * V.y));
}

float InputSystem::GetRightDragDistance() const
{
    const POINT V = GetRightDragVector();
    return std::sqrt(static_cast<float>(V.x * V.x + V.y * V.y));
}
