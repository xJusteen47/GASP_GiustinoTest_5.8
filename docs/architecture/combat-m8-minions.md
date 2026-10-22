# Combat M8 — Minions, 40+ enemies (implementation specification)

Part of the combat design ([combat.md](combat.md)): read §2, §3, §5, §12, §13 and §15 there, the M4 specification
([combat-m4-hits.md](combat-m4-hits.md): snapshot, inbox, pipeline, lag compensation, presentation) and the outline in
[combat-m5-m9.md](combat-m5-m9.md) that this file expands. Section numbers below are local to this file. M8 assumes M1–M4
merged; it does not need M5–M7 (grab, shield and mount interactions with minions are listed in section 14 and left to
those milestones).

> **Status 2026-10-21:** specification only. Every number in sections 2 and 11 is a **target to confirm with the spike**;
> every *Proposal* resolves an *Open* item of the outline and can be overridden by the spike results or by the
> project owner (questions in [open-questions-m4-m10.md](open-questions-m4-m10.md), section M8).

> **Revision 2026-10-22 (owner direction, [game-design-direction.md](game-design-direction.md) §1–§3, §5, §6b; it wins
> over this file):** (1) reference topology is a **listen server**: every former "dedicated server" budget is now the
> **host's machine, which also renders** (§4d.1, §8.3, §11); dedicated stays supported. (2) Minions get **parts**, stagger
> and juggle factors and weight from remaining health through the bridge plugin `GSTNPartsMinion`
> ([combat-parts.md](combat-parts.md) §5.5; adds an optional `ExtByte` to `FGSTN_MinionNetItem`, §8.2). (3) Big types may
> become grabbable after parts break, and minions can be grabbed and thrown at other minions (§14, M10). (4) Death is a
> crystal shatter, pooled (§4d.4). All new choices are *Proposal*; see §4d.

## 1. Scope

**Goal:** dozens of simple enemies (40 required, 100 stretch) with the **same timelines, hitboxes, hurtboxes, teams,
pipeline and knockback** as fighters, at a small fraction of a fighter's server cost, with bounded bandwidth, on the
**authority host** (a listen-server host that also renders, owner direction §6b; a dedicated server stays supported). Player→minion and minion→player hits are first class (lag compensation, hit presentation, one
correction per hit on the player's client).

In: server-side batch simulation outside NP; timeline player shared with fighters; movement backends (decision by
measurement); LOD; compact replication and client puppets with interpolation; hit flows both ways; AI hook (StateTree);
spawning, pooling, waves; promotion to fighter; debug, tests, harness.

Out: AI design (behaviours are content), navigation setup, group tactics beyond attack tokens, loot and progression,
minion grabs/shields/mounts (M6/M7 follow-ups, section 14), forward prediction of minions (never, combat.md §12.7),
dedicated-only assumptions (*revised 2026-10-22*: the subsystem runs wherever there is authority,
`NM_DedicatedServer || NM_ListenServer || NM_Standalone`; host-specific rules in §4d.1).

**Decision (justified): minions do not run GAS abilities; they run a lighter timeline player over the same timeline
data.**

- Cost: a fighter is three NP models (combat 47, ASC 48, Mover 50) plus an NP proxy and sim-proxy replication. Spike S-FP
  measured ~0.03–0.04 ms per predicted entity per re-simulated frame; on the server an entity costs the same order every
  frame, i.e. 60 entities ≈ 6–7 ms of a 16 ms frame before any AI. Spike 1 (section 2) re-measures the baseline; the
  rule below stops M8 if the fighter path turns out affordable.
- Needs: minions need none of what the ASC provides (prediction keys, rollback of effects, owned tags for prediction,
  cooldown/cost for a player, loadouts). Their timers are frame stamps already (combat.md §5.5), their attributes are a
  handful of numbers, and nothing runs on a minion's owner client.
- Data parity: the designer authors a `UGSTN_CombatTimeline` once; a fighter plays it through `UGSTN_GA_BaseCombat`, a
  minion through `FGSTN_TimelinePlayer` (Core, section 3). Supported tracks on minions: `Hitbox`, `HurtboxModifier`,
  `Movement`, `Tags` (internal, not projected on an ASC), `Event` (cue only, played by clients). `CancelWindow`,
  `WeaponSwitch`, buffer, startup override and costs are ignored (validator warns when a minion move uses them).
- Escape hatch: promotion (section 10) turns a minion into a full fighter, and a few "mini-boss" types can be fighters
  from the start without touching M8.

## 2. Spikes first (record results in combat.md §17)

Run in this order; each has a numeric exit. Tooling: `UGSTN_MinionBench` (a debug console command set in the M8 module,
strips from shipping): `gstn.Minion.Bench.Spawn <N> <Scenario>` with scenarios `Idle`, `Chase` (walk to the nearest
player), `Brawl` (chase + attack every 60–90 frames), `Massacre` (all minions hit at once, then knocked into walls).

**Measurement method (every spike):** **listen-server host** (reference, owner direction §6b: the host process renders and
runs a local player, measured with `stat unit` split into game/render/GPU) and, for comparison, a dedicated server; packaged-like config (`-nullrhi -unattended -NoSound` only for the dedicated comparison;
the host run keeps rendering at the project's minimum-spec resolution; Development build, cooked if possible), 2 PIE or standalone clients (testing.md harness) with 60/15 ms and 2 % loss, 60 s
per cell, N ∈ {20, 40, 60, 100, 150}.
- CPU: `UnrealInsights` with `-trace=cpu,frame,bookmark,log,net -statnamedevents`; scopes `GSTN_MinionBatch` and children
  (`LOD`, `Inbox`, `Timeline`, `Query`, `Movement`, `Publish`, `Replicate`), plus `STAT_GSTN_Minion*` cycle counters and
  `stat unit`/`stat game` on the server. Report mean, p95, p99 and max per frame over the run.
- Network: Net Insights (`-NetTrace=1`) per actor/property for `AGSTN_MinionPack`; `stat net`; the `NetProfile` console
  command as a cross-check. Report bytes/s per client, split by pack tier.
- Client: `stat game` and `stat anim` (puppet count, animation budget), Insights frame view.
- Memory: `memreport -full` before/after spawn; target ≤ 400 B per minion in the store (excluding assets).

| # | Spike | Question | Exit (go / no-go) |
|---|---|---|---|
| 1 | **Baseline: AI fighters** | What does the *existing* fighter path cost for N AI pawns (combat + ASC + Mover + NP replication, AI input source) in `Brawl`? | Records `cF` ms/frame per entity and KB/s. **If N=40 fits the targets of section 11 with no minion code, stop M8 and use fighters** (unlikely; needs ≤ 0.04 ms per entity and ≤ 4 KB/s). |
| 2 | **Fixed-tick hook** | Is there a post-fixed-tick callback in `UNetworkPredictionWorldManager` (after all models of frame F, server) and how to read frame F and the time→frame mapping used by `RewindActors`? | Hook found, or a small fork patch `OnPostFixedTick(int32 Frame)` written (inert if unbound). Time→frame error ≤ 1 frame. |
| 3 | **Movement** | Cost and robustness of option A (Mover, non-predicted) vs B (lightweight, section 6) per minion-frame in `Chase` and `Massacre`. | Decision rule of section 6. |
| 4 | **Replication** | `AGSTN_MinionPack` FastArray bytes/item, per-pack relevancy and `NetUpdateFrequency` tiers, dormancy wake latency; is Iris enabled in 5.8 for this project? | Bytes/minion/s inside budget (section 8); wake latency ≤ 2 frames of the pack rate. If Iris is on, compare with a `UNetObjectFilter` on one actor; keep the simpler one. |
| 5 | **Lag compensation of non-NP entities** | Does `FGSTN_MinionHistory` + `FGSTN_LagCompTime` give a hit at the position the attacker's client displayed? | In `scenario_minion_lagcomp` (section 12) ≥ 95 % hits when aimed at the displayed minion at 60/15 ms, with the correct rewind frame; 0 hits when aimed at the *server-current* position behind the display by > 1 minion radius. |
| 6 | **AI cost** | `UStateTreeComponent` per minion (no AIController) ticking at LOD rates: ms per minion and whether it runs without a controller. | ≤ 0.02 ms per ticking minion at T0, and tick interval control works. Else brain stays a thin `UGSTN_MinionBrainComponent` with a hand-written task list behind the same intent interface. |
| 7 | **Client presentation cost** | Puppet skeletal mesh + animation for N=60 visible: game thread ms with Animation Budget Allocator, optional Animation Sharing. **Also on the listen-server host**, where simulation and puppets share one frame (section 4d.1). | ≤ 3 ms game thread at 60 puppets on the target client; on the host, batch + puppets + host's own game inside section 11's host rows; else add puppet LOD (section 8.4). |
| 8 | **Promotion at runtime** | Spawn an NP fighter mid-session and hand over from a puppet under 60/15 ms. | Client shows no pop > 15 cm and no rollback beyond the first sync; time to first control ≤ 500 ms. |

## 3. Core types (`GSTNCombatCore`, pure, Automation-tested)

Prerequisite (verify in spike 2 prep): the timeline data structs (`FGSTN_CombatTimelineData`, tracks, `FGSTN_HitboxSpec`,
`FGSTN_HitData`) and `UGSTN_HitShapeUtils` live in `GSTNCombatCore`. Anything of them still in `GSTNCombatGAS`/`NP` that
has no GAS/NP dependency (`FInstancedStruct`, `UCurveVector`, soft montage refs are Engine-level) **moves to Core in the
first M8 commit** (file move, no behaviour change).

- **`FGSTN_TimelinePlayer`** (static, stateless): `Evaluate(const FGSTN_CombatTimelineData&, int32 TimelineFrame,
  FGSTN_TimelineFrameResult& Out)` fills `{ TArray<FGSTN_ActiveHitbox> Hitboxes (local-space shape, space, key
  interpolated, HitData*, HitGroup); EGSTN_HurtState HurtState; TArray<FGSTN_HitShape> HurtOverrides; FVector
  LocalVelocity (movement track, cm/s); float GravityScale; FGameplayTagContainer Tags; TArrayView<const
  FGSTN_TimelineEvent> Events (frames in (Prev, Frame]) }`. No allocation (caller-owned scratch), O(tracks), identical on
  server and client. **M3's NP-side track lookup is refactored to call it** (golden test: same output as before for the
  M3/M4 test timelines). Used by the minion batch, by client puppets (hurt state and active hitboxes for predicted
  hits and debug draw) and by the validator.
- **`FGSTN_HitVictimView`**: `{ EGSTN_EntityKind Kind (Fighter/Minion); float Health, MaxHealth, DamagePercent (percent
  games), Weight, KnockbackResistance, HitstunResistance; EGSTN_HurtState HurtState; float ArmorThreshold; uint8
  Flags (Shielding, InHitstop...); FGameplayTagContainer Tags; FGenericTeamId Team; }`. M4's `FGSTN_HitResolutionContext`
  reads the victim through this view (a thin refactor: the fighter path fills it from its attribute snapshot, the minion
  path from the store). Steps never touch a component.
- **`UGSTN_KnockbackModel_HealthFraction`** (*Proposal* for the percent-vs-HP question): behaves as
  `PercentScaling` with `DamagePercent` replaced by `(1 − Health/MaxHealth) × HealthToPercentScale` (default 100), so
  wounded minions fly further and fresh ones barely move; `UGSTN_KnockbackModel_Fixed` stays available per definition.
  Attacker hitstop must not depend on victim data (M4 formula already does not); the model exposes
  `bAttackerHitstopIndependentOfVictim = true` and a test asserts it, because the client predicts it against a puppet.
- **`FGSTN_MinionStats`** (data): `MaxHealth, Weight, KnockbackResistance, HitstunResistance, PoiseHits (hits absorbed
  as armor, 0 = none), WalkSpeed, RunSpeed, TurnRateDegPerSec, Acceleration, Braking, GravityScale, StepHeight,
  CapsuleRadius, CapsuleHalfHeight`. Converted to attributes only on promotion.
- **`FGSTN_MinionHistory`**: ring of `K` frames (default 24 ≥ `MaxRewindMs` 250 ms at 62.5 Hz + margin) of
  `{ FVector3f Pos; float Yaw; uint8 ActionState; uint8 HurtState; uint16 HurtOverrideKey }` per minion slot, struct of
  arrays, ~16 B × 24 × N. `Sample(Handle, FrameOrTimeMs, Out)` interpolates between two frames and clamps to the ring
  (returns `bClamped`).
- **`FGSTN_MinionIntent`**: `{ FVector2f MoveDir; float DesiredSpeed; float DesiredYaw; FGameplayTag Command; int32
  TargetEntityId; uint8 Flags (Sprint, StrafeTarget); int32 StampFrame }`. Latest value wins; stale after 30 frames
  (minion idles).

## 4. Module and component layout

Everything below lives in the separate plugin **`GSTNMinion`** (section 4b); file paths are relative to its `Source/`.

| Type | File | Role |
|---|---|---|
| `UGSTN_MinionDefinition` (`UPrimaryDataAsset`, type `GSTN_MinionDefinition`) | `Data/GSTN_MinionDefinition.h` | One per minion type: `TypeTag`, `FGSTN_MinionStats`, `UGSTN_HurtboxSetDefinition`, `TArray<FGSTN_MinionMove> Moves { FGameplayTag Command; UGSTN_CombatTimeline* Timeline; int32 CooldownFrames; float MinRange, MaxRange; int32 Priority; }`, reaction timelines (`Spawn`, `Hitstun`, `Launched`, `Knockdown`, `GetUp`, `Death`), `KnockbackModel` (instanced override), `FGenericTeamId Team`, `EGSTN_MinionMovementBackend`, `PuppetClass`, `StateTree`, `LODOverride`, `PromotionFighterClass` (soft), `MaxHitsPerFrame`. `IsDataValid`: ≤ 31 moves, only supported tracks, hurtbox set present, timelines ≤ 1023 frames, `Hitstun`/`Death` timelines present. |
| `UGSTN_MinionLODSettings` (`UDataAsset`) | `Data/GSTN_MinionLODSettings.h` | Tier distances, hysteresis, intervals (section 7). |
| `FGSTN_MinionHandle` | `Runtime/GSTN_MinionStore.h` | `{ uint16 Index; uint16 Generation; }`; stale handles resolve to invalid. |
| `FGSTN_MinionStore` (plain C++) | same | Struct-of-arrays, free list, capacity `gstn.Minion.Max` (default 128, pre-allocated). Per slot: generation, definition index, position/velocity/yaw, `ActionState`, `MoveId`, `TimelineStartFrame`, `MovePausedFrames`, `HitstopEndFrame`, `HitstunEndFrame`, `NextMoveFrame[move]` cooldown stamps (small fixed array), `Health`, `KnockbackPercent`, LOD tier and phase, intent, flags, last-attacker ring (4 entries, for KO credit), `HitByRing[4] { AttackerId, TimelineStartFrame, HitGroup }` (once per group per attacker move on the victim side too). |
| `UGSTN_MinionCombatComponent` (`UActorComponent`, authority only, no tick) | `Runtime/GSTN_MinionCombatComponent.h` | The designer-facing handle on `AGSTN_MinionActor`: BeginPlay registers in the subsystem and gets a handle; Blueprint API `SetIntent`, `RequestCommand`, `GetHealth`, `GetHandle`; server delegates `OnMinionHit`, `OnMinionDied`, `OnLODChanged`. Holds no per-frame state. |
| `AGSTN_MinionActor` (`AActor`, `bReplicates = false`) | `Runtime/GSTN_MinionActor.h` | Server-side handle: root scene, the component, the brain component, no mesh and no movement component unless backend A. Pooled. Moved by the subsystem (`SetActorLocationAndRotation` only for T0/T1 and when something reads the actor: debug, AI queries; T2/T3 keep a stale transform, flagged). |
| `UGSTN_MinionCombatSubsystem` (`UWorldSubsystem`, authority: dedicated, listen host or standalone) | `Runtime/GSTN_MinionCombatSubsystem.h` | Owns the store, history, grid contribution, inbox, token allocator and spawn queue; the batch tick (section 5). |
| `IGSTN_HittableProvider` (plain interface) | `GSTNCombatNP/World/` | New in M4's world subsystem: `AppendSnapshotEntities(FGSTN_StepSnapshot&)`; NP components and the minion subsystem both implement it (section 9). |
| `AGSTN_MinionPack` (`AActor`, replicated) + `FGSTN_MinionNetArray` | `Net/GSTN_MinionPack.h` | Replication (section 8). |
| `UGSTN_MinionPresentationSubsystem` (client `UWorldSubsystem`), `AGSTN_MinionPuppet`, `FGSTN_MinionInterpolator` | `Client/` | Puppets, interpolation, hit presentation (section 8). |
| `UGSTN_MinionSpawner` (static library), `UGSTN_MinionWaveDefinition` | `Spawn/` | Spawning, waves (section 10). |
| `IGSTN_MinionMovementBackend`, `FGSTN_MinionMovement_Lightweight`, `FGSTN_MinionMovement_Mover` | `Movement/` | Section 6. |
| `UGSTN_MinionBrainComponent` + StateTree schema `UGSTN_MinionStateTreeSchema` and tasks | `AI/` | Section 9b. |
| `UGSTN_MinionBench`, `UGSTN_MinionDebugLibrary` | `Debug/` | Spike tooling, `GetStoreSnapshotJson()` for the harness, debug draw. |

## 4b. Plugin and module boundaries

Minions are optional and self-contained, so they are a **separate plugin, `Plugins/GSTNMinion/`** (like `GSTNInput` and
`GSTNCombat`), never a module of `GSTNCombat`. Combat must build, run and pass its tests with `GSTNMinion` absent.

| Plugin / module | Type | Depends on | Contents |
|---|---|---|---|
| `GSTNMinion` / **`GSTNMinionCore`** | Runtime | `GSTNCombatCore`, `GameplayTags`, `Core`, `CoreUObject` (no Engine world, no NP, no GAS) | Pure and Automation-tested: `FGSTN_MinionStore`, handles, LOD logic and scheduler, `FGSTN_MinionHistory`, `FGSTN_MinionIntent`, attack-token allocator, net item pack/unpack and quantization, `FGSTN_MinionInterpolator`, `UGSTN_MinionDefinition` data and validation, native tags. |
| `GSTNMinion` / **`GSTNMinionRuntime`** | Runtime | `GSTNMinionCore`, `GSTNCombatGAS`, `GSTNCombatNP`, `NetCore`, `NavigationSystem`, `StateTreeModule`, `GameplayStateTreeModule`, `Engine` | Subsystem, component, actor, packs, presentation subsystem and puppets, spawner, waves, brain, lightweight movement backend, debug and bench. |
| `GSTNMinion` / **`GSTNMinionMover`** | Runtime, optional | `GSTNMinionRuntime`, `Mover` | Backend A only. Loaded only if spike 3 picks it for some type; dropping it leaves the interface and backend B untouched. |
| `GSTNMinion` / **`GSTNMinionEditor`** | Editor | `GSTNMinionRuntime`, `UnrealEd` | Definition and registry validators, wave actor details, later M5 hooks. |

Rules:
- **Allowed**: `GSTNMinion*` → `GSTNCombat*` (public headers only), never the reverse. **Must not depend on `GSTNMinion`**:
  `GSTNCombatCore/GAS/NP/Editor`, `GSTNInput*`, the project module `GASP_Giustino2` (project content and code reach minions only
  through soft references or from a game-feature/sample module that is itself optional).
- `GSTNMinion.uplugin` lists `GSTNCombat` as an enabled plugin dependency. Disabling `GSTNMinion` in the project (or in a
  cooked build) removes the subsystem, packs, puppets and content; no combat code path changes, because the seams M8 adds to
  `GSTNCombat` are generic and inert without a provider: `IGSTN_HittableProvider` registration (empty list), the fork hook
  `OnPostFixedTick` (unbound), `FGSTN_TimelinePlayer` and `FGSTN_HitVictimView` (used by the fighter path too),
  `HitTargets` of 16 entries, entity-id kind bit, the `Combat.Entity.Fighter` tag. These stay in `GSTNCombat` and are reviewed as
  combat changes (separate commits, M3/M4 regression gate).
- `GSTNMinionRuntime`'s `ShouldCreateSubsystem` is false without authority (true on dedicated, listen host and standalone);
  the presentation subsystem is created wherever there is a screen (clients **and the listen-server host**, §4d.1), false on a
  dedicated server. Both are no-ops with no pack/definition loaded.
- **Isolated tests**: `GSTNMinion.Core.*` (Automation) builds with `GSTNCombatCore` only, so it runs without NP, Mover or a
  world; `GSTNMinion.Runtime.*` uses the editor world with a stub `IGSTN_HittableProvider` host and a fake NP-free snapshot, so
  no fork is needed; harness scenarios live in `Tools/GASNPPTest/minion_scenarios.py` (imports `gasnpp_test`) and need the fork
  only for the networked ones. Build check per commit: compile with `GSTNMinion` disabled, then enabled; combat tests run in both.
- Content: `Plugins/GSTNMinion/Content/` (default registry, test definitions) follows the combat plugin's `.gitignore`/LFS
  rule (combat.md §3 note); test maps stay in the project's `Content/GASNPPTest`.
- Documentation: combat.md §3 gets a row per module above; CLAUDE.md's table gets one line for the plugin.

## 4c. Gameplay tags

All tags are **native** (`UE_DECLARE_GAMEPLAY_TAG_EXTERN` in `Public/GSTN_MinionTags.h`, `GSTNMinionCore`), per CLAUDE.md,
except the designer-made ones (marked ini), which live in `Plugins/GSTNMinion/Config/Tags/GSTNMinion.ini` (the same exception as
`Config/Tags/GSTNCommands.ini`; CLAUDE.md's tag rule gets this second exception). Minions have no ASC, so **no tag below is
ever granted as a loose tag on a minion**; "who adds" means who writes it into event containers, views or queries.

| Tag | Source | Owner | Meaning | Added / removed by |
|---|---|---|---|---|
| `Combat.Entity.Fighter` | native | `GSTNCombatGAS` (`GSTN_CombatTags.h`) | Event/view marker for NP fighters | Combat event emitter adds to `Tags` of `HitLanded/HitReceived/KO` (M4) |
| `Combat.Entity.Minion` | native | `GSTNMinionCore` | Same marker for minions | Minion subsystem, in the same events and in `FGSTN_HitVictimView::Tags` |
| `Combat.Event.MinionSpawned`, `.MinionDespawned`, `.MinionPromoted` | native | `GSTNMinionCore` | Authoritative server events (combat.md §5.6) | Spawner / subsystem broadcast; never removed |
| `Minion.State.Idle/Walk/Move/Hitstop/Hitstun/Launched/Knockdown/GetUp/Dying/Dead/Spawning` | native | `GSTNMinionCore` | Names of `EGSTN_MinionActionState` for Blueprint/StateTree queries and debug; wire value is the 4-bit enum | Batch publishes into a transient container per query; derived, never stored |
| `Minion.LOD.Combat/Near/Far/Dormant` | native | `GSTNMinionCore` | Tier names (section 7) for StateTree conditions and `OnLODChanged` | Batch LOD stage; derived |
| `Minion.Type` (parent) | native | `GSTNMinionCore` | Root of type tags | Declared once |
| `Minion.Type.<Name>` (e.g. `Grunt`, `Heavy`) | **ini** | `GSTNMinion.ini` (test types in project test content) | `UGSTN_MinionDefinition::TypeTag`; filters in waves, events, quests | Designers add types; removal blocked while a definition references it (validator) |
| `Command.Minion.<Name>` (e.g. `Attack.Swing`) | **ini** | `GSTNMinion.ini` | Command tags selecting a move of a minion's `Moves` list; **never** placed in a player binding set or slot (the input conflict validator ignores `Command.Minion`) | Designers; the brain requests them |
| `Combat.State.Hitstop`, `.Hitstun`, `.Tumble`, `.Intangible` (existing M2–M4) | native | `GSTNCombatGAS` | Reused unchanged in `FGSTN_HitVictimView::Tags` so pipeline steps are shared | Minion batch fills the view per hit, from store fields (`Launched` and `Knockdown` map to `Tumble`) |
| `GameplayCue.Minion.Hit/Death/Spawn` | native | `GSTNMinionRuntime` | Default cues played locally on puppets through `UGameplayCueManager` (no ASC) | Presentation subsystem on `HitCounter` change/state change; hits' own `CueTag` takes precedence |
| `Combat.Link.Type.Minion` | reserved, **not declared in M8** | future (`GSTNMinion` when built; `Combat.Link.*` is owned by `GSTNLink`, M6 §1c) | Link type for grabbing/mounting minions (section 14); the partner is an `FGSTN_EntityRef` of kind `Minion` (M6 §7.0) | — |

No new lock, set-by-caller or hit-type tags are needed; relationships (`Combat.Relationship.*`), team (`FGenericTeamId`) and hit
types are reused.

**Entity ids.** The `Minion` kind id of `EGSTN_EntityKind` (= `FGSTN_EntityRef::Kind`, `Fighter` = 0, M6 §7.0) is declared by `GSTNMinion`; in a sync state or link a minion is `FGSTN_EntityRef{Minion, Handle.Index, low 8 bits of Handle.Generation}`. Inside the hit pipeline minions use `EntityId = MinionEntityBase | Handle.Index` with `MinionEntityBase = 0x40000000`
(*Proposal*: NP proxy ids never reach it; the `HitTargets` ring and the inbox treat it as an opaque id;
`UGSTN_CombatWorldSubsystem::FindEntity` dispatches on the bit). A 16-bit generation is checked on delivery so a hit
stamped for a recycled slot is dropped.

## 4d. Owner direction of 2026-10-22 (all *Proposal*)

### 4d.1 Listen-server host (owner direction §6b)

- **Host is authority and a player.** Its attacks on minions are server-side hits at zero latency (no lag compensation, no
  pack round trip); clients see the same hit one pack period + interpolation buffer later. This is the host advantage of
  M4 §13, now visible on every minion; accepted by default.
- **Host puppets.** The host needs a visible minion, but replication to oneself is waste: on a listen host the presentation
  subsystem builds puppets **directly from the store** (current transform, `ActionState`, `MoveId`, `HitCounter`), with no pack
  item, no interpolator and no extra render delay. Remote clients are unchanged. Same puppet class, so cost per puppet is the
  client cost of section 11, and the host pays batch + puppets + its own game in one frame.
- **Budget consequence.** Section 11 gains host rows: the minion batch gets at most **~10 % of the 16 ms frame** (target
  N=40: mean ≤ 1.0 ms, p99 ≤ 2.0 ms, tighter than the dedicated row), puppets ≤ 2 ms game thread, the rest is the host's own
  game. If the spike does not fit, in order: lower `MaxActiveT0` (48 to 32), puppet LOD earlier, lower T0 rate; **record the max
  N on the host honestly**, as the rule of section 6 already does. N=100 stays a dedicated-only stretch.
- **Host upload bytes per minion.** The limit is the host's home upload, paid per connected client. Per minion update
  18 B (19 B with `ExtByte`, section 8.2); 40 typical minions ≈ 5.2 KB/s per client, so a host with 3 clients ≈ **16 KB/s
  (~130 kbit/s)** and 7 clients ≈ 36 KB/s: inside a 5 Mbit/s home upload, but the per-client figures of 8.3 are the ones to
  defend because they multiply. *Proposal*: per-client cap stays ≤ 5 KB/s at N=40 and a host-total soft cap
  `gstn.Minion.HostUploadBudgetKBps` (default 40) lowers the T0 rate to 10 Hz before it is exceeded.
- **Host leaves = session ends** (owner direction); the store has no migration, nothing to design here.

### 4d.2 Parts through `GSTNPartsMinion` ([combat-parts.md](combat-parts.md) §5.5)

- `GSTNMinion` stays free of parts. The bridge plugin adds `FGSTN_PartsMinionExtension { PartSet }` on `UGSTN_MinionDefinition`
  (≤ 8 parts), store columns `BrokenMask` (`uint8`) and `PartDamage[8]` (9 B per part-bearing minion), and runs the **same**
  `UGSTN_HitStep_PartResolve` from stage 2.2 through `FGSTN_HitVictimView` (`FGSTN_PartsSnapshotExt`). The M8 side only
  provides: the optional pipeline step slot, `HurtboxIndex` in `PendingHits` entries, the `ExtByte` of the wire (8.2) and an
  `OnOutcomeApplied` store hook; nothing else knows parts.
- **Health and weight (Helldivers-like).** The minion's health stays `Health` (5 bits on the wire). Under `PercentCapped`
  `HealthFraction = 1 - Progress`, the same number as `KnockbackModel_HealthFraction` (§3), so minions and fighters agree on
  "how wounded". Effective weight = base weight × weight curve of progress × lost mass of broken parts (combat-parts §4.3),
  derived at each resolution from the store, never stored: a battered minion flies further.
- **Stagger and juggle.** `StaggerFactor` and `JuggleFactor` per type (`BodyReaction`) and per part enter as
  `FGSTN_HitReactionModifiers` (M4 seam, combat-parts §1 and §4.4), so **hitstun and hitstop depend on the hit and on the part
  hit**: a `StaggerValue` below the threshold gives `bNoFlinch` (`PoiseHits` stays the coarse version for types without parts).
  Attacker hitstop uses only static data (band and part reaction), never the victim's broken mask or health (the assertion
  of §3 is extended to the part-aware model).
- **Wire.** `ExtByte = BrokenMask` only for part-bearing types (`bHasExt`, 0 bits otherwise), dirty only on change: about +1 B
  per update (+5 %). Chips follow the existing 5-bit `Health`, limbs follow the mask (combat-parts §5.5).
- **Promotion** copies `BrokenMask`/`PartDamage` into the fighter's `FGSTN_PartsFragment` (combat-parts §5.5).

### 4d.3 Grabbable big types, throwing minions

- A heavy type with a part set stays ungrabbable while the projected tag `Combat.Part.GrabLocked` is present (combat-parts §7);
  the store mask feeds the same function, so after enough parts break it becomes grabbable through a rules-asset edit only.
  A boss keeps `Combat.Immunity.Grab`.
- Any grabbable minion can be **grabbed and thrown at other minions**: M6 owns the link, [M10](combat-m10-held-objects.md)
  owns the throw (the thrown minion becomes a projectile entity with hitboxes, credit to the thrower; link type
  `Combat.Link.Type.Minion` stays reserved here, section 14). M8's contribution: a held or thrown minion is removed from the grid
  and from the attack queries but keeps its slot (generation unchanged), so the landing can **return** it (wall splat, bounce or
  destruction by designer data, the M4 surface-impact rules referenced, not redefined) or kill it.

### 4d.4 Death: crystal shatter (presentation only)

- `Dying` plays the death timeline as now; at `Dead` the puppet is replaced by the shatter of combat-parts §6.3
  (pre-fractured pieces, **pooled**; Geometry Collections only for players and bosses, never minions) driven from the KO event
  and the last hit's direction. Limbs: pooled chunk + stump cap (combat-parts §6.2 option A), keyed
  `(EntityRef, PartIndex, BreakSerial)`. The corpse is released after `CorpseSeconds` as before; budgets and the "40 deaths in
  2 seconds" bench case are those of combat-parts §6.4, **measured on the host** (it renders). Skipped debris never changes
  gameplay.

## 5. Batch tick (server) and order

Per fixed-tick frame F on the server:

1. **NP models in priority order**: combat (47) → ASC (48) → Mover (50), for every fighter. The first combat tick of F
   builds snapshot S(F) from end-of-F-1 state; the minion provider contributes every minion at its end-of-F-1 state
   (the store has not been written this frame). Fighters hit minions here: delivery writes a stamped entry (`Frame = F`)
   into `UGSTN_MinionCombatSubsystem::PendingHits` instead of a component inbox.
2. **Post-fixed-tick hook** (fork, spike 2) → `UGSTN_MinionCombatSubsystem::TickBatch(F)`. If no fighter ticked (empty
   server) it builds S(F) itself. Stages, each a tight loop over the minions scheduled for F (section 7 bucketing):
   1. **LOD update** (tier from nearest-player distance and activity, hysteresis).
   2. **Inbox drain**: entries with `Frame < F`, sorted by (victim index, `AttackerId`, hitbox order) — the M4 rule
      (reaction at F+1, trades order-independent). Per entry: build `FGSTN_HitVictimView` from the store, run the
      pipeline of the rules asset (steps `TeamFilter`, `HurtState`, `ComputeKnockback`, `ApplyDamage`, `Hitstop`;
      `MeterGain` is skipped for minion victims) → write `Health`, `KnockbackPercent`, `ActionState` (`Hitstop` →
      `Hitstun`/`Launched`), `HitstopEndFrame`, `HitstunEndFrame`, a pending launch velocity, cancel of the current move,
      `HitCounter++`, last-attacker ring; at 0 health → `Dying` (death timeline) and a `KO` event. `ExtraEffects` of the hit
      are ignored for minions in v1 (counter `STAT_GSTN_MinionIgnoredEffects`; a minion status map is a later hook,
      section 14).
   3. **Timeline advance + attack queries** (hitboxes use end-of-F-1 transforms, exactly as fighters, M4 §6): start the
      requested command (cooldown, token, state checks), advance `TimelineFrame` (paused frames excluded in hitstop), evaluate
      `FGSTN_TimelinePlayer`, query active hitboxes against **fighters only** (the grid is queried with the `Fighter` kind
      mask; minion–minion is not tested, they share a team; variants still apply by relationship), hit groups via
      `HitByRing`-style dedupe on the attacker side (the minion's own entries), deliver to the fighter's inbox with
      `Frame = F`. The player drains it in its combat tick of F+1 → correction on its client (spike 2.1 of M4).
   4. **Movement** through the backend (section 6): desired velocity from intent and the timeline's movement track,
      knockback integration, gravity, collision, ground snap, minion–minion separation.
   5. **Publish**: grid cell update (T0/T1), history ring write for frame F, replication dirtying (section 8), debug draw.
3. **Next frame**: S(F+1) reads the store as written in 2. Fighters therefore see minions exactly one frame older than
   their own tick, consistently for every fighter, regardless of NP id order.

The AI (StateTree) runs in the normal world tick, not in the fixed tick; it only writes `FGSTN_MinionIntent` (latest wins).
This is server-only behaviour that is never rolled back, so it needs no determinism; the batch is deterministic given the
store, the intents, S(F) and the inbox (the order inside a stage is by slot index, tests rely on it).

## 6. Movement: options and decision rule

Both options implement `IGSTN_MinionMovementBackend::StepBatch(FGSTN_MinionMoveContext&, TArrayView<const int32> Slots)`.
Output: new position/velocity/ground flag per slot. Selected per type (`EGSTN_MinionMovementBackend`), default from
`UGSTN_MinionLODSettings`.

| | A: Mover, non-predicted | B: lightweight (`FGSTN_MinionMovement_Lightweight`) |
|---|---|---|
| What | A `UMoverComponent` per active minion on the server, simulated outside NP (no proxy, no sync state replicated), driven with layered moves (the same `FGSTN_LayeredMove_Knockback` and freeze types of M4) | Kinematic capsule in the store: desired velocity + gravity + launch → one capsule sweep against static world (channel `GSTN_MinionBlock`), one slide along the normal, one down-trace for ground snap within `StepHeight`; minion–minion separation by the spatial hash (radius push), no physics body |
| Pros | Slopes, steps, modes, falling and (future) wall modes for free; one movement code path for promotion | 2–3 traces per active minion-frame, no per-minion component, batchable, trivial LOD (variable dt) |
| Cons | Component per minion (memory, tick overhead), unknown behaviour outside NP in this fork (spike 3 checks), harder to batch and to LOD | Own collision code to maintain: no wall modes, no complex shapes; must be hardened against tunnelling and corners |
| Unknowns | Cost per Mover outside the NP service; `bUseNetworkPrediction` equivalents | Collision correctness on the project's levels |

**Spike 3 protocol**: same `Chase` and `Massacre` scenarios, N=60, T0 for all, 62.5 Hz fixed dt, plus a hostile test
course (stairs, 40° slope, thin wall, corner, ledge). Measure `cA`, `cB` = mean ms per minion-frame of the `Movement`
scope, and robustness = failures in 10 minutes with 60 minions and random launches (3000 cm/s): fell out of world,
stuck > 2 s, tunnelled a wall.

**Decision rule** (*Proposal*, applied by whoever runs the spike):
1. Choose **A** only if `cA ≤ 0.03 ms` **and** `cA ≤ 2 × cB` **and** A has 0 failures. (Then B is not built beyond the
   interface; promotion reuses the same movement.)
2. Otherwise choose **B** if `cB ≤ 0.015 ms` and it has 0 failures after fixes; B is the expected outcome.
3. If neither passes, ship B with `MaxActiveT0` reduced (section 7) and record the maximum N honestly.
Per-type override stays: flying or large types may use A (few instances).

## 7. LOD tiers (server)

`UGSTN_MinionLODSettings` (distances are to the nearest **living** fighter on the plane, cm; all cvar-overridable,
`gstn.Minion.LOD.*`). Hysteresis: a tier change needs the distance beyond the border ± 10 % and a **dwell ≥ 30 frames**
(hits, spawning and death force T0 without dwell).

| Tier | Condition | Batch interval | Work |
|---|---|---|---|
| **T0 Combat** | ≤ 1500, or hit/attacking/launched/dying | 1 | Everything: AI at 30 Hz, timeline, queries, movement, grid, history, replication at 15 Hz |
| **T1 Near** | ≤ 3500 | 2 (dt × 2) | Timeline and movement, queries only while an attack is active, grid every 2 frames, AI at 10 Hz, replication 5 Hz |
| **T2 Far** | ≤ 8000 | 4 (dt × 4) | Movement without knockback support (steering + ground snap every 8 frames), no queries, not in the grid (cannot be hit), AI at 2 Hz, replication 1 Hz |
| **T3 Dormant** | > 8000 or idle with no fighter within range | 16 (wake check only) | No movement, no AI tick, removed from the grid, `bDormant` in the pack |

- Why skipping is safe: timelines and cooldowns are frame stamps, so a minion stepped every Nth frame advances its
  timeline by N frames in one go (`TimelineFrame` jumps; events between are fired once). Attack timelines never start at
  T2/T3 (a command needs T0/T1).
- **Scheduling**: slot `Phase = Index % Interval`; a minion runs in frame F when `(F + Phase) % Interval == 0`, so cost is
  spread evenly. T0 minions run every frame; `MaxActiveT0` (default 48) caps them — the farthest T0 minions are demoted to
  T1 without being attackers first (attackers are never demoted: attack tokens already cap them, section 9b).
- Wake: any hit, any fighter entering the T1 radius, an explicit `Wake()`.
- Debug: `gstn.Minion.Debug.LOD 1` colours capsules per tier; `stat GSTNMinion` shows counts per tier.

## 8. Replication

### 8.1 Packs and relevancy

Minions are not actors on clients. `AGSTN_MinionPack` (replicated, `NetPriority` low, `bAlwaysRelevant = false`) holds a
`FGSTN_MinionNetArray` of up to **16** items; the subsystem assigns a minion to a pack by spawn group (a wave spawns
into whole packs), re-packing only at spawn. Per pack: `IsNetRelevantFor` = any viewer within `8000 cm` of any member
(plus always relevant while a member is T0 near that viewer); `NetUpdateFrequency` follows the pack's **best tier**
(T0 15 Hz, T1 5 Hz, T2 1 Hz, T3 dormant via `SetNetDormancy`, flushed on wake); `NetCullDistanceSquared` is not used.
Spike 4 compares this with an Iris object filter; keep the simpler one that meets the byte budget.

### 8.2 Item layout (`FGSTN_MinionNetItem : FFastArraySerializerItem`, custom `NetSerialize`)

| Field | Bits | Notes |
|---|---|---|
| `Id` | 8 (index inside the pack) | Pack-local; the global handle is `PackId << 4 | Id` (pack id replicated once) |
| `TypeIndex` | 8 | Index into `UGSTN_MinionRegistry` (project-wide list of definitions, same order on both sides); sent on add only, then 0 bits via `bHasType` |
| Position X, Y, Z | 3 × 19 | cm, relative to the registry's `WorldOrigin` (±2.6 km at 1 cm) |
| Yaw | 8 | 1.4° steps |
| `ActionState` | 4 | `Idle, Walk, Move, Hitstop, Hitstun, Launched, Knockdown, GetUp, Dying, Dead, Spawning` |
| `MoveId` | 6 | Index in the type's move list + reaction timelines (0 = none) |
| `MoveStartFrame` (low bits) | 10 | Server frame of the move start, wrap 16 s; client reconstructs from its frame estimate |
| `FrameStamp` (low bits) | 8 | Server frame of this sample, wrap 4.1 s, for interpolation |
| `HitCounter` | 3 | Increments per accepted hit, detects repeated hits; drives presentation and de-duplicates the attacker's predicted flash |
| `Health` | 5 | 1/32 of max; the pack also carries `MaxHealth` per type from the definition |
| `ExtByte` (*Proposal*, owner direction 2026-10-22) | 0 or 8 | Optional extension byte guarded by a type-level `bHasExt` (0 bits for types without an extension), owned by a bridge plugin: `GSTNPartsMinion` sends `BrokenMask` (combat-parts §5.5). Dirty only when it changes. Not counted in the total below |
| `Speed` | 6 | 0..`RunSpeed`, animation blend and extrapolation |
| `Flags` | 3 | `Teleport` (no interpolation across), `Promoting`, `Elite` |
| **Total** | **~121 bits ≈ 15.2 B** | + FastArray header (ReplicationID, change mask) ≈ **3 B** → **~18 B per item update** (**~19 B** for part-bearing types) |

Dirty rules: an item is marked dirty when position moved > 3 cm or yaw > 4° or any other field changed, at most once per
pack update; an idle minion costs 0 bytes. Removal = FastArray remove (death after the death timeline plus a grace time).

### 8.3 Byte budget (per client, per second; on a listen host multiply by the connected clients, §4d.1)

Wire cost per update 18 B (`bytesPerItem`); rates from section 7.

| Situation | Calc | Bytes/s |
|---|---|---|
| 12 T0 minions moving/fighting | 12 × 15 Hz × 18 | 3240 |
| 20 T1 | 20 × 5 × 18 | 1800 |
| 8 T2 | 8 × 1 × 18 | 144 |
| **40 minions typical** | | **≈ 5.2 KB/s** (target: ≤ 5 KB/s, hard ≤ 8) |
| 100 minions, 24 T0 | 24×15×18 + 36×5×18 + 40×1×18 | ≈ 10.4 KB/s (cap: ≤ 12) |

Out-of-relevancy and dormant minions cost 0. If the measured number exceeds the cap, in order: lower T0 rate to 10 Hz
(interpolation hides it), drop `Speed`/`Health` precision, delta-code position against the previous item of the same
connection (needs Iris or a custom `NetDeltaSerialize`; not planned).

### 8.4 Client: puppets, interpolation, presentation

- `UGSTN_MinionPresentationSubsystem` listens to `PostReplicatedAdd/Change/Remove` of every pack: acquire a pooled
  `AGSTN_MinionPuppet` (skeletal mesh + anim instance of the definition, no capsule, no movement component, tick
  disabled except the animation) on add (first sample `Teleport`, so late joiners see minions at current positions in
  their current move: the move start frame is reconstructed, the montage starts at the matching offset); release on
  remove. Puppet LOD: far puppets use the lowest mesh LOD, animation budget allocator, optionally Animation Sharing;
  hidden when culled.
- **`FGSTN_MinionInterpolator`**: per puppet a ring of 8 samples keyed by reconstructed server frame
  (`FrameStamp` + the client's estimate of the server frame at receipt). Render time = the NP interpolation time of the
  client (the same clock `FixedTickInterpolationBufferedMS` defines for interpolated proxies; spike 2 finds the accessor
  or adds a one-line getter to the fork). *Proposal*: **minions are displayed on the same render timeline as interpolated
  players** (same buffer), so a minion and a remote player hit on the same server frame are shown together and the
  lag-compensation time of section 9 is one value. Between samples: linear position, shortest-arc yaw; when no sample is
  newer, extrapolate with `Speed` and yaw for ≤ 150 ms, then hold; on `Teleport` snap. Interpolated state also feeds
  puppet animation (speed, state) and the montage choice (`ActionState` + `MoveId` + start frame → montage position;
  hitstop pauses the montage).
- **Hittable on the client**: each puppet registers in `UGSTN_CombatWorldSubsystem` (provider on the client) with its
  interpolated transform; hurt state and overrides come from `FGSTN_TimelinePlayer::Evaluate(MoveId, frame)`. The
  attacker's M4 prediction (hit confirm, own hitstop, `HitTargets` entry) therefore works against minions with no new
  code in the attacker path; a disagreement with the server is the usual attacker reconciliation.
- **Hit presentation** (`UGSTN_HitPresentationComponent` of M4, now also for minions): a new predicted `HitTargets` entry
  with a minion id plays flash, local cue and puppet mesh pause for the hitstop length immediately; the authoritative
  `HitCounter` change confirms it (de-duplicated by the expected counter value for that attacker within 500 ms) or, if
  no confirmation comes in 500 ms, ends the effect. Other clients play the same effects on `HitCounter` changes only.
  Reaction (hitstun/launch) shows when `ActionState` arrives: expected delay = pack period + interpolation buffer, which
  the immediate flash/pause must cover (M4 risk); a launched puppet is extrapolated ballistically from the first
  `Launched` sample (velocity implied by two samples) for smoothness.
- Death: `Dying` plays the death timeline's montage; `Dead` is held for `CorpseSeconds` then released; ragdoll is a cosmetic
  option on the puppet (client-only, no gameplay effect).

## 9. Hit flows

### 9.1 Registry and snapshot (changes to M4 §5)

`UGSTN_CombatWorldSubsystem` keeps NP-component entries and gains `RegisterProvider(IGSTN_HittableProvider*)`. The snapshot
builder calls `AppendSnapshotEntities` of each provider; the minion provider writes, per minion in T0/T1 and not `Dead`:
`EntityId`, transform and velocity, facing, `HurtState` and override key (from the current timeline frame, cached by
the batch), team, `bInHitstop`, action state, and the `FGSTN_HitVictimView`. Entities carry a `Kind` bit for the grid
masks (`Fighter`/`Minion`). The grid is one uniform grid shared by both kinds; minions update their cell in stage 2.5.
On a client the provider is the presentation subsystem (puppets). `RestoreFrame` invalidation of M4 stays unchanged: the
store is server-only and never rolls back.

### 9.2 Player → minion

1. Client attacker (predicted, M4 §6) as above; the server attacker ticks the same hit in its combat tick of F.
2. Server, remote attacker with `bUseLagCompensation`: **minions are not NP actors, so `RewindActors` does not move
   them.** The attacker builds its query as follows: rewound fighters through `RewindActors` as in M4 §9, minions through
   `FGSTN_MinionHistory`: candidate minions come from the *current* grid inflated by `MaxMinionSpeed × RewindMs`
   (cm, default 700 × 0.25 = 175) around the attacker's hitbox bounds; for each candidate `Sample(Handle, RewindFrame)`
   gives the transform and hurt state at that frame; the overlap test uses it. No per-attacker grid rebuild. AI
   attackers never rewind.
3. **`FGSTN_LagCompTime`**: one conversion (server frame ↔ the rewind time `RewindActors` receives) so both overlays use
   the same instant: the frame the attacker's client displayed = its last input's interpolation time (M4 §9), which is
   also the minion render time of section 8.4 (the *Proposal* that makes this a single number). `MaxRewindMs` clamps both.
4. A new hit goes to `PendingHits` (stamped F, generation-checked); drained at F+1 (stage 2.2): damage, launch, hitstun,
   `HitCounter++`. Dedupe: `HitTargets` on the attacker, `HitByRing` on the victim (a hit that the victim already has for
   `(AttackerId, TimelineStartFrame, HitGroup)` is dropped, protecting against re-delivery after attacker re-simulation
   on the server, which cannot happen today but is cheap to keep).
5. **Crowd cleave**: a spin through 10 minions overflows M4's 8-entry `HitTargets` ring; M8 raises it to **16 entries**
   (*Proposal*, a change to M4 §11 sync state, +48 B worst case, delta-cheap) and, when the ring is full, evicts the oldest.
6. Result on the attacker's client: predicted flash and own hitstop immediately; server reaction displayed ≈ pack period
   + interpolation buffer later.

### 9.3 Minion → player

Stage 2.3 (section 5): the minion's hitboxes (world shapes from `Root`/`BakedSocket`/`WorldAtMoveStart` of its timeline,
end-of-F-1 transform) are tested against fighters in S(F); relationship = minion team vs fighter team via the same resolver,
variants apply; an accepted hit is a stamped inbox entry on the fighter's component (the M4 victim inbox, written by the
server which ticks that instance). The player's client receives it as a **correction** (rollback of about RTT/16 frames,
M4 §12.7); this is the number to budget:
- Per-player rollback load: hits/s from minions × RTT/16 frames × 0.03–0.04 ms per re-simulated entity-frame. Two hits in
  the same frame are one inbox drain, so one rollback.
- **Attack tokens** (`FGSTN_MinionAttackTokens`, in the subsystem): at most `MaxSimultaneousAttackersPerPlayer` (default 4)
  minions hold a token to *start* an attack against a given fighter; others circle/wait (AI hook, section 9b). This bounds
  both the corrections per second and the visual noise; it is a rule of the subsystem, not of the AI.
- Hitstop: the minion's attacker hitstop is real and applies to its timeline (frame stamps); the fighter's hitstop, DI, tech
  etc. are M4/M6 fighter logic, untouched.
- `UGSTN_CombatRules.HittableRelationships` and `bFriendlyFire` apply: enemy AoE hitting minions of the same team is a
  rules-asset question (default: minions ignore minions).

## 9b. AI hook-up (StateTree; not an AI design)

- `UGSTN_MinionBrainComponent` (on `AGSTN_MinionActor`, server) runs a **`UStateTreeComponent`** with the custom schema
  `UGSTN_MinionStateTreeSchema` (context: the actor, the minion component, subsystem queries: nearest fighters, token
  request, LOD tier). StateTree over Behavior Tree because it runs without an `AAIController`/pawn, supports a tick
  interval per LOD cheaply and its tasks are plain structs (spike 6 confirms the cost; the fallback is a hand-written
  task list behind the same intent API).
- Output only through `FGSTN_MinionIntent` (section 3) via the minion component: `SetMoveTarget(Location|Entity)`,
  `Face(Entity|Yaw)`, `RequestCommand(Tag)`, `Idle()`. The batch converts a move target into `MoveDir` with
  `UGSTN_MinionSteeringUtils` (seek, arrive, separation; no path following: a StateTree task asks the navigation system
  for a path asynchronously at ≤ 2 Hz and feeds waypoints as successive targets).
- Provided tasks/conditions (small, generic): `GSTN_MoveToTarget`, `GSTN_FaceTarget`, `GSTN_RequestAttack` (acquires a token,
  waits for `OnMoveFinished`), `GSTN_TargetInRange(Command)` (uses the move's `MinRange/MaxRange`), `GSTN_IsLODAtLeast`.
  Reactions to hits are not decided by the brain: the batch interrupts moves on hit (hitstun is not negotiable); the
  brain receives `OnMinionHit`/`OnMinionDied` events.
- Players use `UGSTN_AIInputSource` through their command; minions do **not**: a command tag only selects a move of the
  minion's own move list.

## 10. Spawning, pooling, promotion

- `UGSTN_MinionSpawner::Spawn(World, Definition, Transform, FGSTN_MinionSpawnParams {SpawnGroup, InitialTarget, bSpawnTimeline})`
  returns a handle (invalid when the store is full or the per-frame spawn budget is spent: spawns go through a queue with
  `gstn.Minion.SpawnsPerFrame` = 4 to avoid hitches). The store slot, a pooled `AGSTN_MinionActor` and a free pack item
  (or a new pack) are claimed; the minion starts in `Spawning` (spawn timeline, intangible by its `HurtboxModifier`).
- Despawn/death: slot freed after the death timeline + `CorpseSeconds`; actor returned to the pool (components reset,
  StateTree stopped); handle generation bumped; pack item removed; clients release the puppet. Pools: server actors (cap
  `gstn.Minion.Max`), client puppets per type (`PuppetPoolPerType`, default 16, grows).
- `UGSTN_MinionWaveDefinition` (`UDataAsset`): `Entries { Definition, Count, SpawnPoints tag/query, DelayFrames }`, a
  trigger-driven `UGSTN_MinionSpawnerActor` places it; alive count reported to Blueprint (`OnWaveCleared`).
- **Promotion to fighter**: `UGSTN_MinionSpawner::Promote(Handle, PromotionParams {FighterClass override, bKeepHealthFraction,
  PromotionTimeline})`: (1) server marks the minion `Promoting` (flag replicated, the minion becomes intangible and idle);
  (2) spawns `PromotionFighterClass` (a Pawn with `UGSTN_CombatSimComponent`, Mover, ASC, AI input source) at the same
  transform/yaw; attributes initialised from the kit's init effect, then `Health` = `MaxHealth × HealthFraction`; the
  fighter starts the promotion timeline ability (adopted, M3 §7.4); (3) removes the minion (slot, pack item); the item
  carries `Promoting` and the fighter replicates `PromotedFromMinionId`, so the client cross-fades the puppet into the
  fighter actor (hold the puppet until the fighter's first interpolated transform). The in-flight move is dropped (the
  promotion timeline masks the swap). No demotion. Cap `MaxFightersFromMinions` (default 4) because each is a full NP
  entity (spike 1 numbers). Kill credit, team, StateTree asset and the last-attacker ring carry over.

## 11. Targets and budgets (authority host, 62.5 Hz = 16 ms frame; listen-server host rows added, §4d.1; to confirm in spike 1/3/4/7)

| Metric | Target N=40 | Target N=60 | Stretch N=100 |
|---|---|---|---|
| Server `GSTN_MinionBatch` mean / p99 (dedicated) | ≤ 1.2 / 2.4 ms | ≤ 2.0 / 3.5 ms | ≤ 3.5 / 6 ms |
| **Host** `GSTN_MinionBatch` mean / p99 (listen server, also renders; ≤ ~10 % of the frame) | ≤ 1.0 / 2.0 ms | ≤ 1.6 / 3.0 ms | not targeted |
| **Host** game thread incl. its own player and host puppets (4 players + minions, Brawl) | ≤ 11 / 14 ms | ≤ 13 / 16 ms | not targeted |
| **Host upload** (per-client figure × clients, 3 clients) | ≤ 16 KB/s | ≤ 21 KB/s | not targeted |
| Whole server game thread (4 players + minions, Brawl) | ≤ 8 / 12 ms | ≤ 10 / 14 ms | ≤ 13 / 16 ms |
| Per minion-frame at T0 (all stages) | ≤ 0.03 ms | ≤ 0.03 ms | ≤ 0.035 ms |
| Network per client | ≤ 5 KB/s | ≤ 7 KB/s | ≤ 12 KB/s |
| Server memory per minion | ≤ 400 B store (+ actor); +9 B with parts | | |
| Client game thread for puppets | ≤ 2 ms | ≤ 3 ms | ≤ 5 ms (LOD) |
| Corrections on a player from minion hits | ≤ 1 per hit frame; ≤ 6 per second under 4 tokens | | |

## 12. Test content, tools and harness

- Content: two test definitions in `Content/GASNPPTest`: `DA_Minion_Grunt` (walk, 1 swing with a root hitbox and a baked-socket
  hitbox, hitstun, launched, death, 40 HP, `KnockbackModel_HealthFraction`) and `DA_Minion_Heavy` (armor, slower, overhead). Maps:
  `L_MinionYard` (flat), `L_MinionCourse` (stairs, slope, thin wall, corner, ledge). Python helper
  `Tools/GAS/setup_minion_assets.py` (idempotent, backs up like `setup_gas_assets.py`) creates them. **Tell the user** before
  touching shared Blueprints/maps; no change to `SandboxCharacter_Mover` is needed in M8 (the fighter side is M4).
- Console: `gstn.Minion.Debug.{Capsules,Hitboxes,LOD,History,Intent}`, `gstn.Minion.Bench.*`, `stat GSTNMinion`.
- **Automation (`GSTNCombat.Minion.*`, Core and Minion module, pure where possible)**: store alloc/free/generation reuse;
  handle staleness; LOD thresholds, hysteresis and dwell; bucket scheduling spread (≤ ±1 minion per frame);
  `FGSTN_TimelinePlayer` goldens (identical to the pre-refactor M3/M4 results, frame-skipping fires events once);
  `FGSTN_HitVictimView` through each pipeline step with fake contexts for a minion victim; `KnockbackModel_HealthFraction`
  values and the attacker-hitstop independence assertion; inbox ordering and generation drop; history ring interpolation,
  clamp and `MaxRewindMs`; net item serialization round trip with quantization bounds (position ≤ 0.5 cm, yaw ≤ 0.8°),
  frame-stamp wrap reconstruction near 8-bit and 10-bit boundaries; interpolator (render time between samples,
  extrapolation cap, teleport, out-of-order); attack token allocator (cap, release on death/hit); lightweight movement on a
  test world (flat, step, slope, wall slide, no tunnelling at 3000 cm/s, ground snap); definition `IsDataValid` cases.
- **Harness (`gasnpp_test.py`, 60/15 ms, with and without 2 % loss; helpers `minion_spawn`, `minion_snapshot` via
  `UGSTN_MinionDebugLibrary::GetStoreSnapshotJson` on the server, puppet positions via the test component)**:
  `scenario_minion_replicate` (server spawns 40 over 3 packs; after settling, each client's interpolated puppet position is
  within 20 cm of the server's position at the displayed frame; bytes/s read from `stat net`), `scenario_minion_late_join`
  (a client connects with 40 active), `scenario_minion_player_hits_minion` (c1 jabs a stationary grunt: server health, launch
  and hitstun equal what the model computes; c1's predicted hitstop and flash precede the reaction; c2 sees the reaction),
  `scenario_minion_lagcomp` (grunt strafing at 400 cm/s; c1 attacks the displayed position: hit with lag compensation,
  miss without; attack at the server-current position behind the display: miss), `scenario_minion_hits_player` (grunt jabs
  c1: corrections on c1 equal the hit count, snapshots equal after), `scenario_minion_tokens` (12 grunts vs c1: ≤ 4
  attacking at once), `scenario_minion_lod` (c1 walks away: tier transitions, no popping, a hit wakes a dormant minion),
  `scenario_minion_promote` (grunt → fighter, client cross-fade, no pop > 15 cm), `scenario_minion_horde` (60 minions, both
  clients fighting 60 s: no unexplained reconciles beyond one per hit, server metrics inside section 11). Clean = snapshots
  equal and `report_reconciles(mark).unexplained == 0` (testing.md), the expected corrections counted.

## 13. Acceptance criteria

1. Spikes 1–8 recorded in combat.md §17 with measured numbers; the movement backend chosen by the rule of section 6, the
   byte layout adjusted to the measurement.
2. At N=40 and N=60 the targets of section 11 hold in `scenario_minion_horde`; the max N within the targets is recorded.
3. A designer builds a minion type (definition, reuse of an existing timeline, StateTree) and spawns a wave without C++.
4. Player→minion hits: lag compensation scenario passes; predicted attacker hitstop matches the server (no attacker
   reconcile in `scenario_minion_player_hits_minion`); crowd cleave over 12 minions hits each once.
5. Minion→player hits: one correction per hit frame on the victim's client, state equal afterwards; token cap enforced.
6. Late join, LOD transitions and promotion scenarios clean.
7. M1–M4 tests and scenarios still clean (the `FGSTN_TimelinePlayer` and `FGSTN_HitVictimView` refactors must not change a
   result); Core and GAS modules have no NP/Minion dependency.
8. combat.md updated (§3 modules, §13, §15, §17, §18) and this file where implementation differs.

## 14. How to extend

- **Parts on a minion type** (owner direction 2026-10-22): enable `GSTNPartsMinion`, add a `UGSTN_PartSetDefinition` (≤ 8 parts) to
  the definition's extension; no M8 change (§4d.2).
- **New minion type**: create `UGSTN_MinionDefinition` (stats, hurtbox set, moves pointing at existing or new
  `UGSTN_CombatTimeline` assets, reaction timelines, puppet mesh/anim, StateTree); add it to `UGSTN_MinionRegistry` (append
  only: the index is the wire id; `IsDataValid` blocks reordering by storing a stable id table); spawn it from a wave. No C++
  unless it needs a new track, step or task.
- **New attack**: author a timeline (M5 editor when available) with `Hitbox`/`HurtboxModifier`/`Movement` tracks; add an
  entry to `Moves` with its command tag, range and cooldown; it plays on fighters unchanged.
- **New behaviour**: a StateTree asset using the provided tasks, or a new task struct deriving the minion task base.
- **New hit rule** (e.g. minion armor): a pipeline step in `UGSTN_CombatRules`; it sees the minion through
  `FGSTN_HitVictimView` — do not read components in steps.
- **Status effects on minions** (burn, slow): *later hook*: `UGSTN_MinionStatusDefinition` mapping a hit's extra-effect
  tag to a store-level status (timer stamp + tick), still no ASC; ignored in v1.
- **New movement backend**: implement `IGSTN_MinionMovementBackend`, add the enum value; flying types are the first user.
- **Shield/grab/mount on minions** (M6/M7 follow-ups): grabs and mounts of minions need a different link type
  (`Combat.Link.Type.Minion`, the minion's store slot as parked entity, referenced by `FGSTN_EntityRef{Kind = Minion}`; needs `GSTNLink`, M6 §7.0); shields use the `FGSTN_HitVictimView` shield fields and the
  M6 step. Not designed here. *Revision 2026-10-22:* the owner wants minions grabbed and thrown at other minions; the throw is
  specified in [combat-m10-held-objects.md](combat-m10-held-objects.md), the grab in M6, the grabbability gate in §4d.3.
- Related: promotion is the only supported way to give a minion prediction, GAS effects or loadouts.

## 15. Risks

- **Host frame budget** (listen server, owner direction §6b): batch, puppets, shatter debris and the host's own game share
  one frame, so the dedicated targets are not enough; the host rows and the `MaxActiveT0`/puppet LOD fallbacks of §4d.1 are
  the mitigation. Host advantage (zero-latency hits on minions) is accepted unless the owner asks for parity.
- **Parts bridge** (`GSTNPartsMinion`) adds `ExtByte` to the wire and a part-aware resolve per hit (≤ 2 µs): both are measured
  in the bench; the M8 wire change lands as its own commit.
- **The fighter-path baseline may already be enough** (spike 1): then M8 shrinks to configuration of AI fighters and
  server LOD; the exit rule is explicit so effort is not wasted.
- **Fork hook for the post-fixed-tick** (spike 2) is a fork patch in the NP service: keep it small and inert. Tell the user
  before patching. If impossible, the fallback is a tickable subsystem ordered after the world manager's tick group; then
  a minion may read a frame boundary one frame late (documented, acceptable, tests adjusted).
- **Hits from many minions cause many corrections** on the player's client; tokens bound it, the player's number is measured in
  `scenario_minion_horde`. A client-side hit inbox (M4 risk) is the real fix and stays a separate decision.
- **Own collision code** (backend B): tunnelling and corner cases; mitigated by the hostile course, the 10-minute soak and
  keeping A selectable per type.
- **Lag compensation drift**: the interpolation buffer, pack rate and rewind clamp must agree; if the minion render time
  diverges from the player's (e.g. a lower buffer for minions), the single-number property is lost. Any change to the
  buffer must re-run `scenario_minion_lagcomp`.
- **Puppet cost on clients** may dominate before the server does; the puppet LOD and animation sharing are planned, not free.
- **Sync-state and pipeline refactors** (`HitTargets` 16 entries, `FGSTN_HitVictimView`, `FGSTN_TimelinePlayer`) touch M3/M4
  code; land them as separate commits with the M3/M4 scenarios as the regression gate.
- **Wire id stability**: the registry index and `MoveId` are protocol; changing definitions between server and client builds
  desyncs presentation (add a registry hash check at connect).
- **Dormant packs and late join**: a missed wake can leave a client without a minion; the scenario covers it, and the pack
  flushes dormancy on any relevance change.
