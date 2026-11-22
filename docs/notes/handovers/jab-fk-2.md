# Handover: Overlord jab round 2 (desktop, 2026-11-18). Jab 2 Fable PASS, jab 1 Fable FAIL (narrow, posa only)
Branch `claude/desktop-jab-fk` (pushed). Round 1 at 496eb8c9, round 2 at 26804243, plus this note and `docs/notes/reviews/jab-fk-2/review.md`.

## Causes found (checked by comparing data with the in-game capture: `var_jab3` gives 0.0 cm pelvis/feet error after the blend-in)
- Jab 2 slide was NOT a root motion double count. The capsule moves once (NetMontageSimulator queues Mover root motion), and both ABPs are
  RootMotionFromMontagesOnly, so the visual root is locked. Real cause: `ik_foot_l/r` were left at the reference pose, and the ABP puts the feet on them
  (known from the DAir kit, `Tools/GAS/setup_overlord_dair_kit.py` `_bake_ik_feet`). `fk/pub_jab_r2.py` now bakes them from the feet.
- Jab 1 hip pop: `casc_fk_take.py` added the template root offset (-8.6,-13.8) to the whole body, but the owner's poses are already in the mesh frame.
  Plan key `root_offset: [0,0,0]` fixes it; root track is now 0 in both clips (`ue_write_take.py --keep-root` for both).
- New tool options: `keys[].shift` takes z, `ground_z`, `foot_paths` spot as `{"pos","quat"}` (exact copy of another take's foot), `root_offset`.
- Capture: another editor (bugpass test runs) held MCP port 8000 (2 failed starts). `MCP_PORT=8017 Tools/GASNPPTest/animr2/cap_one.sh ...` works.

## Data (plans `Tools/AnimPipeline/cards/overlord/jab/fk/plan_jab{1,2}_r2.json`, publish `fk/pub_jab_r2.py` headless, backups `/Game/AnimPipeline/Backup/*_r2pre_*`)
- Jab 1: left foot pinned for the whole clip, right foot steps f2->f7 (lift 5), key 7 pelvis -3 cm z (reach), return at f24, arm overshoot/settle
  offsets f9-f17. AM_OverlordJab1V3 blend-in 0 -> 0.1 s.
- Jab 2: f0 = jab 1 a11 (0.0 cm), eased root_forward, right foot (striking side) steps f2->f8 (lift 8), left pinned until f22, root lock AnimFirstFrame.
- Capture `Saved/AnimR2/jabfk2/var_jab3`; mp4 + sheets + jsonl in `docs/notes/reviews/jab-fk-2/variant/`.

## Owner-confirm items, applied under rule 6 (recommended options)
- Jab 1 step width: kept the owner's O7 foot spot (step ~27 cm out). Wind-up hand height: kept the owner's O1 pose. a9-a16 hold: moving hold
  (overshoot/settle) instead of a dead hold.

## Next: jab 1 round 3 (last pose round before the owner decides), all `posa`, from review.md items 1-4
1. Right foot drags 23 cm inward a1-a5 (FK arc of the owner's O1 crossover): plant it a0-a3 with a heel pivot, lift a4, land on a6.
2. Overshoot too small (4 cm): hand 8-10 cm with wrist drag at a8-a9, settle by a12, elbow 5-8 deg (bump `offsets` in plan_jab1_r2).
3. Pin the left foot on the game idle spot (~6 cm off now): use the idle feet from `faf_frames.jsonl` f17-f20, and also set blend-in 0.05 s.
Jab 2 polish (not blocking the PASS): left foot step-up in recovery a17-a24 so the jab 1 restart does not drag it; hold contact a9-a10.
Gioco: pawn floats until the first root-motion move; 2.3 cm drift on the jab 1 restart.

Proof: `pub_r2_result.txt` lines "ik gap a9 [0.0, 0.0]"; error table from var_jab3 frames.jsonl versus `jab{1,2}_r2_bones.json` (script in this thread's
scratch, same math as `fk/casc_fk_take.py` output, mapping jsonl x = comp y, jsonl y = -comp x). Cost: about 165k context (Opus) + 1 Fable review subagent.
