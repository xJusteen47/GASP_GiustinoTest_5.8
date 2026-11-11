# IO-2b: rest of the single button layout (laptop, 2026-11-11)
Branch `claude/laptop-io-2b` from `747ce8b9` (tip of `claude/desktop-io-2`). Handover before: `io-2.md`. Worked in the main laptop folder
(not a worktree: the laptop session cannot switch to one); editor was closed.
**Done** (code tip `5ab3262d`; the branch tip is the commit of this file):
1. `2d510738` shield: `Defence.Shield` drives the press binding (`GSTN_DefenceLibrary`), the hold window and the tech in tumble
   (`GSTN_TumbleLanding`) through `FGSTN_ShieldAbilitySpec::GetLayoutShieldButton()`. `HoldButton` on the spec is now empty by default
   (= the layout). The shield timeline is built on the ability CDO (before any GameRules), so `FGSTN_HoldWindowTrack` got `LayoutFunction`,
   resolved by the hook every tick (`ResolveHoldButton`). Same for the drill `FGSTN_BoreTrack` (`Spin.Drill`).
2. `5fd9b9fe` `Combat.Attack/Strong/Special/Crouch` (standard set), `Movement.Dash/Dodge/Interact` (movement set), `Link.Grab/Release/
   Pummel/ThrowAttack/ThrowStrong/ThrowSpecial/ThrowJump/Crouch/LatchAttack/SpecialGrab`. All in `DefaultGSTNInput.ini`, rows in
   `GSTNInput.Core.Layout.Defaults`. Defaults unchanged.
3. Right stick: the two "buttons" are `UGSTN_InputConfig::AutoAimStickKey` (Gamepad_Right2D) and `AutoReticleToggleKey` (R3): physical
   keys on the input config asset (not Project Settings), not logical buttons, so not layout functions. Left as is; IO-3 per-player rebinding
   is where they belong. Noted in the GSTNInput README.
4. `5ab3262d` GR-1 limit fixed: `FGSTN_ActiveGameRules::GetSerial()/OnChanged()` (GSTNCore). `UGSTN_LoadoutComponent` recompiles its
   table (same revision) on change and at BeginPlay if built under another serial (skips worlds tearing down); `UGSTN_CombatSimComponent`
   replaces a revision's table set built under an older serial. GSTNCombatGAS/NP now list GSTNCore (Build.cs + uplugin: link error otherwise).
**Not fixed, described**: the standard/movement slot set ASSETS are baked by `Tools/GAS/setup_gas_assets.py` from the builders, so they
carry the editor's default layout, not a match's GameRules. Making them follow needs a runtime remap at compile (new design, not done).
**Gate (laptop)**: build ok after 2 fixes (C4459 shadowing; missing GSTNCore link). Headless GSTN **1513/0**, 0 ensures
(`Saved/IO2b/`). SUITE `R31_ALL=1` on the laptop: **50/50 PASS** (`Saved/R31/report-io2b.txt`), 0 ensures. The editor then crashed at
QUIT, after the MCP bridge stopped (access violation at 0x0, no GSTN frame; crash `Saved/Crashes/UECC-...22584B76...`). The laptop has
older exit crashes of the same shape; the desktop should check whether its suite run quits cleanly on this tip.
**Unseen**: nobody made a GameRules asset with a Button layout piece and moved a function in PIE, nor watched a client receive the rules
after its loadout (the recompile path is untested by any scenario). Suggest a test scenario for it in the next thread.
No manual step for the owner; the "Cose da fare a mano" artifact is unchanged (merge tip did not move).
Cost: ~130k context, Opus, estimated ~$6-8. Wall clock ~1h40 (builds 3x, suite ~20 min).
