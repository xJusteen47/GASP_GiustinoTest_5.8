# Debug Options window (editor tab) — handover 1

Owner ask 2026-11-16: the debug menu goes in a separate EDITOR window, like GASP's options window in the base map, not on the game screen.
Branch `claude/project-thread-ruaqdp` (base `claude/integ-night-1116` 194ad7e3). Cloud, Opus medium, NOT compiled (desktop busy).

## How GASP does it (read on the desktop, GameAnimationSample/Content/Widgets, strings only)
- `GameAnimationWidget` is an Editor Utility Widget (Blutility) opened as an editor tab by `EUB_OpenWidget` (SpawnAndRegisterTab), which a
  `LevelButton` actor in the level calls. Its rows are `EUW_SimpleConsoleButton/Dropdown/SpinBox`: ExecuteConsoleCommand on a CVar, refresh
  from `DataDrivenCVarEngineSubsystem`. So: editor tab + rows that set console variables.

## What we had
No on-screen combat menu existed: the switches were only CVars (`gstn.Combat.Debug.Hitboxes/Hurtboxes`, Minion, Camera, Held...), plus the
Gameplay Debugger (apostrophe) and the input-display corner overlay, both unchanged.

## What was done (choice applied: C++ Slate tab instead of an EUW asset, because no manual BP wiring and an asset can't be authored from cloud)
- `GSTNCombatDebug/Private/Window/SGSTN_DebugOptionsPanel.*`: lists every `gstn.*Debug*` variable by rule (no hand list), grouped by
  `gstn.<Group>.`, row by type, live values, writes with ECVF_SetByConsole; filter box, Refresh, Gameplay Debugger button (EnableGDT in PIE).
- `GSTNCombatDebugModule.cpp`: nomad tab in **Tools > Debug > GSTN Debug Options** (engine's Debug category) + command `gstn.Debug.OpenWindow`.
- Build.cs: `WorkspaceMenuStructure` (editor only). Test `GSTNCombat.Debug.OptionsWindow.ListsDebugVariables`. README section.
- Pad: an editor tab is mouse UI; pad toggles in game stay for the Rivals-style hitbox view (Input Action, separate thread).

## Build steps (desktop, _ue59, when free: mark _ue59/IN-USO.txt)
1. Merge this branch into the build branch; build editor target (script with 45 min timeout).
2. Run tests `GSTNCombat.Debug.OptionsWindow` and `GSTNCombat.Debug.Module` (30 min timeout).
3. Open editor, Tools > Debug > GSTN Debug Options; PIE in L_Test_Hub, tick Combat > Debug.Hitboxes: boxes appear; untick: gone.
Risks if it fails to compile: SExpandableArea named slots, SSpinBox optional min/max. Max 2 fix attempts.

## Open / da confermare (to the owner via coordinator)
- Auto-open the window when L_Test_Hub opens (GASP does it with a level button)? Default: not done, the docked tab is remembered by the layout.
