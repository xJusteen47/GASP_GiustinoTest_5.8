# Locomotion finish 1: run start, skid, run stop (desktop _ue59, Opus, 2026-11-19) - context limit reached, handed back
Branch `claude/locomotion-finish-1`. Brief: coordinator note "finisci tutto" items 3-6. Crouch: crouch-veloce-4.md, crouch-veloce-5-air.md.

## 3. Run start - clip ON SCREEN now, Fable FAIL (gioco x2, posa x1): docs/notes/reviews/run-start-frantic/ingame2/review_fable.md
- Cause: the "Run Start" alias came only from idle states; GASP leaves idle on the push frame (our getter is one frame late), so the
  state entered on release, 70 frames late (Saved/Crouch/v9/run.log.ue.log: stamp=0 f829, "LocoState enter ... Run Start" f899).
- Fix (Tools/GAS/run_start/run_start_states.py v2): alias also from Locomotion Transition/Loop, entry only while 0 <= t < 0.25 s.
  Proof Saved/Crouch/v10/run.log.ue.log: stamp=0 f811, Run Start entered f812. Diagnosis cvar `gstn.RunStart.Log 1` (C++ log per tick;
  machine names print but state reads "None": GetStateMachineInstance(index) returns null on the main instance, harmless).
- Open: stamp leg lifts ~half the clip in game (foot pinning / stride warping at near-zero speed: gate them off while 0 <= t < 0.33);
  steps v63-97 slide probable (needs checker floor / foot world X log); hand-off clip f21-27 pelvis +12-15 cm in 5 f (posa, the r3 FBX
  from the Blender/agy card). Push in rs_start is at v40 (capture.log), not v20: no latency.

## 4. Skid - facing FIXED, Fable FAIL (gioco, narrow): docs/notes/reviews/skid-runstop-4/review_fable_skid.md
- Cause of the double turn: SS_180_Turn_Root180 had enable_root_motion False + force root lock, so its root 180 never reached Offset Root
  Bone (Accumulate). Set enable_root_motion True (asset saved). Capture Saved/Skid/lf1: one clean 180 f121-142.
- Open (attempt 2): feet slide 126/219 cm f139-172 because the state holds the clip while the sim restarts at f133. Fix A (reviewer):
  exit Skid at state elapsed ~0.42 s (AnimGetter GetInstanceCurrentStateElapsedTime) with 0.2 s blend; guard SkidEntry against
  re-entry (e.g. GetAnimOwnerSkidClipTime < 0.3). First dump the existing Skid exit transitions (GSTN_AnimBlueprintToolset).

## 5. Run stop - V2 curves authored, NOT written to Unreal yet
- Tools/AnimPipeline/cards/runstop/run_stop_gen.py: rise delayed and spread (trunk -28 at STOP -> -22 +8 -> -4 +18 -> +2 +25), pelvis
  stays low (no bump), arms/head follow later. Gen run (run_stop_<L|R>_bones.json). Next: ue_write_take.py <bones> AS_RunStop_Ganon_<s>_V2
  --no-write; run_stop_root.py <s> AS_RunStop_Ganon_<s>_V2; headless Tools/GAS/run_stop2_write.py with RUNSTOP_VERSION=V2; rerun
  Tools/GAS/routing/route_states.py (chooser rows read the DA); capture mv_run_stop (cvar gstn.RunStop.Mode 1, 230 frames, release v180).
  Base grid of the V1 stop after the skid fix: docs/notes/reviews/skid-runstop-4/stop_side.png + stop_lf1.mp4 (not reviewed).
- Dust on the plant: not started.

## 6. L3 dash - not started. Reference ready: branch claude/project-thread-ymsnu8 docs/notes/reference/dash-ganon/NOTE.md
  (traits: compact load + dust, held lunge f9-12, heavy settle). VFX: simple Smash-style puff (owner via coordinator). Ledge rule: brief
  docs/notes/reviews/run-start-frantic/brief.md lines 62-65.

## Also
- Gemini VFX peer review: docs/notes/reviews/agy-vfx-theory/esito.md (e04141c7).
- Stray EMPTY untracked file Tools/AnimPipeline/run_stop_gen.py (my mistake, delete was refused by permissions): delete it.
- ABP_GSTN_Locomotion backup before this thread: Saved/Backups/RunStart/ABP_GSTN_Locomotion_pre_lf1.uasset.
- Lock _ue59 released at the end of this thread. Cost: ~2 h desktop, Opus, ~150k context, 6 Fable review subagents (~600k tokens), 1 agy call.
