#include "Editor/Input/EditorViewportCommandTool.h"

#include "Editor/Input/EditorViewportInputMapping.h"
#include "Editor/Selection/SelectionManager.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "Editor/EditorEngine.h"
#include "Component/GizmoComponent.h"
#include "Engine/Runtime/Engine.h"
#include "GameFramework/AActor.h"

FEditorViewportCommandTool::FEditorViewportCommandTool(FEditorViewportClient* InOwner, FEditorViewportController* InController)
	: Owner(InOwner), Controller(InController)
{
}

bool FEditorViewportCommandTool::HandleInput(float DeltaTime)
{
	(void)DeltaTime;
	if (!Owner)
	{
		return false;
	}

	UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine);
	if (!EditorEngine)
	{
		return false;
	}

	bool bConsumed = false;

	if (EditorEngine->IsPlayingInEditor()
		&& EditorViewportInputMapping::IsTriggered(Owner->GetRoutedInputContext(), EditorViewportInputMapping::EEditorViewportAction::EndPIE))
	{
		EditorEngine->RequestEndPlayMap();
		bConsumed = true;
	}

	if (Owner->GetSelectionManager()
		&& EditorViewportInputMapping::IsTriggered(Owner->GetRoutedInputContext(), EditorViewportInputMapping::EEditorViewportAction::DuplicateSelection))
	{
		const TArray<AActor*> ToDuplicate = Owner->GetSelectionManager()->GetSelectedActors();
		if (!ToDuplicate.empty())
		{
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

			Owner->GetSelectionManager()->ClearSelection();
			for (AActor* Actor : NewSelection)
			{
				Owner->GetSelectionManager()->ToggleSelect(Actor);
			}
			bConsumed = true;
		}
	}

	if (Owner->GetGizmo()
		&& EditorViewportInputMapping::IsTriggered(Owner->GetRoutedInputContext(), EditorViewportInputMapping::EEditorViewportAction::CycleGizmoMode))
	{
		Owner->GetGizmo()->SetNextMode();
		bConsumed = true;
	}

	return bConsumed;
}
