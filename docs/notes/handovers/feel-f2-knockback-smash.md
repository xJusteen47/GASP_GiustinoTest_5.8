# F2 handover: knockback with the Smash formula (2026-11-17, cloud, Opus high, not compiled)
Branch `claude/project-thread-8xfrpx` from cocsel 3fd73243. Owner 2B, scope: standard rules + test/playtest kits. Not run: script and tests go to F4.
## Done
- Model: `UGSTN_KnockbackModel_PercentScaling` (already in code) gets `bBaseKnockbackInCmPerSecond`: K = (...)·KBG/100·UnitScale + BKB, BKB in cm/s
  (same K as Smash BKB×UnitScale). A move never retuned keeps its authored speed (flat) instead of ×22.5. P = percent AFTER the hit (before + damage), as Smash.
- UnitScale 22.5 (80 Smash units = tumble 1800); thresholds 540/750/1800 unchanged. Curve flattened (percent model + falling curve = damage counted twice).
- C++ tooling: `UGSTN_CombatRules::SwitchKnockbackModel / SetDefaultWeightCurveLinear / EvaluateDefaultWeightCurve`; `UGSTN_TestKitContentLibrary::
  WriteMoveHitKnockback / GetMoveTimelineAsset`; `UGSTN_LinkContentLibrary::GetKitThrows / DescribeThrowKnockback / WriteThrowKnockback`. Describe prints damage %.1f.
- Table: `python3 Tools/GAS/knockback_smash_data.py` -> `docs/notes/reviews/feel-f2/knockback_table.csv` (31 moves, K old/new at 0/40/100/150, launch/tumble %,
  weights 80/100/140) + `knockback_table_tilts_smash.csv`. Script output: "jab launch from 80/80/80 %, strong tumble from 40 %, tilts (smash) 19/11/4 %".
- Script `Tools/GAS/setup_knockback_smash.py [--model smash|weight] [--tilts hop0|smash] [--no-overlord]`: rules + every move bound on KIT_Test_Light/Medium/
  Heavy/Playtest + KIT_Example_Impacts, throws (native ones get BP children GA_Smash_<Kit>_Throw<Dir>). `--model weight` restores the old model, curve 1->0.6,
  growth 1.8 and old bases (from Saved/KnockbackSmash/before.json). Log Saved/KnockbackSmash/setup.log.
- Guards: setup_playtest_kit.py and setup_launch_impacts.py no longer write percent_growth on a non weight driven model.
- Tests (GSTN_CombatTests_Reaction.cpp): SmashFormulaKnownValues (3 hand cases of the published formula; no online calculator reachable from the cloud),
  SmashBaseInCmPerSecond (equivalence + F2 anchors), SmashMonotonicDeterministic, SwitchKnockbackModel.
## Owner decisions (2026-11-17 20:45Z: "vanno bene le consigliate")
1. Tilts hop0 (launch at 0 % at 30°, the 11-17 playtest) is the default; `--tilts smash` gives 19/11/4 %. 2. Heavy jab from 80 % like the other jabs.
3. Overlord moves converted (same K at 0 %): dropkick KBG 80, dair V9 70. `--no-overlord` leaves them flat.
4. RULE, scope Overlord jab 1 / jab 2 only: jab 1 launches little so it combos into jab 2 (K 400 at 0 %, KBG 20: stagger in place until ~110 %,
   the 31 f stagger lock holds it); jab 2 stays a JAB modelled on Ganondorf's Ultimate jab (BKB 41 -> 923 cm/s, KBG 74, ssbwiki; its own 7 %):
   launches at 0 % (~1300), tumbles from ~49 %. Other KBG per role: jab 40/30/20, tilt 90/70/70, special 60/80/100, strong 100, throws F60 B80 (heavy 100) U70 D40.
## What changes elsewhere (flagged, not retuned)
- High % is far less extreme: old model ×5-6 at 100 % (growth 1.8 × curve 0.6), new ×2-4. Hitstun = 0.02·K follows (e.g. light throw F at 100 %: 121 -> 39 f).
- Hard bounce / tech / balloon read K or speed: fewer hard bounces at high %. Weight spread 80 vs 140 at 100 %: ×1.75 -> ×1.2-1.3 (formula 200/(w+100)).
- MaxHealth of the rules asset assumed 100 for the old column (not readable from cloud). Rerunning the kit scripts resets growth: rerun this script after.
## F4
Build, run GSTNCombat.Core.Reaction.*, run the script headless, check setup.log (no WARNING), PIE: jab no launch at 60 %, strong tumbles at ~40 %.
