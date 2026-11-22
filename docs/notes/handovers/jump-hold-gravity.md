# Jump: owner's hold-gravity model (cloud, Opus high, NOT compiled). Branch `claude/project-thread-qaz1xr` from `claude/project-thread-98dml5` 72d1afaf
## Owner's model (2026-11-17 23:58Z, "facsimile dei primi Mario 2D")
Instant impulse; within a max hold time, held = reduced gravity; released inside it = boosted gravity until descending or the max time;
then the descent gravity. Scope: every jump of the combatant's own (ground, coyote, air jumps), all kits.
## Audit: what it was
Already 90% there (FGSTN_JumpLogic, combat sync state, 62.5 Hz, predicted): rise/cut/fall scales, MinHold/MaxHold, cut ramp 3 f (owner 11-16).
Differences: (1) reaching MaxJumpHoldFrames counted as a RELEASE: cut gravity until the apex, not the descent gravity; (2) MinJumpHoldFrames 2
delayed a tap's cut; (3) the values were Mario 3D World (rise 0 = constant speed, cut = fall = 4.1), not SMB-shaped.
## Change
- `EGSTN_JumpGravityModel` (GSTN_JumpTypes.h): HoldWindow (default, owner) / CutUntilApex (the behaviour before). Per kit:
  `UGSTN_GA_PlatformJump::GravityModel` (Jump|Kit Data, next to CutRampFrames). New state flag `bHoldWindowOver` (1 bit on the wire).
- HoldWindow: at MaxHold the target becomes FallGravityScale; held to the end ramps rise->fall over CutRampFrames; a cut long before switches at once.
- Defaults (ini + kits + QA script; SMB1 x 5.625 cm/px, big Mario 32 px = 180 cm): JumpVelocity 1350, Rise 2.6, Cut 9.0, Fall 9.0,
  MinHold 0, MaxHold 30 f, FastFallGravityScale 12 (was 6 < fall 9: fast fall would have been slower), MaxFallSpeed 1500 / FastFall 2400 kept.
  Kits L/M/H: impulse 1540/1350/1170, cut=fall 7.9/9.0/11.6, rise 2.6.
- Expected (`python Tools/GAS/jump_hold_sim.py`, model of the same logic at 62.5 Hz): medium full 344 cm (1.9 h) apex f30, air 0.82 s,
  tap 107 cm; light 440/157; heavy 259/65. To go back to 3DW on a kit: GravityModel CutUntilApex + 1240/0/4.1/4.1/6.0/2/11.
- QA bars (movement_scenarios.py jump_full/jump_tap), test JSON `window_over`, docs combat.md + combat-m3-moves-kits.md updated.
- RUNNING JUMP (owner 00:07Z: option, OFF by default): `UGSTN_GA_PlatformJump::RunBoost` (FGSTN_JumpRunBoost, Core): ground jump impulse
  x lerp(1, 1.12, horizontal speed 470..850 cm/s) = up to ~+25 % height (SMB1). Speed from the Mover sim-state tracker (sync state), in the move creator.
## Tests added (not run: cloud)
`GSTNCombat.Core.Jump.RunBoost` (ramp, cap, off, step) and `GSTNCombat.Core.Jump.HoldWindowModel`: phases held/released/tap/apex, ramp, CutUntilApex option, apex comparison, resets, wire round trip.
Existing Jump/CutRamp tests are unaffected by design (checked by hand: their releases fall inside the window).
## Desktop steps (Haiku low; one editor, editor CLOSED for 3-4)
1. Build: `Tools/CI/Run-Timed.ps1 -Preset Build -Log Saved\Logs\ci\jumphold-build1.log` (UHT: new UENUM + UPROPERTY).
2. Tests: `Tools/CI/Run-Timed.ps1 -Preset Test -Log Saved\Logs\ci\jumphold-test1.log -Tests "GSTNCombat.Core.Jump+GSTNCombat.Core.Gravity+GSTNCombat.GAS"`.
3. Kit data: `UnrealEditor-Cmd <uproject> -run=pythonscript -script=Tools/GAS/setup_test_kit_archetypes.py`, then
   `setup_qa_movement_1117.py` and `setup_overlord_kit_shared_f4.py` (both read PRESET_STATS) -unattended -nullrhi.
4. `Tools/GASNPPTest/run_movement_qa.ps1 -Names move_jump_full,move_jump_tap,move_fast_fall` + gasnpp_test `variable_jump`.
5. Owner playtest: full / tap / mid hold; compare with a kit set to CutUntilApex; tick RunBoost on a kit's jump ability, jump at sprint 2.
## Open (owner, via coordinator): MaxHold 30 f vs no cap (SMB1 has none).
