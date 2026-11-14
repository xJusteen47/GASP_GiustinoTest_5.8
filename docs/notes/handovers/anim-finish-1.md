# ANIM-FINISH-1 handover: victim reactions, first pass (desktop, UE 5.9, 2026-11-14)
Branch `claude/project-thread-yhnbvx`, folder `GASP_Giustino2_ue59` = comb59 `867f2b39` + n0mxg5 `636fd971` (one conflict in
`build_test_hub_map.py`: both pads kept). No C++ changed, so the playtest folder is still BUILT; editor closed, no map saved.
**Done 1/4 (reactions), not done: dropkick, down-air v9 blend-out, jab v3, any in-game Play capture.**
- **Editor link**: `Tools/AnimPipeline/ue.py` now goes through Unreal's built-in MCP (127.0.0.1:8000, `ue_builtin.py`): run_python_file,
  sample_pose, read_anim_sequence rebuilt on `execute_python`, same result shapes (sample_pose 0.0 diff vs the bridge). Not ported:
  `import_animation` (Cascadeur/FBX import); `UE_MCP_BACKEND=bridge` keeps the old path. Jab 1 v3 authored+built through it as the test.
- **Seven clips** `AS_React_{HitStand,Tumble,Splat,LieUp,GroundHit,GetUp,Roll}`: built in `/Game/AnimPipeline/Rig/Reaction/` (not in git),
  game copies in `Content/GASNPPTest/Combat/Reaction/` (in git). 60 fps; stills are 4 frames; GroundHit 18, GetUp 26, Roll 24.
- **How they are made**: `cards/reaction/reaction_<name>.plan.json` (key poses, world hand/foot targets, `rot_e` pitch) ->
  `python pose_plan.py <plan>` (card with a dense author block + stick preview + floor report, offline) -> `animpipe author <card>` ->
  `animpipe build reaction_<name>` -> `clip_grid.py` (grid) / `clip_video.py` (mp4 + sheet). After a build the level is dirty: run
  `EditorLoadingAndSavingUtils.new_blank_map(False)` before any capture. `animpipe check`/`render` do NOT apply (attack gates:
  legibility, clearance, contact_faf, planted_feet, fists, co_motion fail by construction); `status` keeps saying `check`: ignore it.
- **Review material** `docs/notes/reviews/react-v1/`: one mp4 per clip (right, back, 3/4 front, every frame) + sheets for the 3 moving
  clips. These are EDITOR captures, not Play: rule 10 is only half met and there is no older version to compare. I looked at every key
  pose (6 views for the stills, 4 for the moving clips) and at every frame of Roll: no pops, floor contact within 2 cm at the keys.
- **Still to do for the reactions**: cloud review of react-v1; in-game look (hub Z6 showcase in `build_test_hub_map.py`, or the TL
  Montage field of GA_GroundBounce_Test / GA_GetupRoll_Test / GA_GetupNeutral_Test); Hitstun/Tumble/Splat/lying need the code hook.
- **Choices to confirm (owner)**: Roll is a BACK roll (supine -> over the shoulders -> feet): the body travels toward the character's
  BACK, so the ability must turn the character's back to the stick (or we author a second clip). Always supine. In LieUp the pelvis is
  10 cm behind the root, head toward -Y; the capsule stays standing (game side). HitStand arms are close to symmetric: first refinement.
- **Free clips**: `_scratch\mixamo` is empty, so everything is authored by hand. Quaternius was not downloaded (a download needs the
  owner's ok in the thread; its hit/knockdown clips are unconfirmed).
- **Antigravity (Gemini 3.8 Flash High), what worked**: a short coding task with exact steps (`clip_video.py`, 69 lines) came back right
  at the first try and ran unchanged. What did not: one long run (write the script + 84 frames of editor captures + ffmpeg) returned
  empty after the bridge timeout with nothing written. So: give it one file or one short command per call, never a long batch; run long
  loops yourself. Pose refinement by numbered steps on a plan file was NOT tried (context ran out): try it first on HitStand.
- **Found in passing**: opening the editor rewrites `Plugins/GSTNPiggyback/Content/Test/PM_BoostJump|PM_ThrowRider.uasset` on disk and
  dirties `IC_GASNPP_Test` and `KIT_Test_Hits` in memory (reverted / not saved here): some startup script is not idempotent.
- **Next thread, in order**: (a) react-v1 review + in-game look, (b) dropkick (poses are in the Antigravity worktree, read-only),
  (c) down-air v9 blend-out, (d) jab v3 round 1 (jab 1 diagonal contact; jab 2 step 15-20 cm, net travel <= 5 cm, no magnet).
- **Cost**: one Fable thread, about 150k tokens of context, 2 Antigravity calls (1 wasted), no subagents; estimate about $30.
