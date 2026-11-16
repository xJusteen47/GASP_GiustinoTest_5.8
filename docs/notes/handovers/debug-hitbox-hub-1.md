# DEBUG-HITBOX-HUB-1 handover (desktop, _ue59, 2026-11-16). PARTIAL: code done and tested, in-game debug draw still not seen
Branch `claude/debug-hitbox-hub` from `claude/mario-run-int` 4313f43d + merges of `claude/project-thread-ruaqdp` (debug options tab) and
`claude/project-thread-pkfl8v` (yaw offset), both clean. Build OK (`Saved/Logs/ci/build.log`), tests `GSTNCombat` 616 ok / 0 fail
(`Select-String Saved\Logs\ci\test.log "Result=\{Success\}"` count; OptionsWindow, KnockbackPreviewMatchesModel, DI.LaunchDirection green).
## Done
- Hitbox debug draw: last shapes linger `gstn.Combat.Debug.HitboxLinger` (0.35 s, darker), 2.5 px while open (was one frame, 1 px).
- New `gstn.Combat.Debug.Body` (owner): movement capsule white, pushbox cylinder blue, rigid core orange. Listed by the Debug Options tab.
- Timeline viewport: active hitboxes in front of mesh/floor, 1.5 px (the DAir sphere sits half under the floor at the feet, -93 cm from the
  root = the feet, offset is right); orange launch arrow per active hitbox (`UGSTN_HitMathUtils::LaunchDirection`, yaw offset included).
  `gstn.Combat.Timeline.OpenAtFrame N` opens a timeline editor on frame N (scripted shots).
- Validator: a Hitbox track's Part is a label (no hit code reads it), empty is no longer an error. ValidateAll 41 -> 22 errors
  (`Saved/Logs/ci/ui.log.ue.log`, "Validate all: 117 assets checked, 22 errors, 234 warnings"). The 22 left NOT analysed.
- Hub: `Tools/GAS/hub_debug_pad.py` removed `Z0_Pad_Debug` + `Z0_Pad_Debug_Text` (the only debug pad in the map; kit pads kept) and placed
  `Z0_Pad_DebugOptions` at (300, -800) by the spawn; L_Test_Hub saved (owner exception). Stepping on it runs `gstn.Debug.OpenWindow`
  (test_hub watcher, needs `h.prepare()`). Report `Saved/TestHub/debug_pad.txt`.
## Stomp on an enemy below (owner question)
`Saved/DebugHub/cap/dair_below/frames.jsonl` (sequence `dair_below`, new in pie_capture_move.py): dummy placed 20 cm ahead is pushed to 48 cm
on the ground before the jump; stomp hits at v92 with centres ~41 cm apart (dummy 11% -> 22%, actionState 2). In the air the pushbox does not
separate them (48 -> 37 cm while falling). So: YES it hits; pushbox/core do not stop it. A truly centred enemy is never reached because the
ground pushbox separates them first. Grid `Saved/DebugHub/cap/below_grid.png` (looked once).
## Not done / blocked
- In-game debug shapes NOT visible in either scene captures or PIE viewport HighResShot (`Saved/DebugHub/hires_grid.png`), though the cvars
  were 1 (`cap_dair2.log.ue.log`). 2 capture attempts used (rule 5). Next: one PIE by hand, `gstn.Combat.Debug.Body 1`; if nothing, breakpoint
  in `UGSTN_CombatSimComponent::TickComponent` debug block (is it reached? which world?). Cost guess 2 $.
- Stomp startup capsule: already reduced by data (10 Body Capsule tracks, `setup_dair_body_capsule.py`, visible in the timeline shot). Not
  changed; smaller = rerun with `DAIR_CAPSULE_HH` lower. Da confermare con l'owner whether it is small enough.
- Debug Options panel calls FindConsoleObject every tick (log perf warning): cache the objects. Small.
- `AM_OverlordDAirV9.uasset` dirty before this thread, untouched; PM_BoostJump etc. were not dirty here.
**Cost** estimate ~13 $ (Opus, 4 builds, 1 test run, 3 editor sessions).
## Follow-up: pad without h.prepare() (coordinator 2026-11-16 17:14)
- New native `AGSTN_DebugOptionsPad` (GSTNCombatDebug, not Shipping): red 240 cm cylinder, slow tick; when the local player's pawn is within
  120 cm it runs `gstn.Debug.OpenWindow` once (leave and step again to re-run). No script needed in Play. Builds OK (`Saved/Logs/ci/build.log`).
  `hub_debug_pad.py` and `build_test_hub_map.py` now spawn this actor.
- NOT in the map yet: both clean-Play runs (`Saved/Logs/ci/play.log`, `play2.log`, script `boot_play.py` in the thread scratchpad) hit
  TIMEOUT because the editor never ran its -ExecCmds `py` command (no "Cmd: py" line, Python itself started); the UI run an hour earlier did.
  2 attempts used (rule 5). Cause not found (blocking modal? restored layout?).
- To finish (owner or next thread, 2 minutes in the open editor, NOT in Play): Python console
  `import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import hub_debug_pad as p; p.run()` -> saves L_Test_Hub with the native pad;
  then a plain Play, walk onto the red pad left of the spawn: the GSTN Debug Options window opens.
