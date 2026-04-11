#pragma once

#include "Editor/Input/EditorViewportTools.h"

class FEditorViewportClient;

class FEditorNavigationTool final : public IEditorViewportTool
{
public:
	explicit FEditorNavigationTool(FEditorViewportClient* InOwner);
	bool HandleInput(float DeltaTime) override;

private:
	FEditorViewportClient* Owner = nullptr;
};

