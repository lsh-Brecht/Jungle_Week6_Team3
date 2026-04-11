#include "Editor/Input/EditorViewportCommandTool.h"

#include "Component/GizmoComponent.h"
#include "Editor/EditorEngine.h"
#include "Editor/Input/EditorNavigationTool.h"
#include "Editor/Input/EditorViewportController.h"
#include "Editor/Input/EditorViewportInputMapping.h"
#include "Editor/Selection/SelectionManager.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "Engine/Runtime/Engine.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"

FEditorViewportCommandTool::FEditorViewportCommandTool(FEditorViewportClient* InOwner, FEditorViewportController* InController)
	: Owner(InOwner), Controller(InController)
{
}

namespace
{
using EAction = EditorViewportInputMapping::EEditorViewportAction;

EEditorViewportModeType ToModeType(EGizmoMode InMode)
{
	switch (InMode)
	{
	case EGizmoMode::Translate: return EEditorViewportModeType::Translate;
	case EGizmoMode::Rotate: return EEditorViewportModeType::Rotate;
	case EGizmoMode::Scale: return EEditorViewportModeType::Scale;
	default: return EEditorViewportModeType::Select;
	}
}

TArray<int32> GetCommandActionCandidates()
{
	return {
		static_cast<int32>(EAction::CycleMode),
		static_cast<int32>(EAction::SetModeSelect),
		static_cast<int32>(EAction::SetModeTranslate),
		static_cast<int32>(EAction::SetModeRotate),
		static_cast<int32>(EAction::SetModeScale),
		static_cast<int32>(EAction::CycleGizmoMode),
		static_cast<int32>(EAction::FocusSelection),
		static_cast<int32>(EAction::DeleteSelection),
		static_cast<int32>(EAction::SelectAll),
		static_cast<int32>(EAction::NewScene),
		static_cast<int32>(EAction::LoadScene),
		static_cast<int32>(EAction::SaveScene),
		static_cast<int32>(EAction::SaveSceneAs),
		static_cast<int32>(EAction::DuplicateSelection)
	};
}
}

bool FEditorViewportCommandTool::HandleInput(float DeltaTime)
{
	(void)DeltaTime;
	if (!Owner || !Controller)
	{
		return false;
	}
	if (Owner->GetRoutedInputContext().bImGuiCapturedKeyboard)
	{
		return false;
	}

	int32 TriggeredActionId = 0;
	if (!InputBindingUtils::TryGetHighestPriorityTriggeredAction(
		Owner->GetRoutedInputContext(),
		EditorViewportInputMapping::GetBindings(),
		GetCommandActionCandidates(),
		TriggeredActionId))
	{
		return false;
	}

	const EAction Action = static_cast<EAction>(TriggeredActionId);
	switch (Action)
	{
	case EAction::CycleMode:
		return Controller->CycleMode();
	case EAction::SetModeSelect:
		return SetMode(EEditorViewportModeType::Select);
	case EAction::SetModeTranslate:
		return SetMode(EEditorViewportModeType::Translate);
	case EAction::SetModeRotate:
		return SetMode(EEditorViewportModeType::Rotate);
	case EAction::SetModeScale:
		return SetMode(EEditorViewportModeType::Scale);
	case EAction::CycleGizmoMode:
		return TryCycleGizmoMode();
	case EAction::FocusSelection:
		return FocusPrimarySelection();
	case EAction::DeleteSelection:
		return DeleteSelectedActors();
	case EAction::SelectAll:
		return SelectAllActors();
	case EAction::NewScene:
		return NewScene();
	case EAction::LoadScene:
		return LoadScene();
	case EAction::SaveScene:
		return SaveScene();
	case EAction::SaveSceneAs:
		return SaveSceneAs();
	case EAction::DuplicateSelection:
		return DuplicateSelection();
	default:
		return false;
	}
}

bool FEditorViewportCommandTool::SetMode(EEditorViewportModeType InModeType)
{
	return Controller ? Controller->SetMode(InModeType) : false;
}

bool FEditorViewportCommandTool::FocusPrimarySelection()
{
	if (!Owner || !Owner->GetSelectionManager() || !Controller)
	{
		return false;
	}

	AActor* PrimarySelection = Owner->GetSelectionManager()->GetPrimarySelection();
	if (!PrimarySelection)
	{
		return false;
	}

	FEditorNavigationTool* NavigationTool = Controller->GetNavigationTool();
	if (!NavigationTool)
	{
		return false;
	}

	NavigationTool->FocusOnTarget(PrimarySelection->GetActorLocation());
	return true;
}

bool FEditorViewportCommandTool::DeleteSelectedActors()
{
	if (!Owner || !Owner->GetSelectionManager())
	{
		return false;
	}

	const TArray<AActor*> SelectedActors = Owner->GetSelectionManager()->GetSelectedActors();
	if (SelectedActors.empty())
	{
		return false;
	}

	for (AActor* Actor : SelectedActors)
	{
		if (!Actor)
		{
			continue;
		}

		if (UWorld* ActorWorld = Actor->GetWorld())
		{
			ActorWorld->DestroyActor(Actor);
		}
	}

	Owner->GetSelectionManager()->ClearSelection();
	return true;
}

bool FEditorViewportCommandTool::SelectAllActors()
{
	if (!Owner || !Owner->GetSelectionManager())
	{
		return false;
	}

	UWorld* World = Owner->ResolveInteractionWorld();
	if (!World)
	{
		return false;
	}

	const TArray<AActor*>& Actors = World->GetActors();
	if (Actors.empty())
	{
		return false;
	}

	Owner->GetSelectionManager()->ClearSelection();
	bool bSelectedAny = false;
	for (AActor* Actor : Actors)
	{
		if (!Actor || !Actor->IsVisible())
		{
			continue;
		}
		Owner->GetSelectionManager()->ToggleSelect(Actor);
		bSelectedAny = true;
	}
	return bSelectedAny;
}

bool FEditorViewportCommandTool::NewScene()
{
	UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine);
	if (!EditorEngine)
	{
		return false;
	}

	EditorEngine->NewScene();
	return true;
}

bool FEditorViewportCommandTool::LoadScene()
{
	UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine);
	return EditorEngine ? EditorEngine->LoadSceneWithDialog() : false;
}

bool FEditorViewportCommandTool::SaveScene()
{
	UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine);
	return EditorEngine ? EditorEngine->SaveScene() : false;
}

bool FEditorViewportCommandTool::SaveSceneAs()
{
	UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine);
	return EditorEngine ? EditorEngine->SaveSceneAsWithDialog() : false;
}

bool FEditorViewportCommandTool::DuplicateSelection()
{
	if (!Owner || !Owner->GetSelectionManager())
	{
		return false;
	}

	const TArray<AActor*> ToDuplicate = Owner->GetSelectionManager()->GetSelectedActors();
	if (ToDuplicate.empty())
	{
		return false;
	}

	TArray<AActor*> NewSelection;
	for (AActor* Src : ToDuplicate)
	{
		if (!Src)
		{
			continue;
		}

		AActor* Dup = Cast<AActor>(Src->Duplicate(nullptr));
		if (Dup)
		{
			NewSelection.push_back(Dup);
		}
	}

	if (NewSelection.empty())
	{
		return false;
	}

	Owner->GetSelectionManager()->ClearSelection();
	for (AActor* Actor : NewSelection)
	{
		Owner->GetSelectionManager()->ToggleSelect(Actor);
	}
	return true;
}

bool FEditorViewportCommandTool::TryCycleGizmoMode()
{
	if (!Owner || !Owner->GetGizmo())
	{
		return false;
	}

	Owner->GetGizmo()->SetNextMode();
	if (Controller)
	{
		Controller->SetMode(ToModeType(Owner->GetGizmo()->GetMode()));
	}
	return true;
}
