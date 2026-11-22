# NIGHT-FOOTLOCK handover: feet planted while a montage blends back to idle, then a step (desktop, 2026-11-16)
Branch `claude/night-knockdown` after b7dd49f9. Folder `_ue59`, Opus high. Editor built (Run-Timed Build OK), ABP saved. Backup of the ABP before
the change: `Saved/Backups/GAS/footlock/SandboxCharacter_Mover_ABP.uasset`. Evidence in `docs/notes/reviews/night-footlock/`.
## What was done (brief route 2, isolated; route 1 rejected: Procedural_Feet is the GASP Control Rig, its pinning is on only while moving)
- New anim node `FGSTN_AnimNode_BlendOutFootLock` (GSTNAnimationCore) + editor node, in the main AnimGraph between Procedural_Feet and
  Procedural_FullBody (`abp_diff.txt`: only that node and its two links). Local-space node: when no foot is held it only evaluates its input
  and returns, so GASP foot placement/locomotion is untouched by construction. `a.GSTN.BlendOutFootLock 0` = pass through.
- Request in `UGSTN_CharacterAnimInstance` (game thread): window opens when the last montage stops driving (blend-out start or cut), closes for
  good if the capsule moves > 10 cm/s or after 1.2 s; a new montage releases instantly (its first-frame pop stays as before).
- Node: holds each foot's world transform (two-bone IK thigh-calf-foot), then when the montage weight is 0 and the input feet are still
  (< 10 cm/s for 4 frames) each foot > 4 cm away steps (0.15 s, 4 cm lift, farther foot first, second at 60%); closer feet blend 0.1 s.
  Capsule moving = 0.1 s blend back, no step. Helper `InsertPoseNodeAfter` in the authoring library; script was `fl_insert.py` (scratch).
- Capture logging: `fl` = [phases L*10+R, request, montage weight] in `pie_capture_move.py` and `pie_capture_react_ingame.py` (the react
  logger had a pre-existing bug: `mesh.get_relative_location()` raised, so `mesh_rel`/`root_cs` were never written; fl now has its own try).
  `PIE_CAPTURE_CVARS` added to the move capture. Readers: `footlock_rows.py` (slide vs step after a montage), `footlock_grid.py`.
## Results (lock off = cvar 0, same build; `rows_off.txt`, `rows_on3.txt`; `python Tools/GASNPPTest/footlock_rows.py <capture dirs>`)
- Slide while the capsule is still, L/R cm, off -> on: jab1 17.9/37.6 -> 0.0/0.6; jab1_jab2_faf 17.9/37.7 -> 0.2/0.6; hitstand 18.1/44.6 ->
  0.0/3.9; knockdown_getup 12.6/35.6 -> 0.0/1.1. The remaining distance is covered by the steps (8-13 cm per foot, hitstand R 31.6).
- Timeline (`phases_on3.txt`): lock on the frame after the montage stops, steps at +39-40 frames, free at +48-53. With the first tuning
  (settle 3 cm/s x6) steps came at +47; off-run feet move > 12 cm/s until ~+30 (GASP idle settle), so the hold ends ~10 frames after it.
- dair_land: the capsule moves within 8 frames of the end -> no lock, same as off. dropkick_hit (first tuning, `rows_on_dair_dropkick.txt`):
  locks, steps at once (feet > 40 cm from the game pose), then the capsule is pushed 13 cm and the feet blend back.
- Locomotion: walk_into and jump_only never engage the lock (phase 0 on every frame); summary rows match off within run noise
  (`rows_move_root.txt`: walk_into root max 5.6 vs 6.1, footL creep 4.3 vs 1.2; jump_only identical stats). Frame-by-frame diff not possible:
  runs are not frame-aligned (actor paths differ between sessions).
## Looked at (rule 10): `jab1_jab2_faf_az90.png`, `knockdown_getup_az45.png` + side-by-side mp4s. No knee pop or foot jitter seen; the
  character stands up with the montage's wide stance for ~0.2-0.3 s (feet planted), then the rear foot steps in. Not a PASS by owner yet.
## Open / for the owner
- Taste: the planted wide stance before the step (shorter = lower `SettleSpeed` hold / `FootLockMaxHold`; all values are node properties).
- Not captured: starts/stops/pivot/turn-in-place/crouch (no such sequence exists; the lock cannot engage without a montage end).
- hitstand R foot: 3.9 cm slide while locked (capsule drifts ~0.9 cm, leg near full reach); not investigated.
- Tool note: the insert editor run ended BLOCCATO (hung on quit after saving, LiveCoding pipe error); the save was complete.
Cost: ~9 $ estimated (no subagents).
