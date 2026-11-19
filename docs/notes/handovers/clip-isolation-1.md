# Clip isolation 1 (cloud, Opus, 2026-11-19 19:03-19:50 UTC) - offline clip-in-isolation gate + first results
Branch claude/project-thread-c0lfu3 (from claude/locomotion-finish-4 fcbed236, merged claude/project-thread-hdoy7s for
the skills split). _ue59 not touched. Owner 19:03Z: a clip that fails alone never passes in the full setup -> hard gate.

## Done
- Tools/AnimPipeline/clipcheck/: clip_check.py (offline bpy: plants/slide, floor, pops = spike/snap, loop seam,
  handoff vs reference pose, curves/root-motion flag from sidecar, one grid.png + metrics.json per clip),
  export_clips_ue.py (headless FBX + sidecar export of editor assets, NOT run yet), clips.json (all our locomotion
  clips + GASP references as calibration), README.md (run, thresholds, image).
- Skills: generic gate in skills-public/anim-reviewer §2 and anim-executor §3; commands in .claude/skills/gstn-anim-*/project.md.
  Account skill draft: docs/notes/skills/gstn-move-animation-clipcheck-edit.md (coordinator proposes).
- Results docs/notes/reviews/clip-check/ (command: python Tools/AnimPipeline/clipcheck/clip_check.py
  Tools/AnimPipeline/clipcheck/clips.json docs/notes/reviews/clip-check Dash_Ganon_r3_src RunStart_Frantic_r3_src):
  * Dash r3 source FBX: FAIL. Right foot dragged 6.2 cm f1-3 (root moves from f2, foot pinned from f3; was meant to be
    hidden by dust, puff spec). Toe snaps: ball_r 62 deg -> 0 in one frame at f10, ball_l 0-40-72-0 f21-23.
  * Run start r3 source FBX: FAIL. Finger _03 bones flip 120 deg for single frames almost every frame (in the FBX
    keys themselves: pinky_03_r w 1 -> 0.5 -> 1). Legs: only one-frame extremes (30 fps frantic cycle), planted feet
    move back at ~25-30 cm/s (stomp f1-8) then ~330 cm/s (f14+); not judged without the sim speed (game_speed_cms).
  * Crouch, air crouch, run stop L/R, skid, landing, imported dash/run start: NO SOURCE until the export runs.
- Known limits: thresholds calibrated only on these 2 clips; GASP calibration rows run after the export. Flat-foot
  heights 3.2/9.4 cm measured on the UEFN mannequin. Pop "spike" warns are frequent at 30 fps by nature.

## Next (desktop, editor closed, ~10 min, no _ue59 writes)
1. Merge this branch in a worktree; run export_clips_ue.py (README step 1); check export.log (17 rows, 15 asset rows).
2. Run clip_check.py on all rows (cloud or desktop); first look at GASP_* rows: if a GASP clip FAILs, fix the threshold.
3. Fix pass per clip in its generator (dash_ganon_gen.py: pin from f1 or start root at f3, ease toe release over 3+ f;
   run start: finger keys). Not done here: they are pose rounds (rule 10).
Cost: ~6 $ estimate (Opus thread, no subagents).
