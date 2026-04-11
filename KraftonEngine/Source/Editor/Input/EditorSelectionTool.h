#pragma once

#include "Editor/Input/EditorViewportTools.h"

class FEditorViewportClient;

class FEditorSelectionTool final : public IEditorViewportTool
{
public:
	explicit FEditorSelectionTool(FEditorViewportClient* InOwner);
	bool HandleInput(float DeltaTime) override;

private:
	FEditorViewportClient* Owner = nullptr;
};

