# PUSHBOX-P5 handover: minion core + pushbox, owner PIE bug (desktop, 2026-11-16). DONE (code, tests, 3 captures)
Branch `claude/pushbox-p5` from `claude/pushbox-p4` 0f94b163, folder `GASP_Giustino2_ue59`. Opus, no subagents. Not pushed (owner approval in thread).
`AM_OverlordDAirV9` dirty and jab `director/` untracked: not ours, untouched. No asset, map or project setting saved. `_ue59/IN-USO.txt` set then freed.
## Owner PIE bug (fixed first): NOT the pushbox
- Repro in Play (`pie_capture_move.py` BUG_SEQUENCES, KIT_Example, `PIE_CAPTURE_BUTTON=Strong`): after the strong attack's 42-frame timeline ended,
  its montage `AM_M_relaxed_takedown_stand_F_A` (~4 s) kept playing as a "released tail" (owner rule 2026-11-15, MONTAGE-EXIT-1) and its root
  motion layered move overrode walking: stick held, 0.2 cm/frame for ~200 frames (`reviews/pushbox-p5/strong_walk_before_rows.txt` v72-v252).
  `gstn.Pushbox.Log 1` printed no correction in that window. MONTAGE-EXIT-1 had listed it: "walking during a released tail does not stop it".
- Fix `UGSTN_CombatSimComponent::HandleAbilitySimulationPreTick`: out of a timeline, stick out of Neutral or any reaction state stops the released
  tail (`StopReleasedMontages`), inside the ASC tick, deterministic. After: walking at 6.4 cm/frame from v105 (`strong_walk_after_rows.txt`).
- "Stuck inside the dummy" not reproduced: dummy 10 cm away, walking out at 6.4 cm/frame (`inside_escape_rows.txt`). Likely the same tail.
## Minions (core + pushbox, data)
- `UGSTN_MinionDefinition::BodyCollision` (`FGSTN_MinionBodyCollision`: P4 `FGSTN_BodyCollisionProfile`, default PushboxAndCore; `StatesWithoutBody`
  default Launched, Knockdown). `ResolvePushbox`: pushbox = capsule radius x rules' CapsuleRadiusScale (34 -> 27.2), core x CoreRadiusScale (17).
- Snapshot entries carry it (authority `FillSnapshotEntry`, client `AppendSnapshotEntities`): fighters' push/core read minions with no new code.
  Fighter side skips bodies not overlapping at frame start (exact, same result; keeps lists short in a crowd).
- Lightweight movement: start-of-frame bodies (fighters from the snapshot, minions from the store, sorted by id; `TestFighterBodies` seam),
  sorted cell hash (n log n, 3x3 lookup, no pair loop). Fighter soft push + `ConstrainMove` before the sweep, and again on the move actually made
  after it (the wall slide let a minion slip past a core: first corridor run failed), correction swept: walls win. Minion-minion softness stays
  the existing separation steering. Knob `FGSTN_MinionMovementSettings::bBodyCollision`.
## Proof
- Builds `pushbox_p5_build2..4.log.esito` OK. Final tests `pushbox_p5_tests3.log.ue.log`: 895 `Result={Success}`, 0 `Result={Fail` (GSTNCombat+GSTNLink+
  GSTNMinion+GSTNDefence). New `GSTNMinion.Runtime.Body.*` (infos in `pushbox_p5_tests2.log.ue.log`): fighter into minion gap 32.00 (core sum 32),
  minion into fighter 37.00, minions head-on (no separation) 34.00 (sum 34), corridor 8 minions vs fighter: to fighter 31.91, pairs 36.00, |Y| 26.00
  (wall 26), settles 0.122, second run bit-identical; 160 minions + 4 fighters 0.366 ms/frame vs 0.304 without (`pushbox_p5_tests1.log.ue.log`).
- Capture cap3 (`minion_walk_rows.txt`, `python Tools/GASNPPTest/minion_body_rows.py Saved/pushbox_p5/cap3 minion_walk out.txt`): fighter walks into
  a minion, stops at 43.7 cm, shoves 0.9 cm/frame. Grid looked at: bench puppets are NOT visible (no mesh in this setup) and the bench Chase
  minions did not move, so no visual check of minions and no in-game minion-vs-minion. Numbers only.
## Open (for the owner via coordinator)
- Released tails still override the visual while walking stops them: OK? (owner rule says a new action stops the tail; walking now counts).
- Minion visual capture needs a puppet mesh (`gstn.Minion.Bench.PuppetMesh`) and a moving bench mode; not done (budget).
**Cost**: estimated $10-12 (Opus ~150k context, 4 builds, 3 test runs, 3 captures). Over the $8 budget by the bug detour.
