# Run stop V6 dossier (2026-11-19, desktop _ue59, branch claude/locomotion-finish-3)

Previous review: docs/notes/reviews/locomotion-finish-3/review_fable_runstop_v4.md (FAIL: main item 1, R clip blended ALL channels to the
idle by STOP+22 so the late trunk/head/arm keys never played, 20 still frames; item 2 push started at full speed; tail too long).
Identity traits (brief handoffs/run-start-stop-skid-brief.md section 2): 1) front foot planted, facing forward, no turn; 2) the torso
rises only AFTER the feet stop, arms open low for balance; 3) soft parts last: head and arms land 3-4+ frames after the torso.

## Changes V4 -> V6 (Tools/AnimPipeline/cards/runstop/run_stop_gen.py)
1. R clip: upper body = true idle + mirrored offset from the idle (same timing as L, ends exactly on the game idle). Legs keep their
   own handover (unchanged). Check: per-frame max motion of head/hands/spine/pelvis in the clips now runs to f65 in BOTH (R had a
   dead tail from f53 in V4): L f60 4.2 cm, R f60 3.4 cm, both 0.8 at f65, 0 at f67-68.
2. Stop push eased in: the segments ending on STOP+3 (pelvis, trunk, head, hands) are "io" instead of "out".
3. Tail cut: 69 frames (was 73), 2 still frames after the last key.
Also new since V4 (not a pose change): curves contact_l/r + movedata_speed and Foley Run_L/R notifies on the clips (GASP parity), see
Saved/Routing/curves_author.txt. STOP = clip f31.

## Capture: Saved/RunStop6/cap1 (gstn.RunStop.Mode 1, KIT_Test_Playtest)
- mv_run_stop: stick released at about v180; mv_run_stop_base (base run stage): released about v55.
- Grids here: mv_run_stop_az90.png (v178-272 every 2nd frame), mv_run_stop_base_az90.png (v53-147).
- Videos (1080p, az90 | az45 | game): mv_run_stop_x1.mp4 / _x4.mp4 (from v170), mv_run_stop_base_x1.mp4 / _x4.mp4 (from v45).
- Frames: Saved/RunStop6/cap1/<seq>/<view>/fNNN.png, rows in <seq>/frames.jsonl (body: pelvis, head, hands, feet relative to the actor;
  'abp' logging failed, ignore it). Clip bones: Tools/AnimPipeline/run_stop_{L,R}_bones.json (component space, 69 frames).
- Which clip (L or R) played in each sequence is not logged: tell it from the feet (L clip = left foot in front at the plant).

## V6 (on top of V5; review of V5: docs/notes/reviews/runstop-v5/review_fable.md, FAIL: clip ended on the twisted GASP idle)
- End pose = the game's own idle measured in the V5 capture (Tools/AnimPipeline/cards/runstop/game_idle_component.json): feet step onto
  the game stance, upper body reaches it by an additive (pelvis yaw -15.4, trunk yaw -8.2, pelvis and hands onto the measured
  positions) ramped in over clip f31-f58. Offline: clip f68 is within 1.9 cm (head) / 0.5 cm (hands, feet, pelvis) of the game idle.
- Capture: Saved/RunStop6/cap1 (same sequences and frame numbers as V5). Grids and videos in this folder.
