#pragma once

#include "Engine/Input/InputTypes.h"

struct FInputChord
{
	int32 Key = 0;
	bool bCtrl = false;
	bool bAlt = false;
	bool bShift = false;

	bool MatchesState(const FInputFrame& Frame) const
	{
		return Frame.IsDown(VK_CONTROL) == bCtrl
			&& Frame.IsDown(VK_MENU) == bAlt
			&& Frame.IsDown(VK_SHIFT) == bShift;
	}
};

enum class EInputBindingTrigger : uint8
{
	Pressed,
	Down,
	Released,
	EventType
};

struct FInputBinding
{
	int32 ActionId = 0;
	EInputBindingTrigger Trigger = EInputBindingTrigger::Pressed;
	FInputChord Chord{};
	EInputEventType EventType = EInputEventType::KeyPressed;
	int32 Priority = 0;
};

namespace InputBindingUtils
{
	inline bool IsBindingTriggered(const FViewportInputContext& Context, const FInputBinding& Binding)
	{
		switch (Binding.Trigger)
		{
		case EInputBindingTrigger::Pressed:
			for (const FInputEvent& Event : Context.Events)
			{
				if (Event.Type == EInputEventType::KeyPressed && Event.Key == Binding.Chord.Key && Binding.Chord.MatchesState(Context.Frame))
				{
					return true;
				}
			}
			return false;
		case EInputBindingTrigger::Down:
			return Context.Frame.IsDown(Binding.Chord.Key) && Binding.Chord.MatchesState(Context.Frame);
		case EInputBindingTrigger::Released:
			for (const FInputEvent& Event : Context.Events)
			{
				if (Event.Type == EInputEventType::KeyReleased && Event.Key == Binding.Chord.Key && Binding.Chord.MatchesState(Context.Frame))
				{
					return true;
				}
			}
			return false;
		case EInputBindingTrigger::EventType:
			for (const FInputEvent& Event : Context.Events)
			{
				if (Event.Type != Binding.EventType)
				{
					continue;
				}
				if (Binding.Chord.Key != 0 && Event.Key != Binding.Chord.Key)
				{
					continue;
				}
				if (!Binding.Chord.MatchesState(Context.Frame))
				{
					continue;
				}
				return true;
			}
			return false;
		default:
			return false;
		}
	}

	inline bool IsActionTriggered(const FViewportInputContext& Context, const TArray<FInputBinding>& Bindings, int32 ActionId)
	{
		for (const FInputBinding& Binding : Bindings)
		{
			if (Binding.ActionId != ActionId)
			{
				continue;
			}

			if (IsBindingTriggered(Context, Binding))
			{
				return true;
			}
		}
		return false;
	}
}
