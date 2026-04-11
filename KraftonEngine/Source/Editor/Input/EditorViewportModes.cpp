#include "Editor/Input/EditorViewportModes.h"

#include "Editor/Input/EditorGizmoTool.h"
#include "Editor/Input/EditorSelectionTool.h"

FEditorSelectMode::FEditorSelectMode(FEditorViewportClient* InOwner)
	: Owner(InOwner)
{
	if (Owner)
	{
		GizmoTool = std::make_unique<FEditorGizmoTool>(Owner);
		SelectionTool = std::make_unique<FEditorSelectionTool>(Owner);
	}
}

bool FEditorSelectMode::HandleGizmoInput(float DeltaTime)
{
	return GizmoTool ? GizmoTool->HandleInput(DeltaTime) : false;
}

bool FEditorSelectMode::HandleSelectionInput(float DeltaTime)
{
	return SelectionTool ? SelectionTool->HandleInput(DeltaTime) : false;
}

