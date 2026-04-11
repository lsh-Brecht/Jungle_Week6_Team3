#pragma once

#include "Engine/Input/InputTypes.h"

namespace EditorViewportInputUtils
{
	inline bool IsLeftNavigationDragActive(const FViewportInputContext& Context)
	{
		// ImGui가 마우스를 캡처 중이면 (relative 모드 유지 중이 아닌 한) 에디터 내비게이션은 개입하지 않는다.
		if (Context.bImGuiCapturedMouse && !Context.bRelativeMouseMode)
		{
			return false;
		}

		if (!Context.Frame.IsDown(VK_LBUTTON) && !Context.WasPointerDragEnded(EPointerButton::Left))
		{
			return false;
		}

		const bool bCtrl = Context.Frame.IsDown(VK_CONTROL);
		const bool bAlt = Context.Frame.IsDown(VK_MENU);
		if ((bCtrl && bAlt) || bAlt)
		{
			return false;
		}

		FPointerGesture LeftGesture{};
		if (Context.GetPointerGesture(EPointerButton::Left, LeftGesture)
			&& (LeftGesture.bStarted || LeftGesture.bActive || LeftGesture.bEnded))
		{
			return true;
		}

		constexpr int32 DragThreshold = 5;
		const LONG DragX = LeftGesture.TotalDelta.x;
		const LONG DragY = LeftGesture.TotalDelta.y;
		return (DragX * DragX + DragY * DragY) >= (DragThreshold * DragThreshold);
	}
}
