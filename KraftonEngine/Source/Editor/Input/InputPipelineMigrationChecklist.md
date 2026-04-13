# Input Pipeline Migration Checklist (Week06)

Last Updated: 2026-04-11

## 1) Core Routing / Binding

- [x] Added `InteractionBinding` (`ReceiverVC`, `TargetWorld`, `Domain`)
- [x] Single-target VC routing with `InputRouter`
- [x] Capture-first routing during drag
- [x] Global shortcuts pre-handle path (ESC/F8)

## 2) VC Internal Dispatcher / Context

- [x] Priority-based context chain with first-consume
- [x] Context split: `Command / Gizmo / Selection / Navigation`
- [x] Chord mapping kept and expanded
- [x] Fine-tune consume precedence among overlapping contexts

## 3) Tool / Mode / Controller

- [x] Tool interface and concrete tools added
- [x] Mode interface and Select mode added
- [x] Mode expanded: `Select / Translate / Rotate / Scale`
- [x] Controller orchestration split from VC
- [x] Navigation tool upgraded (Orbit/Pan/Dolly/Wheel/Speed adjust)
- [x] Command tool upgraded (Focus/Delete/SelectAll/NewScene/Duplicate)
- [x] Viewport pane toolbar mode popup wired to VC mode API
- [x] Gizmo mode <-> interaction mode synchronization (toolbar + shortcut cycle)
- [ ] Add domain-specific modes (e.g., dedicated PIE editor tool mode)

## 4) PIE / EditorOnPIE Behavior

- [x] PIE player actor/camera lifecycle (`OnBeginPIE` / `OnEndPIE`)
- [x] F8 possess/eject toggle path
- [x] `EditorVC -> PIEWorld` interaction path
- [x] PIE starts in `Possessed` by default
- [x] Relative mouse mode state in router (activate/maintain/restore)
- [x] Cursor lock/hide while relative mouse mode is active
- [x] Raw mouse delta ingest (`WM_INPUT`) wired to input system
- [x] PIE quick release shortcut (`Shift+F1`) path
- [x] PIE input ownership and capture UX tuning
  - [x] Keep relative-mouse ownership stable via router fallback to current relative viewport
  - [x] Possessed/Eject LMB transition no longer collapses relative mode unexpectedly

## 5) Selection / Picking / Gizmo

- [x] Selection world rebind on PIE enter/exit
- [x] Entry viewport gizmo visibility restore
- [x] Selection tool no longer pre-consumes wheel zoom (wheel now routed to navigation)
- [ ] ID-buffer edge-case regression pass

## 6) Validation

- [x] Debug x64 build passed (2026-04-12, post Load/Save API wiring)
- [x] Previous Release x64 build passed
- [x] Rebuild after current batch edits
- [ ] Multi-viewport manual validation
- [ ] PIE focus switching validation
- [ ] F8 rapid toggle stress pass

## 7) Next Patch Batch

1. Add additional editor modes and connect mode cycle UI hooks.
2. Expand global command set needed by upcoming UI migration.
   - [x] `UEditorEngine` public `Load/Save/SaveAs` entrypoints exposed and key-mapped (`Ctrl+O/S/Shift+Ctrl+S`)
   - [x] File menu wired to real commands
   - [x] Open Asset Folder follow-up UX polishing (error feedback)
3. Tune PIE capture/focus transitions and camera feel.

## 8) Current Working Notes

- [x] Selection click trigger moved to `LButton Released` to reduce navigation pre-consume collisions.
- [x] Selection now ignores click-select on drag release (`PointerDragEnded`) and `Alt` chord.
- [x] Selection click behavior updated with `Ctrl+Click` toggle and non-modifier empty-click clear only.
- [x] Added `InputTrace` logs at VC context-dispatch stage (which context consumed, or none).
- [x] Added `EditorViewportInputUtils::IsLeftNavigationDragActive` (threshold-based LMB navigation drag split).
- [x] Selection tool upgraded with marquee box selection (`Ctrl+Alt+LMB` replace / `Ctrl+Alt+Shift+LMB` additive).
- [x] Cursor hide/lock behavior moved into dedicated `CursorControl` state utility and wired to router relative mode.
- [x] ImGui mouse-capture now blocks editor-side LMB navigation acquire (prevents splitter-drag + camera-rotate dual consume).
- [x] Marquee state ownership moved to `SelectionTool` (VC renders overlay by querying controller/mode tool state).
- [x] Selection no longer consumes LMB navigation-drag release; thresholded left-drag nav keeps precedence.
