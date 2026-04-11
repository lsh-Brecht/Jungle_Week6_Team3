#pragma once

#include "Core/CoreTypes.h"
#include "Editor/Input/EditorGizmoTool.h"
#include "Editor/Input/EditorSelectionTool.h"
#include "Editor/Input/EditorViewportTools.h"

#include <memory>

class FEditorViewportClient;

enum class EEditorViewportModeType : uint8
{
	Select
};

class IEditorViewportMode
{
public:
	virtual ~IEditorViewportMode() = default;
	virtual EEditorViewportModeType GetType() const = 0;
	virtual bool HandleGizmoInput(float DeltaTime) = 0;
	virtual bool HandleSelectionInput(float DeltaTime) = 0;
};

class FEditorSelectMode final : public IEditorViewportMode
{
public:
	explicit FEditorSelectMode(FEditorViewportClient* InOwner);
	EEditorViewportModeType GetType() const override { return EEditorViewportModeType::Select; }
	bool HandleGizmoInput(float DeltaTime) override;
	bool HandleSelectionInput(float DeltaTime) override;

private:
	FEditorViewportClient* Owner = nullptr;
	std::unique_ptr<FEditorGizmoTool> GizmoTool;
	std::unique_ptr<FEditorSelectionTool> SelectionTool;
};
