# Week05 -> Week06 UI Migration Matrix

Last Updated: 2026-04-12

## Scope

- Source reference: `Week05/Source/Editor/UI`
- Target: `Week06/Source/Editor/UI`
- Principle: keep Week06 architecture (InputRouter, Tool/Mode/Controller, PIE pipeline) and migrate UI features with adapters.

## File Mapping

- `EditorLevelWidget` (Week05) -> `EditorSceneWidget` (Week06)
  - Status: `Integrated`
  - Notes: Week06 already includes outliner + scene save/load/new, superseding LevelWidget.

- `EditorFooterLogSystem` (Week05)
  - Status: `Migrated (Round 2)`
  - Notes: Hooked into footer status bar for timed latest message display.

- `EditorConsoleWidget` enhancements (drawer toolbar/input split)
  - Status: `Migrated (Round 1)`
  - Notes: Added reusable drawer methods and backtick char filter.

- `EditorMainPanel` UX features
  - Status: `Partially migrated (Round 3)`
  - Migrated:
    - Main menu bar (`File/View/Help`)
    - Shortcut overlay modal (table layout from Week05)
    - Footer console bar
    - Animated console drawer
    - Footer timed notification line
    - File commands wired (`New/Load/Save/SaveAs`)
    - Footer right-side level path (`Level: <path>` / `Unsaved`)
  - Deferred:
    - Full Week05 toolbar replica (Week06 already uses viewport play toolbar)
    - Fine pixel-parity tuning for spacing/hover/overflow

## Migration Buckets

- Ready to migrate directly
  - Console drawer UX
  - Shortcut overlay
  - View toggles in menu

- Adapter required
  - Open asset folder UX/error handling polish
  - Footer notification queue lifecycle

- Keep Week06 native
  - Scene/outliner panel implementation (`EditorSceneWidget`)
  - PIE play controls in viewport toolbar

## Next Batch

1. Unify command labels/shortcut docs with current input mapping (`EditorViewportInputMapping`).
2. Complete Week05-style viewport toolbar behavior parity (state + interaction edge cases).
3. Manual multi-viewport/PIE regression pass and visual parity screenshot sweep.
