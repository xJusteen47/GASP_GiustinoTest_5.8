# runloop-desktop: run start v4r9 paused, generic (stock GASP) run start switchable and captured, run loop prep (desktop, 2026-11-21)
Branch claude/runloop-desktop (from claude/runstart-entry-desktop fd2d1bc4). _ue59 lock taken 02:14Z, freed at the end; owner editor
closed with Tools/UE59/Close-OwnerEditor.ps1 (ESITO: CHIUSO) and reopened. Re-check (rule 11): runstart-entry "8 hand-off rows 0.1 s"
confirmed in the chooser dump (rows 6/8/10/12, blend 0.1, sampling 0-0.02) - but those assets were NEVER committed: done here (e07b473e).
## Owner direction during the thread (via coordinator, in order)
1. Do the RUN LOOP before redoing the run start: v4r9 (erect torso, lower elbows) PAUSED before any change. Nothing of v4r9 exists.
2. Generic run start for the plain GASP gait first ("John normale" = average Fortnite-style character), then Overlord specifics.
3. Owner rule (scope: run start of ALL playable characters): the frantic limb rev-up stays everywhere, it gives the feel; the hard
   part is not clashing with idle before and loop after. => the stock start alone is only the BASELINE, not the generic start.
## Done
- Game loop facts (capture frames.jsonl + open-editor dump): base run loop = BS_Relaxed_Run_Loop_F_Slopes (M_Relaxed_Run_Loop_F 88 f
  30 fps; slope up/down samples). Stock starts M_Relaxed_Run_Start_F_Lfoot/_Rfoot: 91 f, root 0 -> 375 cm/s by ~f19, contacts L f15
  R f26 L f36 ... (Saved/RunLoop/export/*.meta.json, stock_start_root.json). In game the capsule (DA ramp) reaches 470 cm/s at ~f110
  and crosses 243 cm (1.35 x height) ~45 sim frames = 0.72 s after the first move (diag_v4r7h frames.jsonl loc): the owner's 0.47 s
  first block is a MOVER/DA ramp question, not clip only (owner decision; DA_Locomotion_Default was changed by the owner's editor at
  01:57Z, left uncommitted, not touched).
- Generic variant: gstn.RunStart.Variant 4 = stock start rows re-enabled as siblings (stock blend); 14 (= 4 + Handoff 1) with 0.1 s
  Linear entry. runstart_generic_rows_ue.py via Save-Assets: "ESITO: OK generic rows enabled 4" (Saved/AnimPipeline/rows/
  runstart_generic.txt). Divergence row 20. No C++ change (the cvar takes any value).
- Captures runstart_generic_captures.ps1 -Handoff 1,0: both ASSERT: OK (M_Relaxed_Run_Start_F_* >= 0.8 on f97-140; Saved/
  RunStartGeneric/generic_h{1,0}/assert.txt). Evidence docs/notes/reviews/runstart-generic_h1 (grids + mp4 vs v4r7h).
  Planted-feet travel f80-240 (metrics_summary.json): v4r7h L/R 37.1/51.0 cm, generic_h1 43.3/93.5 -> the stock start on the Overlord
  kit SKATES on the right foot (stock root 375 cm/s vs capsule 470). NOT reviewed (no review thread yet): do not show the owner.
- Exports for Cascadeur: Saved/RunLoop/export/{Relaxed_RunLoop_F, Neutral_SprintLoop_F, Relaxed_RunStart_F_L/R}.fbx + meta.
  Scene Saved/AnimPipeline/runloop/RunLoop_Relaxed_v0_gaspZ.casc = rig of RunLoop_v0_gaspZ + M_Relaxed_Run_Loop_F imported up Z on
  f0-87; frames 88-130 still hold the OLD neutral loop (scene size 131, set_animation_size refused): trim before any export.
## Next (new thread, desktop, Opus high, ~15 $)
1. Generic frantic start (owner rule 3): Cascadeur scene from Relaxed_RunStart_F_L/R (import up Z like above) + frantic limb rev-up
   from frame 1 (fast arm pumps and short quick steps, as v4r8c's warp of the loop cycle: runstart_retime_casc_v4r8.py with
   TAIL_START on the stock loop), ending on M_Relaxed_Run_Loop_F poses (stock phase triangle), root on the capsule ramp to fix the
   R-foot skate. Brief needs 2-3 identity traits (rule 13a) from the owner/coordinator before the build.
2. Then the Overlord base loop per handoffs/overlord-run-loop-piano.md, then v4r9 on the new loop.
COSTO: ~9 $ | ~6 min desktop | 0 token review
