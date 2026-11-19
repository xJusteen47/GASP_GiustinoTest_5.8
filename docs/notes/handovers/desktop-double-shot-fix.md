# Desktop handover: CP3 double-shot "no projectile" (2026-11-15, _ue59, Opus, ~$12 est.)
Branch `claude/integ-cp3`. _ue59 built (editor target), Unreal closed. AM_OverlordDAirV9 still dirty (not mine, left). No maps saved.
**Root cause: the capture harness, not the game.** `Tools/GASNPPTest/pie_capture_move.py` documented `PIE_CAPTURE_BUTTON` but never read it
(`BUTTON = 'Attack'` hard-coded), so the CP5 double-shot captures pressed Attack: the kit's base light attack ran (`GA_Example_Light_C_0`,
entry 14, 1 track, no key), never the double shot. Proof: Verbose log of the old-style run, `Saved/Logs/ci/cap_dsA.log.ue.log` before the fix
showed only `GA_Example_Light_C_0: move frame N ... 0 key(s) crossed` (overwritten by later runs; reproduce with Attack).
**Fix (attempt 1 of 2, worked)**
- `pie_capture_move.py`: reads `PIE_CAPTURE_BUTTON` (default Attack), logs the button at load, and logs per sequence `host shots N` (each rise
  of the host's AGSTN_Projectile count): a shooting capture that shows no shot now says so in capture.log. This is the regression check.
- C++ diagnostics only (no behaviour change, rollback-safe): Verbose `LogGSTNBaseCombat` per move frame (keys crossed) and per key (actions),
  VeryVerbose sim tick; Verbose `LogGSTNProjectile` reasons when Fire Move Projectile / the shot builder refuse. Enable with `log <cat> Verbose`.
- Placeholder mesh: already there (engine sphere, 15 cm radius from the shot's shape) and visible: black spheres in the side grids. Not changed.
**Results** (PIE L_Test_Hub, listen host + 3 clients; `grep "done:" docs/notes/reviews/cp5-data/ds{A,B}/capture.log`)
- A and B: shot_single host shots 1, shot_double 2, shot_slow 2. Log `LogGSTNProjectile: Projectile N fired` in
  `Saved/Logs/ci/cap_ds{A,B}.log.ue.log`: double press = v 1800 then 2700 (second x1.5, overtakes the first ~frame 47 in A); slow = 1800 twice.
- New packs `docs/notes/reviews/cp5-double-shot/{A,B}_shot_{single,double,slow}.*` (DIST 900, AHEAD 350 so the flight fits the frame);
  rows in `cp5-data/ds{A,B}/*.jsonl`. Old `*_close_*` packs (no shot, wrong move) removed. I looked at one grid (A double side): 2 shots, merge.
  Not a rule-10 PASS/FAIL review: that still needs the cloud viewer.
- Tests: GSTNCombat filter 577 Success, 0 Fail (`grep -c "Result={Success}" Saved/Logs/ci/test_ds.log*`), same count as before.
**Note**: earlier captures with a non-Attack button were wrong too; every capture taken with PIE_CAPTURE_BUTTON before today pressed Attack.
**Cost**: Opus, ~1 h, 1 headless dump, 2 builds (incremental), 4 PIE captures, 1 test batch: ~$12 est.
