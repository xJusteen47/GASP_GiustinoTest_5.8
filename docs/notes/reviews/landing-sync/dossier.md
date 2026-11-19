# Landing sync dossier (2026-11-19, desktop _ue59, branch claude/locomotion-finish-3)

## Owner report
A1b_atterraggio_stilizzato_vs_realistico.mp4: when the landing dust spawns the character is still in the jump/air pose.
Owner principle for landings (scope: aerial -> landing): near-hard cut, impact and weight.

## Cause (checked)
- Dust: GSTN_PresentationLogic.cpp ~L231 emits Land on the simulation frame where bGrounded goes false -> true.
- ABP_GSTN_Locomotion enters its landing state on the same change (JustLanded_Light/Heavy: MovementMode OnGround, last frame InAir).
- But all 32 landing rows of CHT_GSTN_LocomotionAnimations (GASP clips M_Relaxed_Jump_*_Land_*) had BlendTime 0.2 s QuadraticInOut:
  the landing pose was only about half in 6 frames later. Report: Saved/Routing/land_blend.txt (Tools/GAS/routing/land_blend.py).
## Fix (applied)
- BlendTime 0.05 s (3 frames at 60 fps) on those 32 rows, curve unchanged. Backup of the chooser: Saved/Backups/LandSync/.

## Captures (pie_capture_move.py mv_jump_full, KIT_Test_Playtest, satin look on in both)
- base = Saved/LandSync/satin_jump (blend 0.2). Capsule bottom reaches 2.1 cm at f80. Pelvis z: f80-82 101.6 (3 identical frames),
  f83 94.6, f86 81.7, f89 65.5, f93 58.9 (bottom). Dust first visible f84.
- fix = Saved/LandSync/blend005 (blend 0.05). Capsule bottom 2.1 cm at f78. Pelvis z: f78-79 95.0 (2 identical frames), f80 69.2,
  f81 59.7, f83 58.2 (bottom). Dust first visible f81.
  Numbers: frames.jsonl of each capture, body.pelvis_z / body.cap_bottom_z.
- Files here: base_{az90,game,az0}.png (f76-93), fix_{az90,game,az0}.png (f74-91), landing_sync_az90.mp4 (real speed, base left,
  fix right, aligned on the touchdown), landing_sync_az90_slow4x.mp4, landing_sync_game_slow4x.mp4.

## Known open item (not fixed, "gioco")
- On the touchdown frame the pose is frozen for 2-3 frames (identical bones, mesh on the floor in the straight-legged air pose).
  The run stop review V4 saw the same 2 frozen frames right before its clip starts (review_fable_runstop_v4.md item 5): looks like a
  general state-entry freeze of the Blend Stack, not specific to landings.
