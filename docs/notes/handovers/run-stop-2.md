# Run stop 2: pose-based Ganondorf stop clip - desktop, 2026-11-19 - clip DONE, in-game FAIL (gioco), stopped at 2 attempts (rule 5)
Brief: coordinator note (owner "A": pose-based run stop, Ganondorf in Smash) + project file handoffs/archivio/2026-11/run-start-stop-skid-brief.md section 2.
Owner style 2026-11-18: fluid and cartoony but in character, "a Ganondorf inside Astro Bot".

## Done
- Cascadeur access was declined (computer-use prompt), so the clip is authored as numbers and solved by the pipeline's IK poser
  (animpipe/author.py Poser): Tools/AnimPipeline/cards/runstop/run_stop_gen.py (curves, identity traits, timing; L and mirrored R with a
  GASP-like end step), run_stop_root.py (root track = sim brake), run_stop_plot.py (stick grid). Pipeline: gen -> ue_write_take.py --no-write
  -> run_stop_root.py -> Tools/GAS/run_stop2_write.py (headless; pilot copy AS_Pilot_RunStop_Ganon_*, final clip rewritten in place).
- Clips /Game/Blueprints/Animation/RunStop/AS_RunStop_Ganon_{L,R}_V1: 61 frames 60 fps, root motion on + root lock ref pose (as GASP).
- DA_AnimSet_Locomotion_Default.RunStop: ClipLeftFoot/RightFoot = the new clips, SettleRate 1.0 (settle authored at real time),
  SettleClipSeconds 0.467; Mode still OFF. Backup Saved/Backups/RunStop2/.
- ABP_GSTN_Locomotion: Tools/GAS/run_stop_states.py now takes the clips from env RUNSTOP_CLIP_L/R (default the new clips); before, the state
  played the GASP clips whatever the DA said. Backup Saved/Backups/RunStop2/ABP_GSTN_Locomotion.before.uasset. Compile up to date w/ warnings.
- C++: lean solver scale 0 while the stop clip plays (braking leaned the body back). Build Saved/RunStop2/build1.log
  (`grep -c "Result: Succeeded"` = 1). Tests not re-run (only the lean scale changed).
- Review: docs/notes/reviews/run-stop-2/REVIEW.md (FAIL gioco; clip = authored take to 0.1 cm).

## Open / next (code thread first, rule 10)
- gioco 1: same clip, different in-game pose run to run (v1c lean + very wide lunge, v2 upright, sprint sits into a deep crouch). Dump the
  final component pose in PIE vs the clip at the same clip time; suspects OffsetRootBone, StrideWarping, Procedural_Feet pinning, entry blend.
- Then re-capture (boot: Saved/RunStop2 capture used PIE_CAPTURE_SEQUENCES=mv_run_stop_base,mv_run_stop, views az0,az90,az270,az45,game,
  cvars gstn.RunStop.Mode 1) and a Fable review frame by frame. Mode = Clip only after a PASS.
- Not done: dust puff VFX at the plant (brief), sprint-length brake (clip authored for 470 cm/s; sprint enters at 756).
Cost: about 1 h of desktop session (Opus, 23:07-00:05Z); estimate not measured against INDICE.md.
