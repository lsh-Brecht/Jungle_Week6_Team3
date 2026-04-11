#include "Editor/Viewport/FLevelViewportLayout.h"

#include "Editor/EditorEngine.h"
#include "Editor/Input/EditorNavigationTool.h"
#include "Editor/Input/EditorViewportController.h"
#include "Editor/Input/EditorViewportModes.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Selection/SelectionManager.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Render/Pipeline/Renderer.h"
#include "Viewport/Viewport.h"
#include "UI/SSplitter.h"
#include "Math/MathUtils.h"
#include "Platform/Paths.h"
#include "ImGui/imgui.h"
#include "WICTextureLoader.h"
#include "Component/CameraComponent.h"
#include "Component/GizmoComponent.h"

#include <algorithm>

// ─── 레이아웃별 슬롯 수 ─────────────────────────────────────

int32 FLevelViewportLayout::GetSlotCount(EViewportLayout Layout)
{
	switch (Layout)
	{
	case EViewportLayout::OnePane:          return 1;
	case EViewportLayout::TwoPanesHoriz:
	case EViewportLayout::TwoPanesVert:     return 2;
	case EViewportLayout::ThreePanesLeft:
	case EViewportLayout::ThreePanesRight:
	case EViewportLayout::ThreePanesTop:
	case EViewportLayout::ThreePanesBottom: return 3;
	default:                                return 4;
	}
}

// ─── 아이콘 파일명 매핑 ──────────────────────────────────────

static const wchar_t* GetLayoutIconFileName(EViewportLayout Layout)
{
	switch (Layout)
	{
	case EViewportLayout::OnePane:          return L"ViewportLayout_OnePane.png";
	case EViewportLayout::TwoPanesHoriz:   return L"ViewportLayout_TwoPanesHoriz.png";
	case EViewportLayout::TwoPanesVert:    return L"ViewportLayout_TwoPanesVert.png";
	case EViewportLayout::ThreePanesLeft:  return L"ViewportLayout_ThreePanesLeft.png";
	case EViewportLayout::ThreePanesRight: return L"ViewportLayout_ThreePanesRight.png";
	case EViewportLayout::ThreePanesTop:   return L"ViewportLayout_ThreePanesTop.png";
	case EViewportLayout::ThreePanesBottom:return L"ViewportLayout_ThreePanesBottom.png";
	case EViewportLayout::FourPanes2x2:    return L"ViewportLayout_FourPanes2x2.png";
	case EViewportLayout::FourPanesLeft:   return L"ViewportLayout_FourPanesLeft.png";
	case EViewportLayout::FourPanesRight:  return L"ViewportLayout_FourPanesRight.png";
	case EViewportLayout::FourPanesTop:    return L"ViewportLayout_FourPanesTop.png";
	case EViewportLayout::FourPanesBottom: return L"ViewportLayout_FourPanesBottom.png";
	default:                               return L"";
	}
}

// ─── 아이콘 로드/해제 ────────────────────────────────────────

void FLevelViewportLayout::LoadLayoutIcons(ID3D11Device* Device)
{
	if (!Device) return;

	std::wstring IconDir = FPaths::Combine(FPaths::RootDir(), L"Asset/Editor/Icons/");

	for (int32 i = 0; i < static_cast<int32>(EViewportLayout::MAX); ++i)
	{
		std::wstring Path = IconDir + GetLayoutIconFileName(static_cast<EViewportLayout>(i));
		DirectX::CreateWICTextureFromFile(
			Device, Path.c_str(),
			nullptr, &LayoutIcons[i]);
	}
}

void FLevelViewportLayout::ReleaseLayoutIcons()
{
	for (int32 i = 0; i < static_cast<int32>(EViewportLayout::MAX); ++i)
	{
		if (LayoutIcons[i])
		{
			LayoutIcons[i]->Release();
			LayoutIcons[i] = nullptr;
		}
	}
}

// ─── Initialize / Release ────────────────────────────────────

void FLevelViewportLayout::Initialize(UEditorEngine* InEditor, FWindowsWindow* InWindow, FRenderer& InRenderer,
	FSelectionManager* InSelectionManager)
{
	Editor = InEditor;
	Window = InWindow;
	RendererPtr = &InRenderer;
	SelectionManager = InSelectionManager;

	// 아이콘 로드
	LoadLayoutIcons(InRenderer.GetFD3DDevice().GetDevice());

	// Play/Stop 툴바 초기화
	PlayToolbar.Initialize(InEditor, InRenderer.GetFD3DDevice().GetDevice());

	// LevelViewportClient 생성 (단일 뷰포트)
	auto* LevelVC = new FLevelEditorViewportClient();
	LevelVC->SetOverlayStatSystem(&Editor->GetOverlayStatSystem());
	LevelVC->SetSettings(&FEditorSettings::Get());
	LevelVC->Initialize(Window);
	LevelVC->SetViewportSize(Window->GetWidth(), Window->GetHeight());
	LevelVC->SetGizmo(SelectionManager->GetGizmo());
	LevelVC->SetSelectionManager(SelectionManager);

	auto* VP = new FViewport();
	VP->Initialize(InRenderer.GetFD3DDevice().GetDevice(),
		static_cast<uint32>(Window->GetWidth()),
		static_cast<uint32>(Window->GetHeight()));
	VP->SetClient(LevelVC);
	LevelVC->SetViewport(VP);

	LevelVC->CreateCamera();
	LevelVC->ResetCamera();

	AllViewportClients.push_back(LevelVC);
	LevelViewportClients.push_back(LevelVC);
	SetActiveViewport(LevelVC);

	ViewportWindows[0] = new SWindow();
	LevelVC->SetLayoutWindow(ViewportWindows[0]);
	ActiveSlotCount = 1;
	CurrentLayout = EViewportLayout::OnePane;
}

void FLevelViewportLayout::Release()
{
	SSplitter::DestroyTree(RootSplitter);
	RootSplitter = nullptr;
	DraggingSplitter = nullptr;

	for (int32 i = 0; i < MaxViewportSlots; ++i)
	{
		delete ViewportWindows[i];
		ViewportWindows[i] = nullptr;
	}

	ActiveViewportClient = nullptr;
	for (FEditorViewportClient* VC : AllViewportClients)
	{
		if (FViewport* VP = VC->GetViewport())
		{
			VP->Release();
			delete VP;
		}
		delete VC;
	}
	AllViewportClients.clear();
	LevelViewportClients.clear();

	ReleaseLayoutIcons();
	PlayToolbar.Release();
}

// ─── 활성 뷰포트 ────────────────────────────────────────────

void FLevelViewportLayout::SetActiveViewport(FLevelEditorViewportClient* InClient)
{
	if (ActiveViewportClient)
	{
		ActiveViewportClient->SetActive(false);
	}
	ActiveViewportClient = InClient;
	if (ActiveViewportClient)
	{
		ActiveViewportClient->SetActive(true);
		UWorld* World = Editor->GetWorld();
		if (World && ActiveViewportClient->GetCamera())
		{
			World->SetActiveCamera(ActiveViewportClient->GetCamera());
		}
	}
}

void FLevelViewportLayout::SetWorld(UWorld* /*InWorld*/)
{
	// World는 GEngine->GetWorld() 경유로 조회되므로 별도 저장이 필요 없음.
	// 호환성을 위해 시그니처만 유지.
}

void FLevelViewportLayout::ResetViewport(UWorld* InWorld)
{
	for (FLevelEditorViewportClient* VC : LevelViewportClients)
	{
		VC->CreateCamera();
		VC->ResetCamera();

		// 카메라 재생성 후 현재 뷰포트 크기로 AspectRatio 동기화
		if (FViewport* VP = VC->GetViewport())
		{
			UCameraComponent* Cam = VC->GetCamera();
			if (Cam && VP->GetWidth() > 0 && VP->GetHeight() > 0)
			{
				Cam->OnResize(static_cast<int32>(VP->GetWidth()), static_cast<int32>(VP->GetHeight()));
			}
		}

		// 기존 뷰포트 타입(Ortho 방향 등)을 새 카메라에 재적용
		VC->SetViewportType(VC->GetRenderOptions().ViewportType);
	}
	if (ActiveViewportClient && InWorld)
		InWorld->SetActiveCamera(ActiveViewportClient->GetCamera());
}

void FLevelViewportLayout::DestroyAllCameras()
{
	for (FEditorViewportClient* VC : AllViewportClients)
	{
		VC->DestroyCamera();
	}
}

void FLevelViewportLayout::DisableWorldAxisForPIE()
{
	if (bHasSavedWorldAxisVisibility)
	{
		for (int32 i = 0; i < ActiveSlotCount && i < static_cast<int32>(LevelViewportClients.size()); ++i)
		{
			LevelViewportClients[i]->GetRenderOptions().ShowFlags.bGrid = false;
			LevelViewportClients[i]->GetRenderOptions().ShowFlags.bWorldAxis = false;
		}
		return;
	}

	for (int32 i = 0; i < MaxViewportSlots; ++i)
	{
		SavedGridVisibility[i] = false;
		SavedWorldAxisVisibility[i] = false;
	}

	for (int32 i = 0; i < ActiveSlotCount && i < static_cast<int32>(LevelViewportClients.size()); ++i)
	{
		FViewportRenderOptions& Opts = LevelViewportClients[i]->GetRenderOptions();
		SavedGridVisibility[i] = Opts.ShowFlags.bGrid;
		SavedWorldAxisVisibility[i] = Opts.ShowFlags.bWorldAxis;
		Opts.ShowFlags.bGrid = false;
		Opts.ShowFlags.bWorldAxis = false;
	}

	bHasSavedWorldAxisVisibility = true;
}

void FLevelViewportLayout::RestoreWorldAxisAfterPIE()
{
	if (!bHasSavedWorldAxisVisibility)
	{
		return;
	}

	for (int32 i = 0; i < ActiveSlotCount && i < static_cast<int32>(LevelViewportClients.size()); ++i)
	{
		LevelViewportClients[i]->GetRenderOptions().ShowFlags.bGrid = SavedGridVisibility[i];
		LevelViewportClients[i]->GetRenderOptions().ShowFlags.bWorldAxis = SavedWorldAxisVisibility[i];
	}

	bHasSavedWorldAxisVisibility = false;
}

// ─── 뷰포트 슬롯 관리 ───────────────────────────────────────

void FLevelViewportLayout::EnsureViewportSlots(int32 RequiredCount)
{
	// 현재 슬롯보다 더 필요하면 추가 생성
	while (static_cast<int32>(LevelViewportClients.size()) < RequiredCount)
	{
		int32 Idx = static_cast<int32>(LevelViewportClients.size());

		auto* LevelVC = new FLevelEditorViewportClient();
		LevelVC->SetOverlayStatSystem(&Editor->GetOverlayStatSystem());
		LevelVC->SetSettings(&FEditorSettings::Get());
		LevelVC->Initialize(Window);
		LevelVC->SetViewportSize(Window->GetWidth(), Window->GetHeight());
		LevelVC->SetGizmo(SelectionManager->GetGizmo());
		LevelVC->SetSelectionManager(SelectionManager);

		auto* VP = new FViewport();
		VP->Initialize(RendererPtr->GetFD3DDevice().GetDevice(),
			static_cast<uint32>(Window->GetWidth()),
			static_cast<uint32>(Window->GetHeight()));
		VP->SetClient(LevelVC);
		LevelVC->SetViewport(VP);

		LevelVC->CreateCamera();
		LevelVC->ResetCamera();

		AllViewportClients.push_back(LevelVC);
		LevelViewportClients.push_back(LevelVC);

		ViewportWindows[Idx] = new SWindow();
		LevelVC->SetLayoutWindow(ViewportWindows[Idx]);
	}
}

void FLevelViewportLayout::ShrinkViewportSlots(int32 RequiredCount)
{
	while (static_cast<int32>(LevelViewportClients.size()) > RequiredCount)
	{
		FLevelEditorViewportClient* VC = LevelViewportClients.back();
		int32 Idx = static_cast<int32>(LevelViewportClients.size()) - 1;
		LevelViewportClients.pop_back();

		for (auto It = AllViewportClients.begin(); It != AllViewportClients.end(); ++It)
		{
			if (*It == VC) { AllViewportClients.erase(It); break; }
		}

		if (ActiveViewportClient == VC)
			SetActiveViewport(LevelViewportClients[0]);

		if (FViewport* VP = VC->GetViewport())
		{
			VP->Release();
			delete VP;
		}
		VC->DestroyCamera();
		delete VC;

		delete ViewportWindows[Idx];
		ViewportWindows[Idx] = nullptr;
	}
}

// ─── SSplitter 트리 빌드 ─────────────────────────────────────

SSplitter* FLevelViewportLayout::BuildSplitterTree(EViewportLayout Layout)
{
	SWindow** W = ViewportWindows;

	switch (Layout)
	{
	case EViewportLayout::OnePane:
		return nullptr; // 트리 불필요

	case EViewportLayout::TwoPanesHoriz:
	{
		// H → [0] | [1]
		auto* Root = new SSplitterH();
		Root->SetSideLT(W[0]);
		Root->SetSideRB(W[1]);
		return Root;
	}
	case EViewportLayout::TwoPanesVert:
	{
		// V → [0] / [1]
		auto* Root = new SSplitterV();
		Root->SetSideLT(W[0]);
		Root->SetSideRB(W[1]);
		return Root;
	}
	case EViewportLayout::ThreePanesLeft:
	{
		// H → [0] | V([1]/[2])
		auto* RightV = new SSplitterV();
		RightV->SetSideLT(W[1]);
		RightV->SetSideRB(W[2]);
		auto* Root = new SSplitterH();
		Root->SetSideLT(W[0]);
		Root->SetSideRB(RightV);
		return Root;
	}
	case EViewportLayout::ThreePanesRight:
	{
		// H → V([0]/[1]) | [2]
		auto* LeftV = new SSplitterV();
		LeftV->SetSideLT(W[0]);
		LeftV->SetSideRB(W[1]);
		auto* Root = new SSplitterH();
		Root->SetSideLT(LeftV);
		Root->SetSideRB(W[2]);
		return Root;
	}
	case EViewportLayout::ThreePanesTop:
	{
		// V → [0] / H([1]|[2])
		auto* BottomH = new SSplitterH();
		BottomH->SetSideLT(W[1]);
		BottomH->SetSideRB(W[2]);
		auto* Root = new SSplitterV();
		Root->SetSideLT(W[0]);
		Root->SetSideRB(BottomH);
		return Root;
	}
	case EViewportLayout::ThreePanesBottom:
	{
		// V → H([0]|[1]) / [2]
		auto* TopH = new SSplitterH();
		TopH->SetSideLT(W[0]);
		TopH->SetSideRB(W[1]);
		auto* Root = new SSplitterV();
		Root->SetSideLT(TopH);
		Root->SetSideRB(W[2]);
		return Root;
	}
	case EViewportLayout::FourPanes2x2:
	{
		// H → V([0]/[2]) | V([1]/[3])
		auto* LeftV = new SSplitterV();
		LeftV->SetSideLT(W[0]);
		LeftV->SetSideRB(W[2]);
		auto* RightV = new SSplitterV();
		RightV->SetSideLT(W[1]);
		RightV->SetSideRB(W[3]);
		auto* Root = new SSplitterH();
		Root->SetSideLT(LeftV);
		Root->SetSideRB(RightV);
		return Root;
	}
	case EViewportLayout::FourPanesLeft:
	{
		// H → [0] | V([1] / V([2]/[3]))
		auto* InnerV = new SSplitterV();
		InnerV->SetSideLT(W[2]);
		InnerV->SetSideRB(W[3]);
		auto* RightV = new SSplitterV();
		RightV->SetRatio(0.333f);
		RightV->SetSideLT(W[1]);
		RightV->SetSideRB(InnerV);
		auto* Root = new SSplitterH();
		Root->SetSideLT(W[0]);
		Root->SetSideRB(RightV);
		return Root;
	}
	case EViewportLayout::FourPanesRight:
	{
		// H → V([0] / V([1]/[2])) | [3]
		auto* InnerV = new SSplitterV();
		InnerV->SetSideLT(W[1]);
		InnerV->SetSideRB(W[2]);
		auto* LeftV = new SSplitterV();
		LeftV->SetRatio(0.333f);
		LeftV->SetSideLT(W[0]);
		LeftV->SetSideRB(InnerV);
		auto* Root = new SSplitterH();
		Root->SetSideLT(LeftV);
		Root->SetSideRB(W[3]);
		return Root;
	}
	case EViewportLayout::FourPanesTop:
	{
		// V → [0] / H([1] | H([2]|[3]))
		auto* InnerH = new SSplitterH();
		InnerH->SetSideLT(W[2]);
		InnerH->SetSideRB(W[3]);
		auto* BottomH = new SSplitterH();
		BottomH->SetRatio(0.333f);
		BottomH->SetSideLT(W[1]);
		BottomH->SetSideRB(InnerH);
		auto* Root = new SSplitterV();
		Root->SetSideLT(W[0]);
		Root->SetSideRB(BottomH);
		return Root;
	}
	case EViewportLayout::FourPanesBottom:
	{
		// V → H([0] | H([1]|[2])) / [3]
		auto* InnerH = new SSplitterH();
		InnerH->SetSideLT(W[1]);
		InnerH->SetSideRB(W[2]);
		auto* TopH = new SSplitterH();
		TopH->SetRatio(0.333f);
		TopH->SetSideLT(W[0]);
		TopH->SetSideRB(InnerH);
		auto* Root = new SSplitterV();
		Root->SetSideLT(TopH);
		Root->SetSideRB(W[3]);
		return Root;
	}
	default:
		return nullptr;
	}
}

// ─── 레이아웃 전환 ──────────────────────────────────────────

void FLevelViewportLayout::SetLayout(EViewportLayout NewLayout)
{
	if (NewLayout == CurrentLayout) return;

	bool bWasOnePane = (CurrentLayout == EViewportLayout::OnePane);

	// 기존 트리 해제
	SSplitter::DestroyTree(RootSplitter);
	RootSplitter = nullptr;
	DraggingSplitter = nullptr;

	int32 RequiredSlots = GetSlotCount(NewLayout);
	int32 OldSlotCount = static_cast<int32>(LevelViewportClients.size());

	// 슬롯 수 조정
	if (RequiredSlots > OldSlotCount)
		EnsureViewportSlots(RequiredSlots);
	else if (RequiredSlots < OldSlotCount)
		ShrinkViewportSlots(RequiredSlots);

	// 분할 전환 시 새로 추가된 슬롯에 Top, Front, Right 순으로 기본 설정
	if (NewLayout != EViewportLayout::OnePane)
	{
		constexpr ELevelViewportType DefaultTypes[] = {
			ELevelViewportType::Top,
			ELevelViewportType::Front,
			ELevelViewportType::Right
		};
		// 기존 슬롯(또는 슬롯 0)은 유지, 새로 생긴 슬롯에만 적용
		int32 StartIdx = bWasOnePane ? 1 : OldSlotCount;
		for (int32 i = StartIdx; i < RequiredSlots && (i - 1) < 3; ++i)
		{
			LevelViewportClients[i]->SetViewportType(DefaultTypes[i - 1]);
		}
	}

	// 새 트리 빌드
	RootSplitter = BuildSplitterTree(NewLayout);
	ActiveSlotCount = RequiredSlots;
	CurrentLayout = NewLayout;
}

void FLevelViewportLayout::ToggleViewportSplit()
{
	if (CurrentLayout == EViewportLayout::OnePane)
		SetLayout(EViewportLayout::FourPanes2x2);
	else
		SetLayout(EViewportLayout::OnePane);
}

// ─── Viewport UI 렌더링 ─────────────────────────────────────

void FLevelViewportLayout::RenderViewportUI(float DeltaTime)
{
	(void)DeltaTime;
	bMouseOverViewport = false;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_None);

	ImVec2 ContentPos = ImGui::GetCursorScreenPos();
	ImVec2 ContentSize = ImGui::GetContentRegionAvail();

	if (ContentSize.x > 0 && ContentSize.y > 0)
	{
		FRect ContentRect = {
			ContentPos.x,
			ContentPos.y,
			ContentSize.x,
			ContentSize.y
		};

		// SSplitter 레이아웃 계산
		if (RootSplitter)
		{
			RootSplitter->ComputeLayout(ContentRect);
		}
		else if (ViewportWindows[0])
		{
			ViewportWindows[0]->SetRect(ContentRect);
		}

		// 각 ViewportClient에 Rect 반영 + 이미지 렌더
		for (int32 i = 0; i < ActiveSlotCount; ++i)
		{
			if (i < static_cast<int32>(LevelViewportClients.size()))
			{
				FLevelEditorViewportClient* VC = LevelViewportClients[i];
				VC->UpdateLayoutRect();
				VC->RenderViewportImage(VC == ActiveViewportClient);
			}
		}

		// 각 뷰포트 패인 상단에 툴바 오버레이 렌더
		for (int32 i = 0; i < ActiveSlotCount; ++i)
		{
			RenderPaneToolbar(i);
		}

		// 분할 바 렌더 (재귀 수집)
		if (RootSplitter)
		{
			TArray<SSplitter*> AllSplitters;
			SSplitter::CollectSplitters(RootSplitter, AllSplitters);

			ImDrawList* DrawList = ImGui::GetWindowDrawList();
			ImU32 BarColor = IM_COL32(80, 80, 80, 255);

			for (SSplitter* S : AllSplitters)
			{
				const FRect& Bar = S->GetSplitBarRect();
				DrawList->AddRectFilled(
					ImVec2(Bar.X, Bar.Y),
					ImVec2(Bar.X + Bar.Width, Bar.Y + Bar.Height),
					BarColor);
			}
		}

		// 입력 처리
		if (ImGui::IsWindowHovered())
		{
			ImVec2 MousePos = ImGui::GetIO().MousePos;
			FPoint MP = { MousePos.x, MousePos.y };

			// 마우스가 어떤 슬롯 위에 있는지
			for (int32 i = 0; i < ActiveSlotCount; ++i)
			{
				if (ViewportWindows[i] && ViewportWindows[i]->IsHover(MP))
				{
					bMouseOverViewport = true;
					break;
				}
			}

			// 분할 바 드래그
			if (RootSplitter)
			{
				if (ImGui::IsMouseClicked(0))
				{
					DraggingSplitter = SSplitter::FindSplitterAtBar(RootSplitter, MP);
				}

				if (ImGui::IsMouseReleased(0))
				{
					DraggingSplitter = nullptr;
				}

				if (DraggingSplitter)
				{
					const FRect& DR = DraggingSplitter->GetRect();
					if (DraggingSplitter->GetOrientation() == ESplitOrientation::Horizontal)
					{
						float NewRatio = (MousePos.x - DR.X) / DR.Width;
						DraggingSplitter->SetRatio(Clamp(NewRatio, 0.15f, 0.85f));
						ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
					}
					else
					{
						float NewRatio = (MousePos.y - DR.Y) / DR.Height;
						DraggingSplitter->SetRatio(Clamp(NewRatio, 0.15f, 0.85f));
						ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
					}
				}
				else
				{
					// 호버 커서 변경
					SSplitter* Hovered = SSplitter::FindSplitterAtBar(RootSplitter, MP);
					if (Hovered)
					{
						if (Hovered->GetOrientation() == ESplitOrientation::Horizontal)
							ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
						else
							ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
					}
				}
			}

			// 활성 뷰포트 전환 (분할 바 드래그 중이 아닐 때)
			if (!DraggingSplitter && (ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1)))
			{
				for (int32 i = 0; i < ActiveSlotCount; ++i)
				{
					if (i < static_cast<int32>(LevelViewportClients.size()) &&
						ViewportWindows[i] && ViewportWindows[i]->IsHover(MP))
					{
						if (LevelViewportClients[i] != ActiveViewportClient)
							SetActiveViewport(LevelViewportClients[i]);
						break;
					}
				}
			}
		}
	}

	ImGui::End();
	ImGui::PopStyleVar();
}

// ─── 각 뷰포트 패인 툴바 오버레이 ──────────────────────────

void FLevelViewportLayout::RenderPaneToolbar(int32 SlotIndex)
{
	if (SlotIndex >= MaxViewportSlots || !ViewportWindows[SlotIndex]) return;
	if (SlotIndex >= static_cast<int32>(LevelViewportClients.size())) return;

	const FRect& PaneRect = ViewportWindows[SlotIndex]->GetRect();
	if (PaneRect.Width <= 0 || PaneRect.Height <= 0) return;

	enum class EToolbarIcon : int32
	{
		Menu = 0,
		Select,
		Translate,
		Rotate,
		Scale,
		WorldSpace,
		LocalSpace,
		TranslateSnap,
		RotateSnap,
		ScaleSnap,
		Camera,
		Setting,
		Count
	};

	static ID3D11ShaderResourceView* ToolbarIcons[static_cast<int32>(EToolbarIcon::Count)] = {};
	static bool bToolbarIconsLoaded = false;
	if (!bToolbarIconsLoaded && RendererPtr)
	{
		auto GetToolbarIconFileName = [](EToolbarIcon Icon) -> const wchar_t*
		{
			switch (Icon)
			{
			case EToolbarIcon::Menu:          return L"Menu.png";
			case EToolbarIcon::Select:        return L"Select.png";
			case EToolbarIcon::Translate:     return L"Translate.png";
			case EToolbarIcon::Rotate:        return L"Rotate.png";
			case EToolbarIcon::Scale:         return L"Scale.png";
			case EToolbarIcon::WorldSpace:    return L"WorldSpace.png";
			case EToolbarIcon::LocalSpace:    return L"LocalSpace.png";
			case EToolbarIcon::TranslateSnap: return L"Translate_Snap.png";
			case EToolbarIcon::RotateSnap:    return L"Rotate_Snap.png";
			case EToolbarIcon::ScaleSnap:     return L"Scale_Snap.png";
			case EToolbarIcon::Camera:        return L"Camera.png";
			case EToolbarIcon::Setting:       return L"Show_Flag.png";
			default:                          return L"";
			}
		};

		ID3D11Device* Device = RendererPtr->GetFD3DDevice().GetDevice();
		const std::wstring IconDir = FPaths::Combine(FPaths::RootDir(), L"Asset/Editor/ToolIcons/");
		for (int32 i = 0; i < static_cast<int32>(EToolbarIcon::Count); ++i)
		{
			const std::wstring FilePath = IconDir + GetToolbarIconFileName(static_cast<EToolbarIcon>(i));
			DirectX::CreateWICTextureFromFile(Device, FilePath.c_str(), nullptr, &ToolbarIcons[i]);
		}
		bToolbarIconsLoaded = true;
	}

	auto GetToolbarIconRenderSize = [](EToolbarIcon Icon, float FallbackSize, float MaxIconSize) -> ImVec2
	{
		ID3D11ShaderResourceView* IconSRV = ToolbarIcons[static_cast<int32>(Icon)];
		if (!IconSRV)
		{
			return ImVec2(FallbackSize, FallbackSize);
		}

		ID3D11Resource* Resource = nullptr;
		IconSRV->GetResource(&Resource);
		if (!Resource)
		{
			return ImVec2(FallbackSize, FallbackSize);
		}

		ImVec2 IconSize(FallbackSize, FallbackSize);
		D3D11_RESOURCE_DIMENSION Dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
		Resource->GetType(&Dimension);
		if (Dimension == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
		{
			ID3D11Texture2D* Texture2D = static_cast<ID3D11Texture2D*>(Resource);
			D3D11_TEXTURE2D_DESC Desc{};
			Texture2D->GetDesc(&Desc);
			IconSize = ImVec2(static_cast<float>(Desc.Width), static_cast<float>(Desc.Height));
		}
		Resource->Release();

		if (IconSize.x > MaxIconSize || IconSize.y > MaxIconSize)
		{
			const float Scale = (IconSize.x > IconSize.y) ? (MaxIconSize / IconSize.x) : (MaxIconSize / IconSize.y);
			IconSize.x *= Scale;
			IconSize.y *= Scale;
		}
		return IconSize;
	};

	auto DrawToolbarTextButton = [](const char* Id, const char* Label, bool bPairFirst = false, bool bPairSecond = false) -> bool
	{
		const ImVec2 TextSize = ImGui::CalcTextSize(Label);
		const ImVec2 Padding = ImGui::GetStyle().FramePadding;
		const ImVec2 ButtonSize(TextSize.x + Padding.x * 2.0f, ImGui::GetFrameHeight());
		const bool bPressed = ImGui::InvisibleButton(Id, ButtonSize);
		const ImVec2 Min = ImGui::GetItemRectMin();
		const ImVec2 Max = ImGui::GetItemRectMax();
		const bool bHovered = ImGui::IsItemHovered();
		const bool bHeld = ImGui::IsItemActive();
		const ImU32 BgColor = ImGui::GetColorU32(bHeld ? ImGuiCol_ButtonActive : (bHovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button));
		ImDrawFlags RoundFlags = ImDrawFlags_RoundCornersAll;
		if (bPairFirst) RoundFlags = ImDrawFlags_RoundCornersLeft;
		if (bPairSecond) RoundFlags = ImDrawFlags_RoundCornersRight;
		ImGui::GetWindowDrawList()->AddRectFilled(Min, Max, BgColor, ImGui::GetStyle().FrameRounding, RoundFlags);
		const ImVec2 TextPos(Min.x + (ButtonSize.x - TextSize.x) * 0.5f, Min.y + (ButtonSize.y - TextSize.y) * 0.5f);
		ImGui::GetWindowDrawList()->AddText(TextPos, ImGui::GetColorU32(ImGuiCol_Text), Label);
		ImGui::GetWindowDrawList()->AddText(ImVec2(TextPos.x + 0.8f, TextPos.y), ImGui::GetColorU32(ImGuiCol_Text), Label);
		return bPressed;
	};

	auto DrawToolbarIconButton = [&GetToolbarIconRenderSize, &DrawToolbarTextButton](const char* Id, EToolbarIcon Icon, const char* FallbackLabel, float FallbackIconSize, float MaxIconSize, bool bPairFirst = false, bool bPairSecond = false) -> bool
	{
		ID3D11ShaderResourceView* IconSRV = ToolbarIcons[static_cast<int32>(Icon)];
		if (!IconSRV)
		{
			return DrawToolbarTextButton(Id, FallbackLabel, bPairFirst, bPairSecond);
		}

		const ImVec2 IconSize = GetToolbarIconRenderSize(Icon, FallbackIconSize, MaxIconSize);
		const ImVec2 Padding = ImGui::GetStyle().FramePadding;
		const ImVec2 ButtonSize(IconSize.x + Padding.x * 2.0f, ImGui::GetFrameHeight());
		const bool bPressed = ImGui::InvisibleButton(Id, ButtonSize);
		const ImVec2 Min = ImGui::GetItemRectMin();
		const ImVec2 Max = ImGui::GetItemRectMax();
		const bool bHovered = ImGui::IsItemHovered();
		const bool bHeld = ImGui::IsItemActive();
		const ImU32 BgColor = ImGui::GetColorU32(bHeld ? ImGuiCol_ButtonActive : (bHovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button));
		ImDrawFlags RoundFlags = ImDrawFlags_RoundCornersAll;
		if (bPairFirst) RoundFlags = ImDrawFlags_RoundCornersLeft;
		if (bPairSecond) RoundFlags = ImDrawFlags_RoundCornersRight;
		ImGui::GetWindowDrawList()->AddRectFilled(Min, Max, BgColor, ImGui::GetStyle().FrameRounding, RoundFlags);
		ImGui::GetWindowDrawList()->AddImage(
			(ImTextureID)IconSRV,
			ImVec2(Min.x + Padding.x, Min.y + (ButtonSize.y - IconSize.y) * 0.5f),
			ImVec2(Min.x + Padding.x + IconSize.x, Min.y + (ButtonSize.y + IconSize.y) * 0.5f));
		return bPressed;
	};

	char OverlayID[64];
	snprintf(OverlayID, sizeof(OverlayID), "##PaneToolbar_%d", SlotIndex);
	constexpr float PaneToolbarHeight = 34.0f;
	ImGui::SetNextWindowPos(ImVec2(PaneRect.X, PaneRect.Y));
	ImGui::SetNextWindowBgAlpha(1.0f);
	ImGui::SetNextWindowSize(ImVec2(PaneRect.Width, PaneToolbarHeight), ImGuiCond_Always);

	const ImGuiWindowFlags OverlayFlags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoScrollbar;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 4.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.0f, 2.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.13f, 0.16f, 0.98f));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.22f, 0.26f, 0.95f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.29f, 0.35f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.16f, 0.30f, 0.53f, 1.0f));
	ImGui::Begin(OverlayID, nullptr, OverlayFlags);
	{
		ImGui::PushID(SlotIndex);

		FLevelEditorViewportClient* VC = LevelViewportClients[SlotIndex];
		FViewportRenderOptions& Opts = VC->GetRenderOptions();
		FEditorViewportController* InputController = VC->GetInputController();
		FEditorNavigationTool* NavTool = InputController ? InputController->GetNavigationTool() : nullptr;
		const bool bOnePane = (CurrentLayout == EViewportLayout::OnePane);
		UGizmoComponent* Gizmo = Editor ? Editor->GetGizmo() : nullptr;
		FEditorSettings& Settings = FEditorSettings::Get();

		static bool GWorldSpaceState[MaxViewportSlots] = { true, true, true, true };
		static bool GTSnapEnabled[MaxViewportSlots] = { false, false, false, false };
		static bool GRSnapEnabled[MaxViewportSlots] = { false, false, false, false };
		static bool GSSnapEnabled[MaxViewportSlots] = { false, false, false, false };
		static int32 GTSnapIndex[MaxViewportSlots] = { 1, 1, 1, 1 };
		static int32 GRSnapIndex[MaxViewportSlots] = { 1, 1, 1, 1 };
		static int32 GSSnapIndex[MaxViewportSlots] = { 1, 1, 1, 1 };
		const char* TranslateSnapLabels[] = { "1", "5", "10", "50", "100" };
		const char* RotateSnapLabels[] = { "5", "10", "15", "30", "45" };
		const char* ScaleSnapLabels[] = { "0.1", "0.25", "0.5", "1.0", "5.0" };

		constexpr float ToolbarFallbackIconSize = 14.0f;
		constexpr float ToolbarMaxIconSize = 16.0f;

		auto DrawTransformIcon = [&](const char* Id, EToolbarIcon Icon, EEditorViewportModeType TargetMode) -> bool
		{
			const bool bSelected = (VC->GetInteractionMode() == TargetMode);
			if (bSelected)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.33f, 0.46f, 0.63f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.37f, 0.52f, 0.70f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.27f, 0.39f, 0.54f, 1.0f));
			}
			const bool bPressed = DrawToolbarIconButton(Id, Icon, Id, ToolbarFallbackIconSize, ToolbarMaxIconSize);
			if (bSelected)
			{
				ImGui::PopStyleColor(3);
			}
			return bPressed;
		};

		if (DrawTransformIcon("##SelectTool", EToolbarIcon::Select, EEditorViewportModeType::Select))
		{
			Opts.ShowFlags.bGizmo = false;
			VC->SetInteractionMode(EEditorViewportModeType::Select);
		}
		ImGui::SameLine();
		if (DrawTransformIcon("##TranslateTool", EToolbarIcon::Translate, EEditorViewportModeType::Translate))
		{
			Opts.ShowFlags.bGizmo = true;
			VC->SetInteractionMode(EEditorViewportModeType::Translate);
			if (Gizmo) Gizmo->SetTranslateMode();
		}
		ImGui::SameLine();
		if (DrawTransformIcon("##RotateTool", EToolbarIcon::Rotate, EEditorViewportModeType::Rotate))
		{
			Opts.ShowFlags.bGizmo = true;
			VC->SetInteractionMode(EEditorViewportModeType::Rotate);
			if (Gizmo) Gizmo->SetRotateMode();
		}
		ImGui::SameLine();
		if (DrawTransformIcon("##ScaleTool", EToolbarIcon::Scale, EEditorViewportModeType::Scale))
		{
			Opts.ShowFlags.bGizmo = true;
			VC->SetInteractionMode(EEditorViewportModeType::Scale);
			if (Gizmo) Gizmo->SetScaleMode();
		}

		ImGui::SameLine(0.0f, 10.0f);
		const ImVec2 SeparatorStart = ImGui::GetCursorScreenPos();
		const float SeparatorHeight = ImGui::GetFrameHeight() - 4.0f;
		ImGui::GetWindowDrawList()->AddLine(
			ImVec2(SeparatorStart.x, SeparatorStart.y + 2.0f),
			ImVec2(SeparatorStart.x, SeparatorStart.y + 2.0f + SeparatorHeight),
			IM_COL32(155, 155, 155, 255),
			1.0f);
		ImGui::Dummy(ImVec2(1.0f, ImGui::GetFrameHeight()));

		ImGui::SameLine(0.0f, 10.0f);
		const EToolbarIcon CoordinateIcon = GWorldSpaceState[SlotIndex] ? EToolbarIcon::WorldSpace : EToolbarIcon::LocalSpace;
		if (DrawToolbarIconButton("##CoordinateSpaceToggle", CoordinateIcon, GWorldSpaceState[SlotIndex] ? "World" : "Local", ToolbarFallbackIconSize, ToolbarMaxIconSize))
		{
			GWorldSpaceState[SlotIndex] = !GWorldSpaceState[SlotIndex];
			if (Gizmo) Gizmo->SetWorldSpace(GWorldSpaceState[SlotIndex]);
		}

		auto DrawSnapSection = [&](EToolbarIcon SnapIcon, const char* Prefix, bool& bEnabled, int32& ValueIndex, const char* const* Labels, int32 LabelCount)
		{
			ImGui::SameLine(0.0f, 6.0f);
			if (bEnabled)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.43f, 0.30f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.50f, 0.36f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.20f, 0.36f, 0.26f, 1.0f));
			}
			char ToggleID[48];
			snprintf(ToggleID, sizeof(ToggleID), "##%sSnapToggle_%d", Prefix, SlotIndex);
			if (DrawToolbarIconButton(ToggleID, SnapIcon, Prefix, ToolbarFallbackIconSize, ToolbarMaxIconSize, true, false))
			{
				bEnabled = !bEnabled;
			}
			if (bEnabled)
			{
				ImGui::PopStyleColor(3);
			}

			ImGui::SameLine(0.0f, 0.0f);
			char ValueButtonLabel[24];
			snprintf(ValueButtonLabel, sizeof(ValueButtonLabel), "%s ▼", Labels[ValueIndex]);
			char PopupID[40];
			snprintf(PopupID, sizeof(PopupID), "##%sSnapPopup_%d", Prefix, SlotIndex);
			char ValueBtnID[48];
			snprintf(ValueBtnID, sizeof(ValueBtnID), "##%sSnapValueBtn_%d", Prefix, SlotIndex);
			if (DrawToolbarTextButton(ValueBtnID, ValueButtonLabel, false, true))
			{
				ImGui::OpenPopup(PopupID);
			}
			if (ImGui::BeginPopup(PopupID))
			{
				for (int32 i = 0; i < LabelCount; ++i)
				{
					const bool bSelected = (ValueIndex == i);
					if (ImGui::RadioButton(Labels[i], bSelected))
					{
						ValueIndex = i;
					}
				}
				ImGui::EndPopup();
			}
		};

		if (bOnePane)
		{
			DrawSnapSection(EToolbarIcon::TranslateSnap, "T", GTSnapEnabled[SlotIndex], GTSnapIndex[SlotIndex], TranslateSnapLabels, IM_ARRAYSIZE(TranslateSnapLabels));
			DrawSnapSection(EToolbarIcon::RotateSnap, "R", GRSnapEnabled[SlotIndex], GRSnapIndex[SlotIndex], RotateSnapLabels, IM_ARRAYSIZE(RotateSnapLabels));
			DrawSnapSection(EToolbarIcon::ScaleSnap, "S", GSSnapEnabled[SlotIndex], GSSnapIndex[SlotIndex], ScaleSnapLabels, IM_ARRAYSIZE(ScaleSnapLabels));
		}

		ImGui::SameLine();
		static const char* ViewportTypeNames[] = {
			"Perspective", "Top", "Bottom", "Left", "Right", "Front", "Back", "Free Orthographic"
		};
		const int32 CurrentTypeIdx = static_cast<int32>(Opts.ViewportType);
		const char* CurrentTypeName = ViewportTypeNames[CurrentTypeIdx];
		char VTPopupID[64];
		snprintf(VTPopupID, sizeof(VTPopupID), "ViewportTypePopup_%d", SlotIndex);
		char CameraPopupID[48];
		snprintf(CameraPopupID, sizeof(CameraPopupID), "##CameraSpeedPopup_%d", SlotIndex);
		char SettingsPopupID[64];
		snprintf(SettingsPopupID, sizeof(SettingsPopupID), "SettingsPopup_%d", SlotIndex);
		char LayoutPopupID[64];
		snprintf(LayoutPopupID, sizeof(LayoutPopupID), "LayoutPopup_%d", SlotIndex);

		auto CalcButtonWidth = [&GetToolbarIconRenderSize, ToolbarFallbackIconSize, ToolbarMaxIconSize](const char* Label, EToolbarIcon Icon, bool bIconButton) -> float
		{
			if (bIconButton)
			{
				const ImVec2 IconSize = GetToolbarIconRenderSize(Icon, ToolbarFallbackIconSize, ToolbarMaxIconSize);
				return IconSize.x + ImGui::GetStyle().FramePadding.x * 2.0f;
			}
			return ImGui::CalcTextSize(Label).x + ImGui::GetStyle().FramePadding.x * 2.0f;
		};

		const float RuntimeMultiplier = NavTool ? NavTool->GetRuntimeCameraSpeedMultiplier() : 1.0f;
		char CameraValueLabel[40];
		snprintf(CameraValueLabel, sizeof(CameraValueLabel), "Cam %.1fx ▼", Settings.CameraSpeed * RuntimeMultiplier);

		float RightWidth = 0.0f;
		int32 RightCount = 0;
		auto AddWidth = [&](float W)
		{
			if (RightCount > 0) RightWidth += ImGui::GetStyle().ItemSpacing.x;
			RightWidth += W;
			++RightCount;
		};
		AddWidth(CalcButtonWidth(CurrentTypeName, EToolbarIcon::Menu, false));
		if (bOnePane) AddWidth(CalcButtonWidth(CameraValueLabel, EToolbarIcon::Camera, false));
		AddWidth(CalcButtonWidth("Settings", EToolbarIcon::Setting, true));
		AddWidth(CalcButtonWidth("Layout", EToolbarIcon::Menu, true));
		AddWidth(CalcButtonWidth("Split", EToolbarIcon::Menu, true));

		const float RightStartX = ImGui::GetWindowContentRegionMax().x - RightWidth;
		if (RightStartX > ImGui::GetCursorPosX())
		{
			ImGui::SetCursorPosX(RightStartX);
		}

		if (DrawToolbarTextButton("##ViewportTypeBtn", CurrentTypeName))
		{
			ImGui::OpenPopup(VTPopupID);
		}
		if (ImGui::BeginPopup(VTPopupID))
		{
			for (int32 t = 0; t < static_cast<int32>(IM_ARRAYSIZE(ViewportTypeNames)); ++t)
			{
				const bool bSelected = (t == CurrentTypeIdx);
				if (ImGui::Selectable(ViewportTypeNames[t], bSelected))
				{
					VC->SetViewportType(static_cast<ELevelViewportType>(t));
				}
			}
			ImGui::EndPopup();
		}

		ImGui::SameLine();
		if (bOnePane && DrawToolbarTextButton("##CameraSpeedBtn", CameraValueLabel))
		{
			ImGui::OpenPopup(CameraPopupID);
		}
		if (bOnePane && ImGui::BeginPopup(CameraPopupID))
		{
			float CameraSpeed = Settings.CameraSpeed * RuntimeMultiplier;
			if (ImGui::SliderFloat("Speed", &CameraSpeed, FEditorNavigationTool::GetMinCameraSpeedValue(), FEditorNavigationTool::GetMaxCameraSpeedValue(), "%.1fx"))
			{
				Settings.CameraSpeed = CameraSpeed;
				if (NavTool)
				{
					NavTool->SetRuntimeCameraSpeedMultiplier(1.0f);
				}
			}
			ImGui::EndPopup();
		}

		ImGui::SameLine(0.0f, 8.0f);
		if (DrawToolbarIconButton("##SettingsIcon", EToolbarIcon::Setting, "Settings", ToolbarFallbackIconSize, ToolbarMaxIconSize))
		{
			ImGui::OpenPopup(SettingsPopupID);
		}
		if (ImGui::BeginPopup(SettingsPopupID))
		{
			ImGui::Text("View Mode");
			int32 CurrentMode = static_cast<int32>(Opts.ViewMode);
			ImGui::RadioButton("Lit", &CurrentMode, static_cast<int32>(EViewMode::Lit));
			ImGui::SameLine();
			ImGui::RadioButton("Unlit", &CurrentMode, static_cast<int32>(EViewMode::Unlit));
			ImGui::SameLine();
			ImGui::RadioButton("Wireframe", &CurrentMode, static_cast<int32>(EViewMode::Wireframe));
			ImGui::SameLine();
			ImGui::RadioButton("Depth", &CurrentMode, static_cast<int32>(EViewMode::Depth));
			Opts.ViewMode = static_cast<EViewMode>(CurrentMode);

			ImGui::Separator();
			ImGui::Text("Show");
			ImGui::Checkbox("Primitives", &Opts.ShowFlags.bPrimitives);
			ImGui::Checkbox("BillboardText", &Opts.ShowFlags.bBillboardText);
			ImGui::Checkbox("Grid", &Opts.ShowFlags.bGrid);
			ImGui::Checkbox("World Axis", &Opts.ShowFlags.bWorldAxis);
			ImGui::Checkbox("Gizmo", &Opts.ShowFlags.bGizmo);
			ImGui::Checkbox("Bounding Volume", &Opts.ShowFlags.bBoundingVolume);
			ImGui::Checkbox("Debug Draw", &Opts.ShowFlags.bDebugDraw);
			ImGui::Checkbox("Octree", &Opts.ShowFlags.bOctree);

			ImGui::Separator();
			ImGui::Text("Grid");
			ImGui::SliderFloat("Spacing", &Opts.GridSpacing, 0.1f, 10.0f, "%.1f");
			ImGui::SliderInt("Half Line Count", &Opts.GridHalfLineCount, 10, 500);

			ImGui::Separator();
			ImGui::Text("Camera");
			ImGui::SliderFloat("Move Sensitivity", &Opts.CameraMoveSensitivity, 0.1f, 5.0f, "%.1f");
			ImGui::SliderFloat("Rotate Sensitivity", &Opts.CameraRotateSensitivity, 0.1f, 5.0f, "%.1f");
			ImGui::EndPopup();
		}

		ImGui::SameLine();
		if (DrawToolbarIconButton("##LayoutMenuIcon", EToolbarIcon::Menu, "Layout", ToolbarFallbackIconSize, ToolbarMaxIconSize, true, false))
		{
			ImGui::OpenPopup(LayoutPopupID);
		}
		if (ImGui::BeginPopup(LayoutPopupID))
		{
			constexpr int32 LayoutCount = static_cast<int32>(EViewportLayout::MAX);
			constexpr int32 Columns = 4;
			constexpr float IconSize = 32.0f;
			for (int32 i = 0; i < LayoutCount; ++i)
			{
				ImGui::PushID(i);
				const bool bSelected = (static_cast<EViewportLayout>(i) == CurrentLayout);
				if (bSelected)
				{
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.33f, 0.46f, 0.63f, 1.0f));
				}
				bool bClicked = false;
				if (LayoutIcons[i])
				{
					bClicked = ImGui::ImageButton("##LayoutIcon", (ImTextureID)LayoutIcons[i], ImVec2(IconSize, IconSize));
				}
				else
				{
					char Label[8];
					snprintf(Label, sizeof(Label), "%d", i);
					bClicked = ImGui::Button(Label, ImVec2(IconSize + 8, IconSize + 8));
				}
				if (bSelected)
				{
					ImGui::PopStyleColor();
				}
				if (bClicked)
				{
					SetLayout(static_cast<EViewportLayout>(i));
					ImGui::CloseCurrentPopup();
				}
				if ((i + 1) % Columns != 0 && i + 1 < LayoutCount)
				{
					ImGui::SameLine();
				}
				ImGui::PopID();
			}
			ImGui::EndPopup();
		}

		ImGui::SameLine(0.0f, 0.0f);
		int32 ToggleIdx = (CurrentLayout == EViewportLayout::OnePane)
			? static_cast<int32>(EViewportLayout::FourPanes2x2)
			: static_cast<int32>(EViewportLayout::OnePane);
		if (LayoutIcons[ToggleIdx])
		{
			const ImVec2 IconSize = ImVec2(ToolbarMaxIconSize, ToolbarMaxIconSize);
			if (ImGui::ImageButton("##SplitToggleIcon", (ImTextureID)LayoutIcons[ToggleIdx], IconSize))
			{
				if (LevelViewportClients[SlotIndex] != ActiveViewportClient)
				{
					SetActiveViewport(LevelViewportClients[SlotIndex]);
				}
				ToggleViewportSplit();
			}
		}
		else
		{
			const char* ToggleLabel = (CurrentLayout == EViewportLayout::OnePane) ? "Split" : "Merge";
			if (DrawToolbarTextButton("##SplitToggleTextBtn", ToggleLabel, false, true))
			{
				if (LevelViewportClients[SlotIndex] != ActiveViewportClient)
				{
					SetActiveViewport(LevelViewportClients[SlotIndex]);
				}
				ToggleViewportSplit();
			}
		}

		ImGui::PopID();
	}
	ImGui::End();
	ImGui::PopStyleColor(4);
	ImGui::PopStyleVar(4);
}

// ─── FEditorSettings ↔ 뷰포트 상태 동기화 ──────────────────

void FLevelViewportLayout::SaveToSettings()
{
	FEditorSettings& S = FEditorSettings::Get();

	S.LayoutType = static_cast<int32>(CurrentLayout);

	// 뷰포트별 렌더 옵션 저장
	for (int32 i = 0; i < ActiveSlotCount && i < static_cast<int32>(LevelViewportClients.size()); ++i)
	{
		S.SlotOptions[i] = LevelViewportClients[i]->GetRenderOptions();
	}

	// Splitter 비율 저장
	if (RootSplitter)
	{
		TArray<SSplitter*> AllSplitters;
		SSplitter::CollectSplitters(RootSplitter, AllSplitters);
		S.SplitterCount = static_cast<int32>(AllSplitters.size());
		if (S.SplitterCount > 3) S.SplitterCount = 3;
		for (int32 i = 0; i < S.SplitterCount; ++i)
		{
			S.SplitterRatios[i] = AllSplitters[i]->GetRatio();
		}
	}
	else
	{
		S.SplitterCount = 0;
	}

	// Perspective 카메라 (slot 0) 저장
	if (!LevelViewportClients.empty())
	{
		UCameraComponent* Cam = LevelViewportClients[0]->GetCamera();
		if (Cam)
		{
			S.PerspCamLocation = Cam->GetWorldLocation();
			S.PerspCamRotation = Cam->GetRelativeRotation();
			const FCameraState& CS = Cam->GetCameraState();
			S.PerspCamFOV = CS.FOV * (180.0f / 3.14159265358979f); // rad → deg
			S.PerspCamNearClip = CS.NearZ;
			S.PerspCamFarClip = CS.FarZ;
		}
	}
}

void FLevelViewportLayout::LoadFromSettings()
{
	const FEditorSettings& S = FEditorSettings::Get();

	// 레이아웃 전환 (슬롯 생성 + 트리 빌드)
	EViewportLayout NewLayout = static_cast<EViewportLayout>(S.LayoutType);
	if (NewLayout >= EViewportLayout::MAX)
		NewLayout = EViewportLayout::OnePane;

	// OnePane이 아니면 레이아웃 적용 (Initialize에서 이미 OnePane으로 생성됨)
	if (NewLayout != EViewportLayout::OnePane)
	{
		// SetLayout 내부 bWasOnePane 분기를 피하기 위해 직접 전환
		SSplitter::DestroyTree(RootSplitter);
		RootSplitter = nullptr;
		DraggingSplitter = nullptr;

		int32 RequiredSlots = GetSlotCount(NewLayout);
		EnsureViewportSlots(RequiredSlots);

		RootSplitter = BuildSplitterTree(NewLayout);
		ActiveSlotCount = RequiredSlots;
		CurrentLayout = NewLayout;
	}

	// 뷰포트별 렌더 옵션 적용
	for (int32 i = 0; i < ActiveSlotCount && i < static_cast<int32>(LevelViewportClients.size()); ++i)
	{
		FLevelEditorViewportClient* VC = LevelViewportClients[i];
		VC->GetRenderOptions() = S.SlotOptions[i];

		// ViewportType에 따라 카메라 ortho/방향 설정
		VC->SetViewportType(S.SlotOptions[i].ViewportType);
	}

	// Splitter 비율 복원
	if (RootSplitter)
	{
		TArray<SSplitter*> AllSplitters;
		SSplitter::CollectSplitters(RootSplitter, AllSplitters);
		for (int32 i = 0; i < S.SplitterCount && i < static_cast<int32>(AllSplitters.size()); ++i)
		{
			AllSplitters[i]->SetRatio(S.SplitterRatios[i]);
		}
	}

	// Perspective 카메라 (slot 0) 복원
	if (!LevelViewportClients.empty())
	{
		UCameraComponent* Cam = LevelViewportClients[0]->GetCamera();
		if (Cam)
		{
			Cam->SetRelativeLocation(S.PerspCamLocation);
			Cam->SetRelativeRotation(S.PerspCamRotation);

			FCameraState CS = Cam->GetCameraState();
			CS.FOV = S.PerspCamFOV * (3.14159265358979f / 180.0f); // deg → rad
			CS.NearZ = S.PerspCamNearClip;
			CS.FarZ = S.PerspCamFarClip;
			Cam->SetCameraState(CS);
		}
	}
}
