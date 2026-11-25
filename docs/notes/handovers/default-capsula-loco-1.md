# Handover default-capsula-loco-1: capsule and new locomotion by default, test kits hidden (2026-11-25)

Branch `claude/project-thread-zz1eir`, base `main` 152d25e6. Cloud thread, machines offline: NOT compiled yet.

ESITO: PARZIALE (codice scritto, build e prova sul fisso da fare)
SUBAGENTI: nessuno
COSTO: ~1,5 $ (stima, Opus, cloud)

## What
- `gstn.Greybox` default 0 -> 1 (`GSTN_GreyboxSubsystem.cpp`), `gstn.Loco.Base` default 1 -> 2 (`GSTN_LocoBaseSubsystem.cpp`). Owner 2026-11-25:
  no more typing them every Play. Divergence rows 26 and 27 updated.
- Harness keeps the old defaults: `Tools/CI/Run-Timed.ps1` adds `-ini:Engine:[ConsoleVariables]:gstn.Greybox=0,...gstn.Loco.Base=1` to every
  editor run (Test, Capture, -Exe UnrealEditor*), joined to an existing `-ini:Engine:` if there is one. Ini priority: a driver that sets the
  cvar from the console (PIE_CAPTURE_CVARS, greybox captures) still wins. `-PlaytestLook` skips the pin.
- Debug Options window, kit row: `FGSTN_DebugKitPicker::GetMenuKitNames` hides kits under a `_Test` / `Test` folder
  (`/Game/GSTN/_Test/Combat/*`, piggyback plugin `Test/`); `gstn.Debug.ShowTestKits 1` lists them (reopen the window). `gstn.Debug.Kit`,
  `KitAtStart` and the harness still accept every kit. Overlord kits (KIT_Test_Playtest...) and `_Examples` stay listed.

## Desktop steps
1. Editor closed, `Tools\CI\Run-Timed.ps1 -Preset Build -Log Saved\Logs\ci\build.log`.
2. `Tools\CI\Run-Timed.ps1 -Preset Test -Log Saved\Logs\ci\test.log` (the output must show the note "presentazione: vecchi default").
3. Open the editor normally, Play in L_Test_Hub without console commands: capsule on the simple graph. Debug Options: no `KIT_Test_Alpha` etc.

## Not checked
- The `-ini:Engine:[ConsoleVariables]` override reaching the cvars (same mechanism as Tools/Rendering/Open-Editor-Forward.ps1); check step 2's
  log for `gstn.Loco.Base=1` lines.
