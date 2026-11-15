# ANIM ROUND 2 handover: jab v3 r2, dropkick r1 built, down-air waits for the code fix (desktop, UE 5.9, 2026-11-15)
Branch `claude/project-thread-anim-r2` (from anim4x `f5377ba2`), folder `GASP_Giustino2_ue59`. No C++ changed: still BUILT; editor closed, no
map saved. **Captures are PRE-MERGE: the laptop branch `claude/project-thread-n216qs` was not on origin when I stopped (context limit).**
- **To finish (next desktop thread, about 30 min)**: merge n216qs, build, then repeat the three captures and rebuild the review folders:
  `Tools/GASNPPTest/animr2/runbook.md` has every command (helpers are copies of `Saved/AnimR2/*.py`; run them from `Saved/AnimR2/`).
  Jab: `run_capture.py KIT_Test_Overlord Saved/AnimR2/jab/variant jab1,jab1_jab2,jab1_jab2_hit,jab1_jab2_faf` then `make_grids_r2.py`;
  dropkick: `... KIT_Test_OverlordDropkick ... dropkick_whiff`; down-air: see `docs/notes/reviews/overlord-dair-v9-codefix/README.md`.
  Bases (old code, round 1 clips) are in `Saved/AnimR2/{jab,dair}/base/` on this desktop: do not recapture them.
- **Jab v3 r2**: `docs/notes/reviews/overlord-jab-v3-r2/` (12 mp4, grids g0-g4, `numbers.md`). Cards edited in place, backups `.r1` / `.r2a`,
  notes `cards/overlord/jab/overlord_jab_v3_round2_notes.md`. What I SAW on the frames (g0 zoom, g1-g3, pre-merge):
  jab 1 first frame v21 already off idle (f0 is now a moved pose); contact v28 hand on the dummy from behind and from the side (wrist
  -18, 78, 134; diagonal 29 deg at contact opening to 36 in the hold; body shifted left, left hand on the hip); jab 2 v33 differs from the
  jab 1 hold (hand 39 cm, shoulders 38 deg), arm visible right of the torso at contact v42, hand on the dummy's chest from the side;
  acting at FAF (new sequence `jab1_jab2_faf`, new jab 1 on v59) shows no snap: pelvis 0.5 cm and feet 0.05 cm from idle at f26.
  Build worst vs source 2.3 / 2.0 cm (was 10.3 / 23.9). `contact_faf` PASS on both; `legibility` and `motion_check` still FAIL.
- **Not judged by me**: `g4_end.jpg` (the squat, expected still there pre-merge), the videos in motion, the hold and recovery of jab 1.
- **Owner's question (Smash / AAA parity)**: my answer is NO, not yet. Key poses and contact now read; missing: overlap and follow-through
  (`motion_check`: stiff chain, no overlap on both jabs), the recovery of jab 2 reaches idle at f23 and stands still for 3 frames, and the
  game-side glitches below. Dropkick: strike pose reads well from the side; the landing cannot be judged until the game holds the loop.
- **Game side, for the code thread**: (1) montage position on the first frames is unstable between runs: jab 1 showed 2.10 on v22 AND v23
  (a dead frame), jab 2 advances 0.48 on v34-v35; first frame shows 0.00-0.4. (2) End squat: unverified until the merge. (3) Dropkick:
  loop must hold until Mover lands then jump to the land section; pop to standing after the land clip (no get-up); 53 cm pelvis drop at
  the loop-to-land cut (capsule). For the capture only, `dropkick_loop_reps.py` = 4 and `press_delay: 5` put the land on touchdown (+1 f).
- **Dropkick r1**: `docs/notes/reviews/overlord-dropkick-r1/` (2 mp4, grid, `numbers.md`; no base, new move). Assets `AM_OverlordDropkick`,
  `AS_OverlordDropkick_{Startup,Loop,Land}`, `KIT_Test_OverlordDropkick`, `TL/GA/WPN`; kit script `Tools/GAS/setup_overlord_dropkick_kit.py`.
  All three clips FAIL `check` (fists open, contact_faf not meant for air moves, hitstop_snap 54 cm on the release, land clearance 4 cm;
  loop legibility crashes with `KeyError: '12'`, a tool bug). Hit numbers provisional.
- **Choices applied, owner to confirm**: jab 1 diagonal reduced to 29 deg at contact so the hand lands on the dummy (coordinator agreed);
  left hand on the hip in both jabs; right-foot stomp 17 cm (was 12); jab 2 "look at the hand" beat removed, arm almost straight at
  contact. Open: jab 2 hand tremor (additive Manuale layer f27-37) now plays on a hanging hand: keep, move or drop? Hitboxes untouched.
- **Antigravity (agy-check)**: sections A-C sent with the owner's written ok; answers verbatim in `docs/notes/reviews/agy-check/esito.md`.
  Section D (tests on jab 1 copies in the editor) NOT done. Its figures and command names are unchecked.
- **Left dirty on purpose**: `AM_OverlordDAirV9.uasset` (as before), `Apri editor 5.9.bat` untracked. `PM_BoostJump` / `PM_ThrowRider` restored.
- **What worked**: Opus-high for poses with numeric targets taken from the review; Sonnet for build/capture from the runbook; base captured
  before any rebuild; one zoom grid (400 px cells, 6 frames) answers "does the hand land" better than a 9-column sheet.
- **Cost**: Fable thread about 150k context; subagents Opus high 307k + 121k tokens, Sonnet about 710k in six runs; estimate $40-50.
