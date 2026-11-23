# Playtest fixes 1117b (desktop, _ue59, Opus, 2026-11-17 ~02:10). Branch `claude/playtest-fixes-1117` (on 87aa5bc4). Budget reached: stopped here.
Diag tool: `Saved/PF1117b/diag_pie.py` (runs in the OPEN editor via `python Saved/AnimR2/run_in_editor.py <it>`, never quits it; rows in Saved/PF1117b/diag/*.jsonl).
Owner video (48 MB) could not be fetched on this machine (tool limit): not watched.
## Done (data, `Tools/GAS/setup_playtest_fixes_1117b.py`, out Saved/PlaytestFixes/fixes_1117b.txt; rerun after setup_playtest_fixes_1117.py)
- JAB AFTER CROUCH, root cause: Command.Attack.CrouchNeutral (grounded) was bound to `GSTN_GA_TestFeetProbe`, a ~150 frame invisible test probe, in
  WPN_Test_OverlordDropkick (KIT_Test_Playtest's weapon) and WPN_Test_Overlord. Crouch+X started it, every jab was refused until it ended.
  Proof before: crouch.jsonl (old run) timeline 5 frames 81..234, taps 140/200 refused. After the fix: same script, crouch+X = jab, taps at 140 and
  200 start jabs (timelineStarted 3, 4). Probe binding removed; crouch+X on ground now = jab fallback (no down tilt exists: owner design).
- TL_Overlord_Dropkick AimPolicy Auto -> SnapToStick (Auto already resolved to a snap; explicit now).
## Found, NOT fixed (next thread)
- BACK AERIAL: the snap works (yaw 5 -> 175 on the move's first frame) but then the yaw drifts back to 0 at ~5 deg/frame for the whole move
  (crouch.jsonl frames 278..300, last run). Nothing holds the orientation during a timeline in the air (bAirTurnDuringMoves off, no steer phase),
  so Mover/GASP orientation intent pulls back to the old facing. Fix (C++, GSTN_CombatSimComponent.cpp ~l.909, input producer): while a timeline runs
  and no steer phase applies, HoldOrientationIntent(OwnerYaw). Needs a build = closing the editor (owner asked at 01:06Z, no answer yet).
- CAMERA: the play view is GASP's `CameraAsset_SandboxCharacter` (/Game/Blueprints/Cameras, GameplayCameraComponent), NOT the GSTN camera
  (measured camera-pawn distance 228 cm; a GSTN Follow override at 1100 cm changed nothing and was reverted, BP restored from git).
  Next: lengthen the boom in that camera asset's rig, or make the GSTN camera the view target (BP change, ask owner).
- LAUNCH / BOUNCE / "air hitstun but stays on ground" / no downed state: light kit Y special at 40% launches fine (strong.jsonl run 2: Falling,
  vz 1412, lands f157). X+stick at 40% = push back on the ground with AS_React_HitStand (correct). Not reproduced yet. Lead: FGSTN_LayeredMove_Knockback
  sets PreferredMode only on its first tick (l.54-60); if that tick is lost (hitstop end, a same-frame queued move) the victim stays Walking while the
  reaction is a launch. Next: diag with RT strong (GA_Example_Strong; IA_GASNPP_Strong is NOT a button of IC_GASNPP_Test, check how RT reaches it)
  at 0/40/100%, log mode + reaction kind per frame; then the ground bounce/knockdown (needs a tumble: K >= 1800).
- OWNER NEW ASKS (01:02Z, scope playtest kits only): knockback 30 deg from the ground, launching at 0% from a low value; check other angles;
  Simple hurtbox too tall/below floor/too wide (HBS_Test_Pawn?, SimpleOnly volume) - fit to the silhouette, keep the between-the-legs fill.
  Note: "launch at 0%" needs base >= LaunchThreshold 750 (global rule) per move, or a per-move launch flag: owner decision if the global rule moves.
- DAIR V9 jerk (head height in the charge, scope: dair charge only): not started. Not locked (no docs/notes/anim-approvate.md).
## Log (Saved/Logs/GASP_Giustino2.log, `grep -E "Error:|Warning:"` -> Saved/PF1117b/log_summary.txt): 16 errors, 49 warnings.
Matter: 6x "Invalid GameplayTag None" in KitTags/WeaponTags (KIT_Test_Playtest, KIT_Test_Hits, Mount kits, WPN_Test_Hits, WPN_Test_OverlordDropkick):
an empty tag entry in the container, harmless at runtime, clean with a resave. 14x LogAutomationTest "Condition failed" at editor start (engine,
before PIE). Rest: engine/sample noise (SmartObjects icon, console perf warnings, Python name clashes, input options .sav missing on first run).
Not committed: AM_OverlordDAirV9.uasset (dirty before), _ue59/, jab/director/. Cost estimate ~16 $ (Opus, no build, ~12 short PIE runs).
