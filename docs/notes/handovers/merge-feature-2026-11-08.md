# Merge to feature, 2026-11-08 (laptop)

Branch `claude/merge-feature-2026-11-08` = `claude/m8-minion-trapped-laptop` at 1cbb331, a **fast-forward** of origin/feature/gas-npp-5.8
(b4149a8): 14 commits, no merge commit, no conflict.

## What goes in
- **Forward prediction** (plan `docs/architecture/plans/fwd-prediction-2026-11-07.md`): plan + owner decisions D1-D5; G0 / G0b / G0c
  measurements; two inert fork test knobs (`np.FixedTickInterpolationBufferedMSOverride`, `np.FixedTickDesiredBufferedInputCountOverride`,
  -1 = off, PATCHES.md §4); **interpolation buffer 50 ms** in `Config/DefaultNetworkPrediction.ini` (owner decision D1, was 100);
  G1 predicted partner presentation in `GSTNLink` (partner window shared with a later G4); owner doc `docs/notes/networking-spiegato.md`.
- **M8 minion fix**: minions trapped under tilted geometry (floor probe snapped the capsule into an overhang), bench penetration / airborne
  traces (`gstn.Minion.Bench.VerbosePenetration`). The launched-minion stall is still open (m8-minion.md).
- **R3-2 canary**: `Tools/Canary/make_canary.py` (code-project canary, `--tests`); 23/23 GSTN plugins build alone.
- **E17**: combat trace channel and sink (`gstn.Combat.Debug.Trace`), one sink call per simulated frame in `GSTNCombatNP`, no-op in Shipping.
- SLIDE brief marked cancelled (owner: movement becomes a separate mode).

## Verification
- Gate on e3b4687 (same engine code as the tip; 1cbb331 only adds the canary Python option and a doc line): all on 1409/0, GSTNMinion off
  1317/0, GSTNPlatform+GSTNPlatformParts off 1387/0, 0 ensures. SUITE_DEFAULT on DefaultLevel, listen + 2, 60/15, 0 %: 49/50 with `di`
  flaky (PASS on its single rerun).
- Per-step runs: SUITE_MOUNT 0 % 12/12 (2 % reds are known mount flakiness, rechecked), SUITE_MINION 0/2 % green except the known lod "no pop".
- Not run on this tip: SUITE_PARTS/DEFENCE/LINK on L_Test_Combat (desktop sweep).

## Owner action
`git push origin <tip sha of claude/merge-feature-2026-11-08>:refs/heads/feature/gas-npp-5.8` (fast-forward). Not pushed to feature by Claude.

## Cost
This merge step ~$0.5 (no build: fast-forward, gate reused). Work it carries: FP ~$21, trapped minions ~$5, canary ~$3, E17 ~$4.
