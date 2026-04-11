#pragma once

#include "Core/CoreTypes.h"
#include "Engine/Input/InputTypes.h"

#include <functional>

struct FRect;

class FInputRouter
{
public:
	using FRectProvider = std::function<bool(FRect&)>;
	using FWorldResolver = std::function<UWorld*()>;

	void SetOwnerWindow(HWND InOwnerWindow) { OwnerWindow = InOwnerWindow; }
	void SetImGuiCaptureState(bool bCaptureMouse, bool bCaptureKeyboard);

	void ClearTargets();
	void RegisterTarget(
		FViewport* InViewport,
		FViewportClient* InClient,
		EInteractionDomain InDomain,
		FRectProvider InRectProvider,
		FWorldResolver InWorldResolver);

	bool Tick(FViewportInputContext& OutContext, FInteractionBinding& OutBinding);

private:
	struct FTargetEntry
	{
		FViewport* Viewport = nullptr;
		FViewportClient* Client = nullptr;
		EInteractionDomain Domain = EInteractionDomain::Editor;
		FRectProvider RectProvider;
		FWorldResolver WorldResolver;
	};

	static bool IsPointInRect(const POINT& Point, const FRect& Rect);
	FTargetEntry* FindEntryByViewport(FViewport* InViewport, FRect& OutRect);

private:
	TArray<FTargetEntry> Targets;
	HWND OwnerWindow = nullptr;

	FViewport* HoveredViewport = nullptr;
	FViewport* FocusedViewport = nullptr;
	FViewport* CapturedViewport = nullptr;

	bool bImGuiCaptureMouse = false;
	bool bImGuiCaptureKeyboard = false;
	uint64 InputFrameCounter = 0;
};
