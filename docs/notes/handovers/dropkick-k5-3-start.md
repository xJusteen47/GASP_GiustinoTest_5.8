# dropkick-k5-3 start notes (from the round-2 thread, 2026-11-21): technical tips for the LAST pose round. Read with review k5r2b/review.md (di23qw 7213d9c2)
The round-2 thread closed at its context limit (rule 2); nothing of round 3 is done. Checkout GASP_Giustino2_n216qs, branch from uvzm3k 7f70af2b.

Tools already there (Tools/AnimPipeline, run from that folder):
- `cards/overlord/dropkick/k5/fold.py`: arm fit with the build check's clearance shapes (physbodies.json). Matches UE clearance within 0.3 cm.
  `fold.py fit <card> <keys...>` (env FOLD_ITERS, FOLD_STARTS, FOLD_X0 = a saved solution to warm-start, FOLD_ELB_IN) writes k5/fold_fit.json
  and draws %TEMP%/k5r2/fit.png (side/front/top silhouettes); `fold.py eval <card> <png> [keys]` prints the per-key clearance.
  The cost terms (TGT elbow heights, fist_fwd, on_chest, tuck...) encode the ROUND-2 target. For round 3, change the cost for the lying keys
  (14, 15, loop 0/6/11): elbows out at the sides (|side| ~18-22), forward of spine_05 by at most ~18-20, arm block within 10-12 cm of the
  sternum, forearms ON the chest. Keep keys 3-13 as they are (they passed), or bring them 5-8 cm closer.
- Applying a fit: copy the result into k5/fold_r2.json (startup/loop sections), then `python k5/make_k5_cards.py` regenerates both cards.
- The search runs in the ANATOMICAL chest frame (side, forward, up from the shoulders and the neck). The Poser idle chest is turned ~24 deg
  yaw against the component frame (a stance idle), so a card arm spec != the anatomical frame: fold.py converts with M.
- The upper arm is only 26.7 cm: an elbow at a given height has a forced forward distance from the shoulder. Clavicle protraction
  (pose "clavicle_l/r" = rotation vector in COMPONENT space about the chest up axis) moves it; for round 3 use little or none.
- Smug head: a card key "head" [pitch-ish, 0, roll] is distributed over neck_01/neck_02/head; the review wants >= 15 deg tilt or turn,
  chin up, eyes down the body at the target. The loop is 12 f and cycles: an overshoot inside the loop pulses every cycle.

Traps:
- animpipe build RETIMES the startup (spacing charge_long): clip f1-f2 keep the f0 pose and the fold lands at f4 whatever the keys say.
  Per-frame proof: Saved/AnimPipeline/animpipe/overlord_dropkick_startup_k5_check/clearance.json. Review accepts "sheet says f4".
- Build: `Run-Timed.ps1 -Preset Capture -Script Tools\AnimPipeline\editor_session.py` (background), wait for a FRESH
  Saved/AnimPipeSession/ready.txt (the old file stays), `python -m animpipe author|build|check <card>`, then refresh the game copies with
  ue.call('run_python_file', {filePath: Tools/GAS/setup_dropkick_kick_variant.py, entryPoint: 'run', kwargs: {refresh: True}}), write quit.txt.
  The quit may crash on an AnimSequence.cpp:5291 cache assert AFTER the saves: harmless.
- Captures: `k5/capture_r2.ps1 -Only k5r2w,k5r2hit,k5r2high` (whiff and hit MUST be separate sessions: the hit dummy blocks the side camera).
  The hit and high runs have no assert spec: run capture_assert.py offline (windows in dropkick-k5-2.md) and COPY the assert lines into the pack.
- Pack: `k5/review_pack_r2.py <base> <k5w> <k5hit> <k5high> <out>`. Review asks: same mp4 start offset for every mp4 (a-20) or the a-number
  burned in, and a head-and-arms close-up of two loop frames. Not done yet.
- Commits in n216qs need `git -c user.name=... -c user.email=...` (no identity configured there).
