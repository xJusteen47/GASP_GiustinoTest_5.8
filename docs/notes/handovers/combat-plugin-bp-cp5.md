# COMBAT-PLUGIN-BP CP5 handover (cloud, 2026-11-15)
Branch `claude/project-thread-fxjn45` from ckzv21 `deb4c840` (CP4). Code `6453a59b`. **NOT BUILT, no .uasset made**: the desktop runs the steps below after CP3's and CP4's.
**Done (editor module GSTNCombatEditor only; runtime untouched)**
- Content Browser > Add > Gameplay > Combat > **Combat Move** (`UGSTN_CombatMoveFactory`, Private/Moves): modal details dialog of `FGSTN_NewCombatMoveOptions`
  (template Strike/Action/Empty, parent class, frames sanitized never refused, montage, key tag, timeline inline or TL_ asset, move events unwired, kit/weapon hookup).
- Asset context menu of a move Blueprint: **Add to Kit or Weapon...** (same hookup struct, upsert by command+requirements, one transaction).
- `UGSTN_CombatMoveAuthoring` (Public/Moves, BlueprintCallable, never saves): CreateCombatMove, MakeStarterTimeline, SanitizeOptions, AddMoveToKit(Assets), UpsertBinding.
- `UGSTN_MoveGraphAuthoring::EventGraph` falls back to the first ubergraph page (the engine GA factory names it "Gameplay Ability Graph": CP3's B script may have needed it).
- Tests `GSTNCombat.Editor.MoveFactory.{StarterTimeline,BindingUpsert,CreatesAndBinds}` (3: `grep -c IMPLEMENT_SIMPLE_AUTOMATION_TEST .../GSTN_EditorTests_MoveFactory.cpp`).
- `Tools/GAS/check_combat_move_factory.py`: headless check via Python, makes 3 moves + binds a copy of KIT_Example, deletes everything, saves nothing.
- Build.cs: + ToolMenus (private), GSTNInputCore moved to public. README section + changelog 0.15.0.
- Docs: artifact **Guida mosse Blueprint** https://claude.ai/artifact/La2oqPzCkPdcqL2kERy9D6 (factory, data vs graph, double shot, visual hooks, validator rules,
  what is missing to publish); architecture artifact CLq2srEECQnd6bKDbBmm72 v5 (section 10, §5/§6 updated); designer guide DyiBrsycoRfXNSMoZUBxun v3 (pointer box).
  Copies on `claude/artifacts` eea93206 (rule 2; not on this branch).
**Design choices (applied, rule 6)**: asset class = UGameplayAbilityBlueprint with a plain K2 EventGraph (created via FKismetEditorUtilities, not the GA factory);
default inline timeline (TL_ asset opt-in); hookup optional and never touches input/project settings; starter Strike = example-jab numbers (r50, 80->160 cm, dmg 6, kb 500, 30 deg).
**Desktop steps (Haiku low, editor closed, max 2 fix attempts each)**
1. `_ue59`: after CP3+CP4 steps, merge `origin/claude/project-thread-fxjn45` (fast-forward from ckzv21), build the editor target (Run-Timed, 45 min).
2. Likely compile spots: `UFactory::GetMenuCategorySubMenus` override signature, `UToolMenus::UnRegisterStartupCallback(FDelegateHandle)`/`UnregisterOwner`,
   `FStructOnScope(UStruct*, uint8*)`, `UE::ContentBrowser::ExtendToolMenu_AssetContextMenu` include (ContentBrowserMenuContexts.h). Fix in place, push here.
3. Tests: `Automation RunTests GSTNCombat.Editor.MoveFactory` (3) + `GSTNCombat.Editor.MoveGraph` + `GSTNCombat.Editor.SimGraph` (regression). Log to `docs/notes/reviews/cp5-factory/`.
4. Headless: `UnrealEditor-Cmd <uproject> -run=pythonscript -script="<repo>/Tools/GAS/check_combat_move_factory.py" -unattended -nullrhi`; expect last line
   `RESULT: PASS (3 moves)`; copy `Saved/Examples/check_combat_move_factory.log` to the same review folder. Nothing to commit but logs.
5. Optional, rendering editor: Add > Gameplay > Combat shows "Combat Move"; dialog opens; right-click a GA_Example_* shows "Add to Kit or Weapon...". Screenshot once.
**Not done / next**: CP6 (public packaging) is an owner decision: fork vs stock GAS first (list + order in the guide, section "Cosa manca"); dialog/menu untested headless.
**Cost**: cloud Opus, ~140k context, no subagents, ~$15.
