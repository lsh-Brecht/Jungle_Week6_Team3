# Week05 -> Week06 UI Migration Matrix

Last Updated: 2026-04-11

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
  - Status: `Partially migrated (Round 2)`
  - Migrated:
    - Main menu bar (`File/View/Help`)
    - Shortcut overlay modal (table layout from Week05)
    - Footer console bar
    - Animated console drawer
    - Footer timed notification line
  - Deferred:
    - Full Week05 toolbar replica (Week06 already uses viewport play toolbar)
    - Footer timed notification queue (needs dedicated log subsystem)

## Migration Buckets

- Ready to migrate directly
  - Console drawer UX
  - Shortcut overlay
  - View toggles in menu

- Adapter required
  - File menu features that need engine API (`Load/Save` public entrypoints)
  - Footer notification queue lifecycle

- Keep Week06 native
  - Scene/outliner panel implementation (`EditorSceneWidget`)
  - PIE play controls in viewport toolbar

## Next Batch

1. Add footer notification subsystem (`EditorFooterLogSystem`) and hook key actions (new/load/save/PIE toggle).
2. Add API-backed File menu commands once public editor engine entrypoints are exposed.
3. Unify command labels/shortcut docs with current input mapping (`EditorViewportInputMapping`).
