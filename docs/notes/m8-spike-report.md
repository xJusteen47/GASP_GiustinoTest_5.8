# M8 spike report

Plan: `docs/architecture/plans/m8-minion.md` (branch `claude/project-thread-b6loil`), §1 rows X1..X14, §3 G0.

## G0a (laptop, 2026-11-06), starting HEAD 3312333

Starting point: local `feature/gas-npp-5.8` + the SD commits up to SD-16 (6 commits ahead of origin, M7 G7-G12/M7-15 **not** merged: step 0 of
the sequence was not done yet). The rows below do not touch M7 files.

### S0 seam audit: no row worse than written

| Id | Result on 3312333 |
|---|---|
| X1 | Present. `FGSTN_TimelinePlayer` (GSTNCombatCore, no NP include) evaluates Hitbox, HurtboxModifier, Movement, Tags, Event (+ CancelWindow, WeaponSwitch) |
| X2 | Present. `FGSTN_HitVictimView` has `HealthFraction`, `Weight`; the kind is not a field: `EntityId` is an int32 documented as the NP proxy id, so the minion kind travels in the id bit planned by X5 (G1). Not worse than written |
| X3 | Present (`IGSTN_HittableProvider`, `UGSTN_CombatWorldSubsystem::RegisterProvider`) |
| X4 | Present (`FGSTN_EntityHistoryRecord` ring, `RequestHistory`/`RecordHistory`, `GSTNLagComp::BuildOverlay`) |
| X5 | Partial as written: `GSTNEntityKinds::Minion = 1` reserved, no dispatch |
| X6 | Missing as written: `MaxHitTargets = 8` (`GSTN_CombatHitState.h:83`) |
| X7 | Missing as written: no hook in the fork; see spike 2 |
| X8 | Partial as written: `ClampRewindTime` only |
| X9 | As written: `FGSTN_PoseHistory` in `GSTNPlatformCore`; `Plugins/GSTNCore` does not exist yet (Q7) |
| X10, X12, X13, X14 | Present |
| X11 | Missing as written (`Combat.Entity.*` not declared) |

### Spike 2: post-fixed-tick hook and time→frame map

- **Hook point** (`NP/Private/NetworkPredictionWorldManager.cpp`, `BeginNewSimulationFrame_Internal`): inside the fixed-tick `while`, after
  `Services.FixedTick` ticked every model (line ~490), smoothing, and the lag-comp capture loop (`CaptureStateAndAddToHistory`, line ~506).
  The rollback loop (`ReconcileSimulationsPostNetworkUpdate_Internal`, after `StepRollback`, line ~339) is the second point. No existing
  delegate fires there: `FixedFinalize` runs once per engine frame, not per sim frame. A no-patch workaround exists for the authority only
  (a `UNetworkPredictionLagCompensation` subclass overriding `CaptureState`, forward loop only); not recommended, Q8 already chose the hook.
- **Patch text for G1** (small and inert when unbound, so no stop condition):
  ```cpp
  // NetworkPredictionWorldManager.h, public
  /** Once per fixed sim frame, after every model ticked it and the lag-comp history captured it; also for each re-simulated frame.
   *  ServerFrame = the Step.Frame the models saw; EndTimeMS = ServerFrame * FixedStepMS = the history stamp RewindActors reads.
   *  Services are locked: do not register instances from a listener. */
  DECLARE_MULTICAST_DELEGATE_ThreeParams(FNpOnPostFixedTick, int32 /*ServerFrame*/, int32 /*EndTimeMS*/, bool /*bResimulating*/);
  FNpOnPostFixedTick OnPostFixedTick;
  // .cpp, forward loop after the lag-comp capture:   OnPostFixedTick.Broadcast(Step.Frame, ServiceStep.EndTotalSimulationTime, false);
  // .cpp, rollback loop after StepRollback:          OnPostFixedTick.Broadcast(Step.Frame, ServiceStep.EndTotalSimulationTime, true);
  ```
  Plus a `PATCHES.md` row. The flag is information for listeners, like the Mover re-simulation flag; simulation code still must not branch on it.
- **Time→frame map**: `Step.Frame = PendingFrame + 1 + Offset`, history stamp = `(PendingFrame + Offset + 1) * FixedStepMS`, so frame F ends at
  `F * FixedStepMS` exactly. Authority `GetCurrentLagCompensationTimeMS` = `TotalSimTime - FixedStepMS`: equal to the newest stamp while the
  models tick, **one frame behind** it after the tick (where the hook runs). A hook listener must use `EndTimeMS`, not that getter.
  `FGSTN_LagCompTime` (X8, G1): `TimeMS = Frame * FixedStepMS`, `Frame = RoundToInt(TimeMS / FixedStepMS)`.
- Side finding: `GetSyncStateAtTime` indexes the local frame buffer with `TimeMS / FixedStepMS` (a server frame), ignoring `Offset`: correct on
  the server (Offset 0), off by `Offset` frames on a client. Not used by M8; note for FP.
- **Measured** (scratch automation test `GSTNSpike.FixedTickHook.TimeFrameMap`, drives `FFixedTickState` as the world manager does; step 33/16/17 ms,
  Offset 0/7/-3/120, 3600 frames; not committed): hook frame vs stamp **0** frames; "now" during tick vs newest stamp **0**; after tick **1**
  (always); round trip of arbitrary client times **0.471** frame max. Error ≤ 1 frame: pass. Log `Saved/SD17/headless-spike.log`.

## G0b spike 1, AI fighters baseline (laptop, 2026-11-07), HEAD 977eb20

Run on the **laptop** (the low-end profile of the brief), not the desktop: editor PIE in one process, `L_Test_Hub`, listen host that renders
+ 2 clients, 60 ms / 15 ms jitter. Scratch command `gstn.Spike.Brawl` (untracked `Source/.../ScratchSpike/GSTN_SpikeBrawl.cpp`, delete by hand):
spawns `BP_GASNPP_TestCharacter` bots with `UGSTN_AIInputSource` on the host, settle 5 s, measure 15 s. Per-world time = `OnWorldTickStart`→
`OnWorldTickEnd` (dispatch, NP fixed ticks, actors, anim, net flush). The hub already has ~10 dummies + 3 players in every row.
CSV `Saved/Logs/m8-g0/brawl-laptop.csv` (run 1 with a driver bug: `brawl-laptop-run1.csv`, same trend).

| N bots | loss | frame ms | host world ms | each client world ms | host out KB/s per client |
|---|---|---|---|---|---|
| 0 | 0 % | 83 | 35 | 14 | 19.5 |
| 10 | 0 % | 160 | 85 | 23 | 10.2 |
| 20 | 0 % | 317 | 203 | 34-36 | 5.1 |
| 40 | 0 % | 514 | 382 | 37-39 | 3.2 |
| 40 | 2 % | 510 | 379 | 37-38 | 3.2 |

- **Host: ~5-9 ms per bot per rendered frame** (slope 0→10 = 5 ms, 10→20 = 12 ms, 20→40 = 9 ms). The frame passes 16 ms long before N=10, so
  NP runs several fixed steps per frame (catch-up spiral) and the per-frame slope overstates the per-step cost: per fixed step it is still
  **≈ 0.5 ms per bot** (50 ms / 10 bots / ~10 steps per frame at N=10), more than 10x the 0.04 ms target of M8 §2 row 1. Clients pay
  ~1 ms per bot (sim proxies: anim + interpolation). KB/s per client falls with N only because the frame rate collapses (fewer flushes).
- Limits of this run: bots did **not chase** (`AddMovementInput` is not read by the BP Mover input on an AI pawn; `inputReady` false) and no
  attack landed in the window (60-90 frame interval vs ~30 frames measured): the numbers are idle AI fighters, so a lower bound of Brawl.
  One process (client worlds tick on the same thread); editor, not cooked; laptop GPU/CPU. Loss 2 % changes nothing at this frame rate.
- **Verdict**: AI fighters are far from fitting N = 40 (no stop condition, and the owner removed it anyway): M8 proceeds with the minion store.
  The desktop repeat of the brief (Insights scopes, cooked, 20/40/60) is optional now; it would only refine the slope.
- Proposed budget for the low-end profile (laptop ≈ Switch-class target): host frame 33 ms (30 fps), sim share ≤ 8 ms per fixed step for
  4 players + 40 minions ⇒ **≤ 0.1 ms per awake minion per step** and ≤ 0.05 ms per far (LOD) minion; clients ≤ 0.05 ms per minion
  puppet; net ≤ 4 KB/s per client at 40 awake (M8 §11 column unchanged). To confirm in G3 (bench) and G11.
