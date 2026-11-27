# Handover default-capsula-loco-1: capsule and new locomotion by default, test kits hidden (2026-11-25)

Branch `claude/project-thread-zz1eir`, base `main` 152d25e6. Code from the cloud thread; built, tested, and verified live on the desktop 2026-11-26 in the worktree `GASP_Giustino2_zz1eir` (not `_ue59`).

## What
- `gstn.Greybox` default `0 -> 1` (`GSTN_GreyboxSubsystem.cpp`), `gstn.Loco.Base` default `1 -> 2` (`GSTN_LocoBaseSubsystem.cpp`). Owner 2026-11-25: no more typing them every Play. Divergence rows 26 and 27 updated.
- Harness keeps the old defaults: `Tools/CI/Run-Timed.ps1` adds `-ini:Engine:[ConsoleVariables]:gstn.Greybox=0,[ConsoleVariables]:gstn.Loco.Base=1` to every editor run (`Test`, `Capture`, `-Exe UnrealEditor*`), joined to an existing `-ini:Engine:` if present. Ini priority: a driver that sets either cvar from the console (`PIE_CAPTURE_CVARS`, greybox captures) still wins. `-PlaytestLook` skips the pin.
- Debug Options window, kit row: `FGSTN_DebugKitPicker::GetMenuKitNames` hides kits under a `_Test` or `Test` folder (`/Game/GSTN/_Test/Combat/*`, `GSTNPiggyback/Content/Test/*`); `gstn.Debug.ShowTestKits 1` lists them (reopen the window to refresh `KitOptions`). `gstn.Debug.Kit`, `KitAtStart` and the harness still accept every kit. Overlord kits (`KIT_Test_Playtest*`, `KIT_Test_Overlord*`) and `_Examples` (`KIT_Example*`) stay listed.

## Desktop verification (2026-11-26)
- **Build (`build.log.esito`)**: `Tools\CI\Run-Timed.ps1 -Preset Build -Log Saved\Logs\ci\build.log` -> `ESITO: OK`, `durata=1249s`, `exit=0`, `REGRESS: OK 8 locked versions, 13 files unchanged`.
- **Automation tests (`test-zz1eir.log.esito` + `Check-TestLog.ps1`)**:
  - `Tools\CI\Run-Timed.ps1 -Preset Test -Log Saved\Logs\ci\test-zz1eir.log` -> `ESITO: OK` (`durata=70s`, `presentazione: vecchi default (gstn.Greybox 0, gstn.Loco.Base 1; -PlaytestLook per quelli nuovi)`).
  - `powershell -NoProfile -ExecutionPolicy Bypass -File "Tools\CI\Check-TestLog.ps1" -UeLog "Saved\Logs\ci\test-zz1eir.log.ue.log" -MinTests 700` -> `ESITO TEST: PASS  ok=735 fail=0 totale=735 (minimo 700)`.
- **PIE CVar checks (`cvar-playtest.log`, `cvar-default.log`)**:
  - `-PlaytestLook` (`cvar-playtest.log.esito`, `durata=69s`, `driver=OK`): `gstn.Greybox=1`, `gstn.Loco.Base=2`, `GSTN_GreyboxCapsule` active on all test characters, driver mesh swapped to `ABP_GSTN_SimpleLoco_C`.
  - Without `-PlaytestLook` (`cvar-default.log.esito`, `durata=78s`, `driver=OK`): `gstn.Greybox=0`, `gstn.Loco.Base=1`, `ABP_GSTN_StockMover_C`.
- **Live Debug Options UI verification (`pr52-debug-options.log.esito`)**:
  - Command: `Tools\CI\Run-Timed.ps1 -Preset Capture -PlaytestLook -Script <temp pr52_probe.py> -Log Saved\Logs\ci\pr52-debug-options.log` -> `ESITO: OK`, `durata=65s`, `exit=0`, `driver=OK`, `precheck=OK`.
  - PIE `L_Test_Hub` without manual console commands: `gstn.Greybox=1`, `gstn.Loco.Base=2`, `gstn.Debug.ShowTestKits=0`; `BP_GASNPP_TestCharacter_C_0` has `GSTN_GreyboxCapsule` visible (`True`) and `SkeletalMesh` (`visible=False`) with `ABP_GSTN_SimpleLoco_C`.
  - AssetRegistry / AssetManager `GSTN_PrimaryKit` inventory (`47` total): `23` `.uasset` under `/Game/GSTN/_Test/Combat/*`, `2` under `/GSTNPiggyback/Test/*` (scanned by `GSTNPiggybackRuntimeModule` / `mount_scenarios`), `18` under `/Game/GSTN/Characters/Overlord/Kits/*`, `4` under `/Game/GSTN/_Examples/*`.
  - UI `ShowTestKits 0 -> 1 -> 0` cycle (checkbox click in `GSTN Debug Options` + close/reopen tab + `SComboBox` scroll + PNG screenshots):
    - `ShowTestKits=0` (`pr52_debug_options_default.png`, `pr52_kits_hidden.png`): `22` kits visible (`4` `_Examples` + `18` `Overlord`), `0` `_Test`/`Test` kits.
    - `ShowTestKits=1` (`pr52_kits_shown.png`): all `47` kits visible (`+23 _Test/Combat` + `2 GSTNPiggyback/Test`).
    - `ShowTestKits=0` re-toggled (`pr52_kits_rehidden.png`): `22` kits visible again, `0` `_Test`/`Test` kits.
  - Artifacts saved in gitignored `Saved/Logs/ci/`: `pr52-debug-options.log.esito`, `pr52_debug_options_probe.json`, `pr52_debug_options_slate_dump.txt`, `pr52_debug_options_default.png`, `pr52_kits_hidden.png`, `pr52_kits_shown.png`, `pr52_kits_rehidden.png`.
  - Note: if `GSTN Debug Options` was left open in the editor's saved layout from a prior session, UE restores the Nomad tab at startup before `UAssetManager` finishes scanning (`(no kit)` until closed and reopened once AssetManager completes).

ESITO: PASS PR #52 verificata: build 1249s, GSTNCombat 735/0, PIE -PlaytestLook gstn.Greybox=1/gstn.Loco.Base=2, Debug Options nasconde i 25 kit _Test/Test (22 Overlord/_Examples visibili) con toggle UI ShowTestKits 0->1->0
SUBAGENTI: 2 explorer + 1 worker (M1 PR #52)
COSTO: 3.0 | 26 | 0
