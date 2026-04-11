#pragma once

#include "Editor/Input/EditorViewportTools.h"

class FEditorViewportClient;
class FEditorViewportController;

class FEditorViewportCommandTool final : public IEditorViewportTool
{
public:
	FEditorViewportCommandTool(FEditorViewportClient* InOwner, FEditorViewportController* InController);
	bool HandleInput(float DeltaTime) override;

private:
	FEditorViewportClient* Owner = nullptr;
	FEditorViewportController* Controller = nullptr;
};

