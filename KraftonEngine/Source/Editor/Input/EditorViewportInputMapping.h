#pragma once

#include "Engine/Input/InputBinding.h"

namespace EditorViewportInputMapping
{
	enum class EEditorViewportAction : int32
	{
		CycleGizmoMode,
		DuplicateSelection,
		EndPIE
	};

	inline const TArray<FInputBinding>& GetBindings()
	{
		static const TArray<FInputBinding> Bindings =
		{
			{ static_cast<int32>(EEditorViewportAction::CycleGizmoMode), EInputBindingTrigger::Released, { VK_SPACE, false, false, false }, EInputEventType::KeyReleased, 120 },
			{ static_cast<int32>(EEditorViewportAction::DuplicateSelection), EInputBindingTrigger::Pressed, { 'D', true, false, false }, EInputEventType::KeyPressed, 130 },
			{ static_cast<int32>(EEditorViewportAction::EndPIE), EInputBindingTrigger::Pressed, { VK_ESCAPE, false, false, false }, EInputEventType::KeyPressed, 200 }
		};
		return Bindings;
	}

	inline bool IsTriggered(const FViewportInputContext& Context, EEditorViewportAction Action)
	{
		return InputBindingUtils::IsActionTriggered(Context, GetBindings(), static_cast<int32>(Action));
	}
}

