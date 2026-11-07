# M8 implementation plan: minions, 40+ enemies (plugins `GSTNMinion`, seams for `GSTNPlatform`)

Plan only, no code. Checkpoints are named **G0..G12**. One checkpoint = one commit (a few for the combat seams of G1, each its own
commit). The spec is [`../combat-m8-minions.md`](../combat-m8-minions.md) (section numbers below are its local ones, "M8 §x"), with the
research [`../many-enemies-replication.md`](../many-enemies-replication.md) ("MER §x") folded into it on 2026-10-22. Where the shipped
code on `feature/gas-npp-5.8` differs from the spec (it was written before M4-M7 were built), the shipped names win and the spec is
corrected in `docs/notes/m8-doc-deltas.md`.

Status: **approved** (2026-11-03). The two architecture questions are decided (owner 16:19Z: "take the most flexible and decouplable
options, you choose"; coordinator decision): **Q7** `FGSTN_PoseHistory` and the shared net primitives move into a new small pure plugin
`GSTNCore` (`Net/`), which `GSTNMinion` and `GSTNPlatform` both depend on (no duplication); **Q8** the small generic `OnPostFixedTick`
hook is added to the NP fork, inert if unbound. Every other default in section 5 is data or a tunable number and stays as written
until the owner changes it.

Sources (short ids used below):

| Id | File | Used for |
|---|---|---|
| M8 | `docs/architecture/combat-m8-minions.md` | The spec of this milestone (owner direction of 2026-10-22 folded in: listen host, parts, platforms, reaction tiers, gravity) |
| MER | `docs/architecture/many-enemies-replication.md` | Replication research: packs + FastArray, push model, dormancy order, per-item rates, the "150 rule", platforms §4 |
| M4 | `docs/architecture/combat-m4-hits.md`, `plans/m4-hits-plan.md` | Snapshot, inbox, pipeline, reaction tiers, lag compensation, hit presentation |
| CP | `docs/architecture/combat-parts.md`, `plans/m4b-parts-plan.md` | P15/P16 (`GSTNPartsMinion`) wait on this plan |
| TS | `docs/architecture/traversal-surfaces.md`, `plans/traversal-surfaces-plan.md`, `Plugins/GSTNPlatform/README.md` | T3 shipped the platform minimum for fighters; T8 (`GSTNPlatformMinion`) waits on this plan |
| TM | `plans/test-maps.md` | TM3 `L_Test_Enemies` is built after M8 |
| DD | `docs/architecture/game-design-direction.md` | Owner direction (wins over M8): §1-§2b parts, capped percent and weight, §2f many enemies and platforms, §5 crystal death, §6b listen server |
| OQ | `docs/architecture/open-questions-m4-m10.md`, section M8 | Owner answers (M8.3 big enemies, M8.4-6, M8.13, M8.14, M8.19) and the open items (section 5 here) |
| MT | `docs/architecture/modularity-and-tags.md` | Layering §3, reuse alone §3.7, Blueprint §3.8, replaceable content §3.9, tags §4 |
| M7P | `plans/m7-piggyback-plan.md` (PR #21 branch) | Format model of this plan |

Owner direction folded in (all already in M8, listed so a checkpoint thread does not have to look for it):

- **Listen server is the reference** (DD §6b): every budget is the host's machine, which also renders; dedicated stays supported.
- **Same hit rules as fighters** (DD §2b): stagger force against the threshold of the part or type, weight picks stagger in place /
  push back / launch, capped percent health with weight from the remaining health (OQ M8.4-M8.6). No separate poise counter.
- **"150 is a population, about 60 awake"** (M8 §4d.1): targets are 40 awake typical, 80 peak.
- **Swarm minions are never platforms**; at most 8 platform minions through the bridge `GSTNPlatformMinion` (M8 §4d.5). M8 only adds inert seams.
- **Death is a crystal shatter** (DD §5), pooled, presentation only. **Parts** come through the bridge `GSTNPartsMinion` (M4b P15/P16).
- **Gravity can change**: the lightweight movement integrates the gravity vector at the minion's position (M8 revision, GSTNGravity later).
- Conventions: Epic coding standard; native tags owned by the plugin (designer leaves in the plugin ini); every plugin usable alone
  (no `/Game/`, no project tags or ini, no `ConstructorHelpers`); designer logic in Blueprint through pure `BlueprintNativeEvent`
  hooks; soft references to content; a human-readable README per plugin; NP fork changes listed in `Plugins/GASNPP/PATCHES.md`.

## 0. Ground rules for every checkpoint

- **Gate before each commit**: full UBT build (not Live Coding for header or reflection changes) in **two configurations**, from G10
  three: everything on; `GSTNMinion` off (must equal the baseline: the test counts of the feature HEAD the gate starts from, plus what G1
  adds to `GSTNCombat*`); from G10 also `GSTNPlatform` off with `GSTNMinion` on. `Automation RunTests GSTN` headless green, every
  count read from a named `-abslog` log (`Select-String`), never estimated.
- **Harness**: scenarios named in the checkpoint clean at 60 ms / 15 ms jitter, at **0 % and 2 % loss**, listen server (host + 2 clients)
  first, dedicated + 2 clients second; replication spikes add a **third headless client** (M8 §2). SUITE_DEFAULT, SUITE_PARTS,
  SUITE_DEFENCE, SUITE_LINK stay green with `GSTNMinion` on and off from the first checkpoint that touches NP code (G1). Clean =
  snapshots equal and `report_reconciles(mark).unexplained == 0`, expected corrections counted (testing.md).
- **Performance numbers** (G0, G4, G6, G7, G11) are measured on the **desktop as listen host that renders** (Development, cooked if
  possible, `stat unit`, Unreal Insights with the `GSTN_MinionBatch` scopes), never on a `-nullrhi` host; `-nullrhi` only for the
  dedicated comparison and the headless clients.
- **Batch code has no `IsServer()` branch inside a stage**: the subsystem exists only with authority (`ShouldCreateSubsystem`), and
  the host's puppets read the store (M8 §4d.1), not a different simulation.
- **No edit of shared test Blueprints** or of the owner's levels; M8 adds its own test content (`DA_Minion_Grunt`, `DA_Minion_Heavy`,
  `L_MinionYard`, `L_MinionCourse`) through an idempotent `Tools/GAS/setup_minion_assets.py` that backs up first. Threads never save
  maps the owner has open (the owner saves with Ctrl+S).
- **Grep gate** (extended in G2): no include or module reference to `GSTNMinion*` from `GSTNCombat*`, `GSTNInput*`, `GSTNLink*`,
  `GSTNDefence*`, `GSTNParts*`, `GSTNPlatform*`; no `/Game/` and no `ConstructorHelpers` in `Plugins/GSTNMinion/Source`.
- **Token rules** (project instructions): a thread starts from a brief, one checkpoint (or two small ones) per thread, 100k context and
  80 tool calls as the cap, a written 3-line diagnosis before each build, at most 2 fix attempts then stop and write what is needed,
  machine output to files (5 lines printed), handover of at most 30 lines with the estimated cost.

## 1. What the shipped code already gives M8 (audit of `feature/gas-npp-5.8` at 60da6fa)

M4 was built with the minion seams in mind, so several M8 §3/§9 items exist. Rows **X1..X14**; G0 S0 re-checks them on the HEAD it starts from.

| Id | Capability M8 needs | M8 § | State on feature | Used by |
|---|---|---|---|---|
| X1 | `FGSTN_TimelinePlayer` in `GSTNCombatCore` (`Timeline/GSTN_TimelinePlayer.h`) | 3 | **Present**; S0 checks it covers the minion tracks (Hitbox, HurtboxModifier, Movement, Tags, Event) without NP | G3 |
| X2 | `FGSTN_HitVictimView`, `FGSTN_HitResolutionContext`, pipeline steps through the view (`Hits/GSTN_HitResolution.h`, `GSTN_HitRunner.h`) | 3, 9 | **Present**; S0 checks the minion fields (kind, health fraction, weight) | G5 |
| X3 | `IGSTN_HittableProvider`, `UGSTN_CombatWorldSubsystem::RegisterProvider`, `AppendSnapshotEntities` (`GSTNCombatNP/World`) | 9.1 | **Present** (tested in `GSTN_CombatNPTests_HitSnapshot.cpp`) | G5 |
| X4 | Lag compensation overlay `GSTNLagComp::BuildOverlay` with a **history ring in the world subsystem for entities without an NP component** and a pose provider | 9.2 | **Present**: the spec's separate `FGSTN_MinionHistory` may become a provider of this ring (S0 decides, recorded in the deltas) | G5 |
| X5 | `FGSTN_EntityRef` with `GSTNEntityKinds` (`ToString` already prints `Minion:5#0`) | 4c | **Partial**: the kind table exists, the minion kind id and `FindEntity` dispatch on the id bit are M8's | G1 |
| X6 | `HitTargets` ring of 16 for crowd cleave | 9.2 item 5 | **Missing**: `MaxHitTargets = 8` (`GSTN_CombatHitState.h:83`) | G1 |
| X7 | Post-fixed-tick hook `OnPostFixedTick(Frame)` in the NP world manager | 2 spike 2, 5 | **Missing** (no hook in `PATCHES.md`); `GSTNPlatform` poses at the start of each simulation frame and would move to the hook | G0, G1 |
| X8 | `FGSTN_LagCompTime` (one server frame ↔ rewind time conversion) | 9.2 item 3 | **Partial**: `GSTNLagComp::ClampRewindTime` exists, no frame conversion type | G1 |
| X9 | `FGSTN_PoseHistory` with swept bounds and payload column | 3, 4b | **Present in the wrong place**: `GSTNPlatformCore/Public/GSTN_PoseHistory.h` (T3); `GSTNMinion` must never depend on `GSTNPlatform` (question 7: decided, `GSTNCore`) | G2 |
| X10 | `UGSTN_KnockbackModel_WeightDriven`, `HurtboxReaction` step, reaction tiers | 3, 4d.2 | **Present** (M4) | G5 |
| X11 | Combat events with entity tags (`Combat.Entity.Fighter`) | 4c | **Missing**: the tag is not declared | G1 |
| X12 | `UGSTN_AIInputSource` on fighters | 2 spike 1 | **Present**: spike 1 (AI fighters baseline) needs no new code | G0 |
| X13 | Plugin split of M9 §5b (`GSTNCombatNP`, `GSTNInputNP` own plugins; `GSTNCombat.uplugin` without NP) | 4b | **Present** | G2 |
| X14 | `GSTNPlatform` T3 for fighters (colliders, pose point, surfaces) | 4d.5 | **Present**, no minion seams (by design) | G10 |

Consequence: one batch of **generic combat follow-ups** (G1: entity kind, `HitTargets` 16, the fork hook, lag-comp time, entity tag)
comes first, in the M6/M7 follow-up style (entry in the deltas file, no minion name in them, tests in the owning plugin). Everything else
lives in `Plugins/GSTNMinion`.

## 2. Plugin layout (M8 §4, §4b, adjusted)

| Module | Type | Contents | Depends on |
|---|---|---|---|
| `GSTNMinionCore` | Runtime | Tags header, `FGSTN_MinionStore` + handles, `FGSTN_MinionIntent`, LOD logic and tier scheduler, attack-token allocator, net item codec, interpolator, `UGSTN_MinionDefinition` + validation, `UGSTN_MinionLODSettings` | `GSTNCombatCore`, `GSTNCore` (shared net primitives, Q7), `GameplayTags` |
| `GSTNMinionRuntime` | Runtime | `UGSTN_MinionCombatSubsystem` (batch), `AGSTN_MinionActor`, `UGSTN_MinionCombatComponent`, `AGSTN_MinionPack`, presentation subsystem and puppets, spawner, waves, brain + StateTree schema, lightweight movement, bench and debug | `GSTNMinionCore`, `GSTNCombatGAS`, `GSTNCombatNP`, `NetCore`, `NavigationSystem`, `StateTreeModule`, `GameplayStateTreeModule` |
| `GSTNMinionMover` | Runtime, optional | Movement backend A (only if spike 3 picks it for a type) | `GSTNMinionRuntime`, `Mover` |
| `GSTNMinionEditor` | Editor | Definition and registry validators, wave details | `GSTNMinionRuntime`, `UnrealEd` |
| `GSTNMinionTests` | Developer | Automation tests needing a world, harness helpers | all of the above |

Native tags are defined in a Runtime module (EXT lesson of M6/M7: an `UncookedOnly` module ensures). `GSTNMinion.uplugin` requires
`GSTNCombat`, `GSTNCombatNP` and `GSTNCore` (Q7); nothing requires `GSTNMinion`. Tests: `GSTNMinion.Core.*` (no
world), `GSTNMinion.Runtime.*` (editor world, stub provider); harness `Tools/GASNPPTest/minion_scenarios.py`, suite **SUITE_MINION**.

## 3. Checkpoints (in order)

Cost per checkpoint is an **estimate** from the M6/M7 threads (about $5-8 for an S/M checkpoint on Opus medium, more when the harness
runs long): S ≈ $3-6, M ≈ $6-10, L ≈ $10-18. Model: Opus medium for G1, G3, G5, G6, G7 (difficult code); Sonnet for G2, G8-G10,
G12 and for every build/test subtask; Haiku for log reading and docs.

### G0 Spikes (scratch code only; results in `docs/notes/m8-spike-report.md` and combat.md §17)

- **S0 seam audit**: re-check X1..X14 on the starting HEAD; a row worse than written is a stop condition (section 4).
- **Spike 1, AI fighters baseline** (M8 §2): `Brawl` with N = 20/40/60 fighters driven by `UGSTN_AIInputSource`, listen host that
  renders + 2 clients. Records ms per entity and KB/s. **If N = 40 fits M8 §11 with no minion code, stop and ask** (M8 shrinks to AI
  fighters plus server LOD).
- **Spike 2, fixed-tick hook**: find a post-fixed-tick point in `UNetworkPredictionWorldManager` (after all models of frame F) and the
  time→frame mapping of `RewindActors`; if none exists, write the patch text (inert if unbound) for G1. Error ≤ 1 frame.
- Spikes 3-8 and R1-R5 need M8 code and run in their checkpoints (G4, G5, G6, G7, G8, G9, G11).
- Gate: no product code; report with numbers. Machine: desktop (host that renders). Size **M** (~$6-10).

### G1 Combat follow-ups (generic, `GSTNCombat*` and the NP fork; separate commits)

- **Entity kind**: declare the minion kind id in `GSTNEntityKinds`, `UGSTN_CombatWorldSubsystem::FindEntity` dispatches on the id bit
  (`MinionEntityBase = 0x40000000`, M8 §4c), generation check on delivery; inert with no provider.
- **`HitTargets` 16** (X6): ring raised from 8 to 16, oldest evicted when full (+48 B worst case in the sync state, delta-cheap).
- **`OnPostFixedTick(int32 Frame)`** (X7): the fork patch from spike 2 (or the found hook), unbound by default, entry in `PATCHES.md`;
  the owner is told before the patch lands (M8 §15).
- **`FGSTN_LagCompTime`** (X8) and the **`Combat.Entity.Fighter`** tag in `HitLanded/HitReceived/KO` events (X11).
- Tests: `HitTargets` eviction and 12-target cleave on fighters, kind dispatch with a stub provider, lag-comp time round trip.
- Gate: build (one configuration: `GSTNMinion` does not exist yet), headless counts = baseline + the new tests, SUITE_DEFAULT,
  SUITE_PARTS, SUITE_DEFENCE, SUITE_LINK at 0 % and 2 % unchanged (the sync-state size change must not add reconciles).
- Size **M** (~$6-10).

### G2 Plugin skeleton and Core (pure)

- `Plugins/GSTNMinion` with the modules of section 2 (Runtime/Editor/Tests empty shells), README, tags (`Combat.Entity.Minion`,
  `Combat.Event.MinionSpawned/Despawned/Promoted`, `Minion.State.*`, `Minion.LOD.*`, `Minion.Type`; ini leaves `Minion.Type.*`,
  `Command.Minion.*` in `Config/Tags/GSTNMinion.ini`), grep gate extended.
- Core: store (struct of arrays, free list, generation, capacity `gstn.Minion.Max` = 128, 160 since G12a), handles, intent, LOD tiers with hysteresis
  and dwell, phase scheduler, attack tokens, net item codec (M8 §8.2 layout) on the shared quantizer, definition + `IsDataValid`.
- Shared net primitives: new plugin `GSTNCore` (Q7 decided): `FGSTN_PoseHistory` and the quantizer moved out of
  `GSTNPlatformCore` with a redirect; `GSTNPlatform` tests must stay green).
- Tests (M8 §12 Core list): store alloc/free/generation, stale handles, LOD thresholds/hysteresis/dwell, schedule spread ±1, token cap,
  codec round trip (position ≤ 0.5 cm, yaw ≤ 0.8°), frame-stamp wrap at 8 and 10 bits, definition validation cases.
- Gate: build with `GSTNMinion` on and off (off = baseline counts), `GSTNMinion.Core.*` green, `GSTNPlatform` tests unchanged.
- Size **M** (~$6-10).

### G3 Authority batch, spawning, bench

- `UGSTN_MinionCombatSubsystem` (authority only), `AGSTN_MinionActor` (pooled, not replicated), `UGSTN_MinionCombatComponent`
  (Blueprint API `SetIntent`, `RequestCommand`, `GetHealth`, events `OnMinionHit/Died/LODChanged`), batch stages of M8 §5 driven by the
  G1 hook (LOD, inbox drain stub, timeline advance through `FGSTN_TimelinePlayer`, publish), spawner with the spawn queue
  (`gstn.Minion.SpawnsPerFrame` = 4), registry with a stable id table, `gstn.Minion.Bench.Spawn <N> <Scenario>`, `stat GSTNMinion`.
- Movement: a placeholder kinematic step (flat ground) until G4.
- Test content: `setup_minion_assets.py` creates `DA_Minion_Grunt` (walk, one swing, 40 HP) and `DA_Minion_Heavy`, plus `L_MinionYard`.
- Tests: batch order by slot index, timeline frame skipping fires events once, spawn queue budget, pool reuse.
- Gate: two-configuration build, headless counts, `GSTNMinion.Runtime.*` green; bench `Idle` and `Chase` at N = 40 run 60 s headless.
- Size **L** (~$10-18).

### G4 Movement backend + spike 3

- `IGSTN_MinionMovementBackend`, backend B `FGSTN_MinionMovement_Lightweight` (gravity vector at the position, one sweep, one slide,
  ground snap, minion separation, push back that stays grounded, launch along trampoline normals); `L_MinionCourse` (stairs, 40° slope,
  thin wall, corner, ledge).
- **Spike 3**: `cB` (and `cA` only if a Mover prototype is cheap to try) at N = 60 in `Chase` and `Massacre`, 10-minute soak with
  random 3000 cm/s launches. Decision rule of M8 §6 applied and recorded; `GSTNMinionMover` built only if A wins for some type.
- Tests: flat, step, slope, wall slide, no tunnelling at 3000 cm/s, ground snap, gravity direction change.
- Gate: build both configurations, headless counts, soak 0 failures (fell out of world, stuck > 2 s, tunnelled).
- Size **M** (~$6-10).

### G5 Hit flows, attack tokens, lag compensation (spike 5)

- Minion provider in the snapshot (T0/T1 minions, X3); player → minion through `PendingHits` (stamped F, generation-checked, drained at
  F+1 sorted by victim, attacker, hitbox), pipeline through `FGSTN_HitVictimView`, reaction tiers into `ActionState` (`Hitstun`,
  `PushedBack`, `Launched`; below the threshold only `HitCounter`), KO and last-attacker ring; `HitByRing` dedupe; minion → player
  queries against fighters only, into the fighter's inbox; attack tokens (`MaxSimultaneousAttackersPerPlayer` = 4).
- Lag compensation: minions feed the world subsystem's history ring or a pose provider (X4, S0's choice), swept-bound rejection, the
  host never rewinds. **Spike 5** recorded.
- Slots for the bridges (inert, no parts code): `HurtboxIndex` in `PendingHits`, the optional pipeline step slot, `OnOutcomeApplied`
  store hook (needed by M4b P16).
- Tests: victim view through each step for a minion, `HealthFraction` model, attacker hitstop independence, reaction tiers with and
  without a part set (stub), inbox ordering and generation drop, token release on death and hit.
- Harness (server-side state only, puppets come in G7): `scenario_minion_hits_player`, `scenario_minion_tokens`.
- Gate: build both configurations, headless counts, the two scenarios at 0 % and 2 %, SUITE_DEFAULT unchanged.
- Size **L** (~$10-18).

### G6 Replication: packs, dormancy, rates (spikes R1, R2)

- `AGSTN_MinionPack` (push model, up to 16 items, `FGSTN_MinionNetArray` with the G2 codec), `MutatePack()` as the only mutation entry
  ("wake or flush first, then mutate"), per-item tier rates (T0 15 Hz, T1 5 Hz, T2 1 Hz, pack frequency as a ceiling), per-pack
  relevancy with budgeted grants (`MaxRelevantPerClient` = 64 items, `gstn.Minion.HostUploadBudgetKBps` = 40), registry hash check at connect.
- **R1** bake-off (packs vs lean actor per enemy; the custom RPC only if packs miss by > 25 %), **R2** dormancy order. R3 (Iris) is
  **not run** unless the owner asks (question 9).
- Tests: tier scheduler never exceeds an item's rate or the pack ceiling, `MutatePack()` order, codec bounds with `bHasType`/`Teleport`.
- Harness: `scenario_minion_dormancy_flush` (10 min, mutate-then-flush as negative control), `scenario_minion_late_join` (state only).
- Gate: build both configurations, headless counts, R1/R2 exits of M8 §2 met, scenarios at 0 % and 2 % with 3 headless clients.
- Size **L** (~$10-18).

### G7 Client presentation, host puppets (spike 7 = R6)

- `UGSTN_MinionPresentationSubsystem`, pooled `AGSTN_MinionPuppet`, `FGSTN_MinionInterpolator` on the NP interpolation render time
  (T0), dead reckoning with 200 ms blends for T1/T2, host puppets read the store directly, predicted hit flash and mesh pause through
  `UGSTN_HitPresentationComponent` confirmed by `HitCounter`, puppets registered as client hittables, `MaxVisiblePuppets` = 60,
  animation budget stack (URO, `OnlyTickPoseWhenRendered`, Animation Budget Allocator), custom primitive data for flash. Death: a
  placeholder pooled shatter (no aesthetics work before the mechanics are done, owner 2026-10-24).
- **Spike 7** on the desktop host that renders and on a client.
- Harness: `scenario_minion_replicate` (40 over 3 packs, puppet within 20 cm at the displayed frame), `scenario_minion_late_join`
  (with puppets), `scenario_minion_player_hits_minion`, `scenario_minion_reaction_tiers`, `scenario_minion_lagcomp`.
- Gate: build both configurations, headless counts, the five scenarios at 0 % and 2 %, spike 7 exit (≤ 3 ms game thread at 60
  puppets, ≤ 2 ms at 40) or the fallback of M8 §4d.1 recorded. **Owner check**: a short in-game video (several angles) of 40 grunts
  being hit, for the owner's own OK, not for Claude's review.
- Size **L** (~$10-18).

### G8 AI hook (spike 6)

- `UGSTN_MinionBrainComponent` with `UStateTreeComponent` (no AIController), schema `UGSTN_MinionStateTreeSchema`, tasks
  `GSTN_MoveToTarget`, `GSTN_FaceTarget`, `GSTN_RequestAttack`, `GSTN_TargetInRange`, `GSTN_IsLODAtLeast`; steering utils (seek,
  arrive, separation; async path at ≤ 2 Hz); a test StateTree "chase and swing". **Spike 6**: ≤ 0.02 ms per ticking minion at T0, else
  the hand-written task list behind the same intent API.
- Harness: `scenario_minion_lod` (walk away: tier transitions, no pop, a hit wakes a dormant minion), `scenario_minion_tokens` with the brain.
- Gate: build both configurations, headless counts, scenarios at 0 % and 2 %. Size **M** (~$6-10).

### G9 Waves and promotion (spike 8)

- `UGSTN_MinionWaveDefinition`, `UGSTN_MinionSpawnerActor` (trigger-driven, `OnWaveCleared`), despawn after `CorpseSeconds`;
  `UGSTN_MinionSpawner::Promote` (minion → NP fighter, client cross-fade, cap `MaxFightersFromMinions` = 4). **Spike 8** recorded.
- Acceptance item 3 of M8 §13: a minion type and a wave made without C++ (definition, an existing timeline, the test StateTree).
- Harness: `scenario_minion_promote` (no pop > 15 cm, first control ≤ 500 ms).
- Gate: build both configurations, headless counts, scenario at 0 % and 2 %. Size **M** (~$6-10).

### G10 Bridge seams for platforms and parts (inert)

- Platform seams of M8 §4d.5: store flag `bPlatformActor` (stage 2.5 skips the pack item), `IGSTN_MinionPublishObserver` called at the
  publish point, per-slot minimum LOD tier and `MaxActiveT0` exemption, pose-history samples readable by the bridge. Optionally the
  `GSTNPlatform` pose point moves to the G1 hook (its own commit, only if T-suite stays green).
- Parts: the `ExtByte` slot (`bHasExt`, 0 bits without a part set) in the item codec, agreed with the M4b P16 text.
- No `GSTNPlatformMinion` and no `GSTNPartsMinion` here: they are T8 and P16, separate plans.
- Gate: **three configurations** (all on; `GSTNMinion` off; `GSTNPlatform` off), headless counts, every M8 scenario so far unchanged
  with the seams unbound. Size **S** (~$3-6).

### G11 Horde bench and budgets (R4, R5)

- `scenario_minion_horde` (60 minions, both clients fighting 60 s), `scenario_minion_bytes` (N = 40 / 80, 3 headless clients, per tier
  and host upload), the 150-population case (≤ ~60 awake). **R4** (orders against snapshots for T1/T2) and **R5** (budgeted relevancy)
  recorded; the tables of M8 §8.3 and §11 re-based on the measured numbers; **max N on the host recorded honestly**, with the fallbacks
  of §4d.1 applied in order if needed (question 10).
- Gate: build both configurations, horde and bytes scenarios at 0 % and 2 %, numbers inside M8 §11 or the recorded fallback.
- Machine: desktop as host that renders. Size **M** (~$6-10).

### G12 Editor, debug, sweep, docs, TM3 brief

- `GSTNMinionEditor` validators (definition, registry order, wave), debug cvars `gstn.Minion.Debug.{Capsules,Hitboxes,LOD,History,Intent}`.
- `scenario_minion_forced_reconcile` (forced reconciles on the player during minion hits); every M8 §12 scenario in listen and
  dedicated, 0 and 2 %; SUITE_DEFAULT/PARTS/DEFENCE/LINK/MINION in the three configurations; byte and CPU tables.
- Docs: README "How to extend", combat.md (§3 modules, §13, §15, §17, §18 roadmap), testing.md, `m8-doc-deltas.md`, M8 §13
  acceptance checklist; TM3 `L_Test_Enemies` brief checked against what shipped (test-maps.md) so the map can be built.
- Size **M** (~$6-10).

**Total estimate**: about **$100-160** over 13 checkpoints (roughly 13-16 threads), most of it in G3, G5, G6, G7 and the harness runs.

## 4. Stop and ask

- S0 shows an X row worse than written (a needed seam is missing or hard-wired to fighters).
- Spike 1 says AI fighters already fit N = 40 (then M8 changes shape: ask before building the store).
- Spike 2 finds no hook and the fork patch is larger than a small inert delegate.
- Spike 3: neither backend passes (ship B with a lower `MaxActiveT0` only after telling the owner the recorded N).
- R1: packs miss the budget by more than 25 % (the custom snapshot RPC is a new architecture).
- Spike 7 / G11: the host frame does not fit at N = 40 even after the fallbacks of M8 §4d.1.
- Any change that would make `GSTNCombat*`, `GSTNPlatform*` or `GSTNParts*` name minions, or change a fighter result in SUITE_DEFAULT.
- An edit to a shared Blueprint, the owner's level, the project camera, or a project-wide switch (Iris, push model on other actors).

## 5. Domande aperte per l'owner (max 10; tra parentesi l'ipotesi che il piano usa se non rispondi)

1. **Numeri** (M8.1): quanti nemici svegli in una scena tipica e quanti nel picco? (40 tipici, 80 di picco; 150 solo come popolazione
   con circa 60 svegli.)
2. **Host** (M8.20): quanti client si collegano di solito e che upload ha l'host? (3 client, almeno 5 Mbit/s.)
3. **Tipi al lancio** (M8.2, M8.3): bastano due tipi a terra, corpo a corpo (Grunt e Heavy)? Volanti e a distanza dopo M10? (Sì.)
4. **Collisione** (M8.18, M8.25): i minion bloccano fisicamente i giocatori o si attraversano e il contatto conta solo per i colpi?
   (Si attraversano; i minion si separano solo tra loro. Più economico e senza correzioni sul giocatore.)
5. **Attaccanti** (M8.8): quanti minion possono attaccare lo stesso giocatore insieme? (4.)
6. **Promozione** (M8.11): un minion diventa elite/boss solo quando lo chiede uno script/Blueprint? Mai il contrario? (Sì, solo da script; niente ritorno a minion.)
7. **Architettura, primitive di rete condivise**: `FGSTN_PoseHistory` oggi sta in `GSTNPlatformCore`, ma `GSTNMinion` non può dipendere da
   `GSTNPlatform`. La sposto con il quantizzatore in un piccolo plugin puro comune (`GSTNCore`, cartella `Net/`) da cui dipendono entrambi,
   o la duplico (~200 righe)? **Deciso 2026-11-03: plugin comune `GSTNCore`.**
8. **Architettura, patch del fork**: va bene un piccolo hook `OnPostFixedTick` nel fork di Network Prediction, spento se nessuno lo usa?
   **Deciso 2026-11-03: sì.**
9. **Iris** (M8.27): restiamo sulla replicazione classica (pacchetti FastArray, push model, dormancy) e saltiamo lo spike Iris? (Sì, risparmia uno spike costoso.)
10. **Host al limite** (M8.21, M8.26): se il frame dell'host non basta, va bene ridurre i minion in combattimento (48 → 32), poi la frequenza,
    poi le animazioni dei lontani (condivise o senza scheletro)? (Sì, in quest'ordine, con il massimo misurato scritto nel report.)

## 6. Edits needing the owner's OK

- The NP fork patch of G1 (question 8, approved 2026-11-03), still announced before it lands, listed in `PATCHES.md`.
- The move of `FGSTN_PoseHistory` out of `GSTNPlatformCore` into `GSTNCore` (question 7, approved 2026-11-03).
- Enabling `GSTNMinion` in the `.uproject` (G2) and `net.IsPushModelEnabled` in `Config/DefaultEngine.ini` (G6; it is safe for
  properties not marked, M8 §8.1, but it is a project ini).
- No shared Blueprint and no owner level is edited; test content is new and made by script.

## 7. Coordination, machines and what waits on M8

- **Waits on M8**: M4b P15 (needs G2/G5 view only) and P16 (needs G6 + G10, then the owner's go-ahead), traversal T8
  `GSTNPlatformMinion` (needs G10 and P16), M12 game modes (waves, spawners), TM3 `L_Test_Enemies` (after G12).
- **Shared files**: G1 edits `GSTNCombatCore`/`GSTNCombatNP` and the fork; it runs only when no spin-drill or M7 checkpoint has
  those files open, or rebases onto them. G2-G12 touch only `Plugins/GSTNMinion`, the harness and new test content (parallel-safe),
  except G2's `FGSTN_PoseHistory` move (touches `GSTNPlatform`) and G10's optional pose-point move.
- **Machines** (project rule: at most 2 code threads): G1-G5 and G8-G10 run on the **laptop** (headless builds and harness), G0, G6,
  G7 and G11 need the **desktop** as a host that renders. M8 starts when the spin-drill (laptop) or M7 (desktop) thread frees a
  machine; one editor or build at a time per machine; the owner's main checkout on the desktop is never touched (worktree).
- **Delegation**: each checkpoint is one fresh thread from a brief in `docs/notes/handovers/`, with a HANDOVER entry in
  `docs/notes/takeover-state.md`.

## 8. Risks

- **Host frame**: batch, puppets, debris and the host's own game share one frame; the host rows and the fallbacks are the mitigation,
  and the real max N is written down.
- **Many corrections on players** from minion hits: tokens bound them; a client-side hit inbox is a separate decision if
  `scenario_minion_horde` shows too many.
- **Own collision code** (backend B): tunnelling and corners; the hostile course, the 10-minute soak and backend A per type.
- **Dormancy and push model**: a missed wake or dirty mark means a client never sees a minion; `MutatePack()` and the dormancy
  scenario are the gate.
- **Combat refactors** (G1: `HitTargets` 16, entity kind): touch M4 sync state; separate commits with the suites as the regression gate.
- **Spec age**: M8 was written before M4-M7 shipped; S0 and the deltas file absorb renames (X4 may remove `FGSTN_MinionHistory` as a type).
- **Multi-segment bosses** (more than 16 parts, M8 §15): not designed; not in this plan.
