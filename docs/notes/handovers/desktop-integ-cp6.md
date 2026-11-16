# Desktop handover: integ-cp3 PIE harness + rule-10 captures (2026-11-15, _ue59, Sonnet, ~$5 est.)
Branch `claude/integ-cp3`. _ue59 built (not rebuilt, C++ untouched), Unreal closed (tasklist 0). AM_OverlordDAirV9 left dirty (variant restored after the base capture). No maps saved, PM_* clean.
**1 PIE harness** (L_Test_Combat, listen host+2, lag 60/15/2%; driver in scratchpad, report `docs/notes/reviews/cp5-pie/`)
- abilities: CMP 6/6 OK. forced_rollback: CMP 6/6 OK (`grep "CMP RESULT" docs/notes/reviews/cp5-pie/abilities_rollback_report.txt`).
- combat_spike: PASS 11 checks (`combat_spike.txt`; from Saved/Logs/ci/pie_batch.log.ue.log). The batch's own report file collided with the Run-Timed log (PermissionError, harmless).
**2 Captures** (L_Test_Hub, 7 views, 60 fps; per sequence mp4 + grids side/behind/front34/game; rows in `docs/notes/reviews/cp5-data/<cap>/`)
- `cp5-double-shot/{A,B}_close_shot_double.*` (camera dist 260, look-at 90 ahead; PIE_CAPTURE_DIST/AHEAD added to pie_capture_move.py).
  **FINDING (label: gioco)**: NO projectile exists in any world during A or B: rows `proj` (all worlds, AGSTN_Projectile count per frame) are empty in all 110 frames
  (`python -c` over cp5-data/dsA/shot_double.jsonl: 0 rows with proj). Both timelines run (timelineStarted 1 then 2) but the Fire Projectile action spawns nothing.
  So 1 vs 2 shots and the faster second one cannot be seen: cause not found (2 probe attempts, not a fix). Next: debug FireMoveProjectile (CanRunMoveActions / CanFire / host in listen).
  The earlier "single/double" claims in integ-cp3.md rested on frame rows only.
- `cp5-jab-chain/jab_jab1_jab2_faf.*` (tap 0, 12, 38 = acting at FAF). No base: no jab variant exists on this branch.
- `cp5-dropkick/dropkick_dropkick_whiff.*` (no variant).
- `cp5-dair-v9/{base,variant}/*_dair_{whiff,land,hit}.*`: base = HEAD AM_OverlordDAirV9, variant = the dirty working copy. Not compared by me (rule 10: needs a viewer).
- `cp5-hit-cue/cue_cue_hit.*` (KIT_Example Attack on a dummy 95 cm ahead): hit lands, hitstop H at v026 and the victim reacts; NO spark visible (Niagara slots empty by CP4 design). The victim's 3 cm shake is not judged by me: needs frame-by-frame look.
Nothing here is a PASS/FAIL review; I only checked that the chain works (one grid looked at).
**Tools**: `Tools/GASNPPTest/make_review_pack.py` (mp4 + grids), `pie_capture_move.py` (+DIST/AHEAD, +proj in rows, +cue_hit sequence), `Tools/CI/Run-Timed.ps1` committed.
**Cost**: Sonnet, ~1.3 h machine, ~$5 est. (6 editor sessions ~90-160 s each, 1 test batch 6 min).
