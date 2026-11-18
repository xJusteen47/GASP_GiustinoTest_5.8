# M10 implementation plan: held and thrown objects (plugin `GSTNHeld`)

Plan only, no code. Checkpoints are named **G0..G13**. One checkpoint = one commit (a large one may be two, same gate). The spec is
[`../combat-m10-held-objects.md`](../combat-m10-held-objects.md) (section numbers below are its local ones, "M10 §x"); it builds on M4 hits,
M6 defence and links, and the M7 follow-ups **as shipped** on `feature/gas-npp-5.8` (audited at 60da6fa), not as specified: where the two
differ, the shipped names win and the spec is corrected in `docs/notes/m10-doc-deltas.md` (created in G0).

Status: **draft for the owner** (2026-11-03); owner answered Q4, Q5, Q7, Q9 at 15:51 and added the deferred single-use/throwable weapon idea. Section 6 has the open questions (Italian); section 5 the defaults applied when no answer
comes. Every default is data (definition, kit or rules fields), except the three marked *structural* in section 6.

Sources (short ids used below):

| Id | File | Used for |
|---|---|---|
| M10 | `docs/architecture/combat-m10-held-objects.md` | The spec of this milestone (owner revision of 2026-10-22 wins over its first draft) |
| DD | `docs/architecture/game-design-direction.md` §3, §4, §6b, §6l | Owner direction: enemies thrown at enemies, interact tap = grab / hold = pick up, surface impacts by designer, listen server, objects follow gravity fields (GRV.4) |
| OQ | `docs/architecture/open-questions-m4-m10.md`, "Domande M10" | Owner answers M10.1, .2, .4, .9, .11, .17 (partial); M10.3, .5-.8, .10, .12-.16, .18-.22 open |
| M6 | `docs/architecture/combat-m6-defence-links.md` §7.0, §8; `Plugins/GSTNLink/README.md` | Link registry, channels, throw of a grabbed enemy, `Interact.Target.*` |
| M7P | `docs/architecture/plans/m7-piggyback-plan.md` (branch `claude/project-thread-wzb12r`), `docs/notes/m7-doc-deltas.md` | Format model, ground rules, the generic `GSTNLink` follow-ups M10 reuses (link effects, interact offers, partner credit) |
| IN | `docs/architecture/interaction.md`, `Plugins/GSTNInteract/README.md` | One focus, one prompt; the I2 simulation slice is **not built** |
| GRV | `docs/architecture/gravity-volumes.md` | Throwables follow fields in simulation; `GSTNGravity` is **not built** |
| MT | `docs/architecture/modularity-and-tags.md` | Layering §3, Blueprint §3.8, replaceable content §3.9, tags §4 |

Owner direction folded in:

- **Thrown enemies are first-class** (DD §3, OQ M10.1/.17): a grabbed enemy thrown at other enemies is a projectile with hitboxes, credit
  to the thrower. They get their own checkpoint (G9) right after the first prop throw, not "phase 2".
- **One interact button** (DD §3): tap = grab (M6), **hold = pick up** (`Command.Link.Pickup`, `Interact.Target.Pickup`). Never a
  competing binding.
- **Surface impacts by designer data** (DD §4, OQ M10.9): a thrown enemy takes M4's surface-impact outcome (`EGSTN_ImpactOutcome`, shipped
  in `GSTN_CombatVictim.h`); a prop keeps its own `ImpactBehaviour`.
- **Objects follow gravity in the simulation** (GRV.4): the ballistic segment carries a gravity vector; a gravity change cuts a segment.
- **Listen server is the reference topology** (DD §6b): host as thrower and as target in every scenario.
- **M9 later** (owner, 2026-11-03 15:34): no stock-GAS adapter module, no `IGSTN_FrameSource`; NP only.
- Conventions: Epic coding standard, native tags owned by the plugin, plugin usable alone (no `/Game/`, no project tags/ini), Blueprint
  extension classes with pure `BlueprintNativeEvent` hooks, soft references to content, a human-readable README.

## 0. Ground rules for every checkpoint

Same as M7P §0, with the plugin configurations changed:

- **Build gate** before each commit: full UBT build (not Live Coding for header/reflection changes) in **three configurations**: all on;
  `GSTNHeld` off (test counts equal to the pre-M10 baseline measured in G0, plus whatever G1-G3 add to `GSTNLink`/`GSTNCombat`); `GSTNLink`
  off (forces `GSTNHeld` off). `Automation RunTests GSTN` green; every count from a named `-abslog` log, never estimated.
- **Headless gate**: harness suites `SUITE_DEFAULT`, `SUITE_LINK`, `SUITE_DEFENCE`, `SUITE_PARTS`, `SUITE_MOUNT`, `SUITE_SPIN`/`SUITE_DRILL` stay green with `GSTNHeld`
  on and off; from G5 on, `SUITE_HELD` (this plan's scenarios) green. Each at 60 ms / 15 ms jitter, **0 % and 2 % loss**; listen server
  (host + 2 clients) primary, dedicated + 2 clients secondary; forced reconciles where the checkpoint names them.
- **Scenario gate**: the scenarios named in the checkpoint clean as M4 §10 defines it (snapshots equal; no reconciles besides forced ones and
  the expected one per hit on the victim's client).
- Simulation code has **no `IsServer()`** and no topology branch. The object subsystem is server-authoritative **by design** (M10 §9 C), so
  it is not simulation code: it runs where authority is, and the host's throw uses the same path with zero latency (M10 §13b).
- A 3-line written diagnosis before every build; at most 2 fix attempts, then stop and write what is needed (project rules).
- **No edit of shared test Blueprints** (`KIT_Test_*` of M3-M7): M10 adds its own `KIT_Test_Held*` kits and objects.
- The main checkout on the desktop is never touched (owner's open editor); work in a worktree or on the laptop clone.
- Grep gate (`Tools/` script, extended in G4): no include or module reference to `GSTNHeld*` from `GSTNCombat*`, `GSTNLink*`,
  `GSTNDefence*`, `GSTNPiggyback*`, `GSTNInput*`, `GSTNInteract*`, `GSTNParts*`, `GSTNSpinDrill*`; no `/Game/` and no `ConstructorHelpers` in
  `Plugins/GSTNHeld/Source`.

## 1. What is already there (audit of `feature/gas-npp-5.8` at 60da6fa)

Audited capabilities (**Z1..Z14**). G0 S0 re-checks them on the HEAD the work starts from.

| Id | Capability M10 needs | M10 § | State on feature | Used by |
|---|---|---|---|---|
| Z1 | `FGSTN_EntityRef {Kind, Id, Serial}` (32 bits) and `GSTNEntityKinds::Object = 2` reserved for `GSTNHeld` | 2(1) | **Present** (`GSTNCombatCore/Sync`) | G1, G4 |
| Z2 | Two link slots per entity (`FGSTN_LinkStates { Body, Hand }`, `EGSTN_LinkChannel::Hand`, `ForbiddenWithBody`) | 2(2), 12 | **Partial**: the Hand slot is serialized and compared, but no type has ever lived on it; the link snapshot extension (32 B, full) carries no Hand serial or partner (M7 G1 note) | G1 |
| Z3 | Link type as data + registry from another plugin (`UGSTN_LinkTypeDefinition`, `UGSTN_LinkRegistry::AddDefinition`, plugin break reasons) | 2(4), 7 | **Present**, proven by `GSTNExtensionTest`, `GSTNPiggyback`, `GSTNSpinDrill` | G5 |
| Z4 | A link whose partner is **not a fighter** (the request is a hit on a victim inbox; the `LinkRequest` step runs on the target's tick; watchdog distance/death read the partner's snapshot entry) | 7, 8 | **Missing**: every link today joins two NP fighters | G1 |
| Z5 | Link effects per role (`FGSTN_LinkEffectDef`, applied/removed by the link tick, magnitude = `Payload` / 255) | 7 (`GE_HoldLoad`) | **Present** (M7 G1) | G6 |
| Z6 | Interact offers per link type, in-simulation probe projecting one `Interact.Target.*` (`FGSTN_LinkTickExtension::ResolveInteractTarget`) | 8 | **Partial**: candidates are **snapshot entities only**; objects at rest are not entities. `GSTNInteract` has the stamped record registry (`FGSTN_InteractRegistry`) but its simulation slice (I2, `GSTNInteractCombat`, `TargetKey`) is **not built** | G2 |
| Z7 | `EGSTN_HitboxSpace::Projectile` | 10 | **Present as an enum value**; the attacker step returns false for it ("owned by the projectile system") | G3, G7 |
| Z8 | Posting a hit to a victim's inbox from outside the attacker step (`UGSTN_CombatSimComponent::PostHit`, used by the M6 throw) | 10 | **Present** | G7, G9 |
| Z9 | Throw of a grabbed enemy with `ThrowHit.ExtensionData` passed through untouched and a warning when the thrown-body struct's plugin is off (`FGSTN_LinkThrow`) | 6b | **Present**: the reserved slot of `FGSTN_ThrownBodyExtension` | G9 |
| Z10 | Outcome-extension seam (`IGSTN_OutcomeExtensionApplier`, `Ctx.OutcomeExtensions`) | 6b | **Present** (used by Parts, Defence, Link) | G9 |
| Z11 | Surface-impact outcome (`EGSTN_ImpactOutcome` in the victim step) | 6b | **Present** (M4 §7b) | G9 |
| Z12 | Credit to a second entity (M7 G3: `Combat.Event.Hit.CreditPartner` copy of `HitLanded`/`KO`) | 10 | **Present**, but for a **live** partner; a thrower who left or died is not resolvable | G3 |
| Z13 | `Combat.HitType.Projectile` tag | 5 | **Present** (`GSTNCombatCore`, "data only until projectiles exist"); `Combat.Source.*` holds only Command/Forced/Adopted | G3 |
| Z14 | Weight, `MaxCarryWeight` attributes; hold/tap input conditions (`GSTN_InputConditions.h`) | 6, 8 | **Present** | G5, G6 |

Not built and **out of this plan**: `GSTNMinion` (M8), `GSTNGravity`, the stock-GAS adapter (M9), `GSTNInteractCombat` (I2).

Consequence: two small **generic** follow-ups in `GSTNLink` (G1 non-fighter partner on the Hand channel, G2 record candidates in the probe)
and one neutral follow-up in `GSTNCombat` (G3 projectile source and detached credit) come first, in the M6/M7 follow-up style (entry in the
deltas file, no "held" name in them, tests in the owning plugin). Everything else lives in `GSTNHeld`.

## 2. Plugin layout (M10 §4, adjusted to the shipped pattern)

`GSTNPiggyback` showed one Runtime module holding both the GAS and the NP side works well while only the NP adapter exists. M10 does the same
and keeps the split possible for M9.

| Module | Type | Contents | Depends on |
|---|---|---|---|
| `GSTNHeldCore` | Runtime | native tags header, `FGSTN_BallisticSegment`, `UGSTN_ProjectileMath` (`PositionAt`, closed form, doubles), `FGSTN_ProjectileStepper`, `FGSTN_ThrownDamageProfile`, `FGSTN_HoldRules`, aim math, `ThrowSerial`, `IGSTN_WorldQuery`, `FGSTN_ThrownBodyExtension`, `FGSTN_ThrownOutcome` | `GSTNCombatCore`, `GSTNLinkCore` |
| `GSTNHeldRuntime` | Runtime | `UGSTN_ObjectDefinition`, `UGSTN_ObjectComponent`, `UGSTN_ObjectSubsystem` (server stepper, pool, dormancy), the `Held` link type asset + `UGSTN_LinkRule_Held`, abilities (`UGSTN_GA_Pickup`, `_Drop`, `_Throw`), `FGSTN_ProjectileTrack`, `UGSTN_GE_HoldLoad`, thrown-outcome applier, ghost projectile, impact behaviours (`UGSTN_ProjectileImpactBehaviour`) | Core, `GSTNLinkGAS`, `GSTNLinkNP`, `GSTNCombatGAS`, `GSTNCombatNP`, `GSTNInteractCore`, `GSTNInputCore` |
| `GSTNHeldEditor` | Editor | definition validators, arc preview in the timeline editor | Runtime, `GSTNCombatEditor` (optional) |
| `GSTNHeldTests` | Developer | Automation tests (`FGSTN_FakeLinkHost`), harness abilities and content | all of the above |

`GSTNHeld.uplugin` requires `GSTNCombat` and `GSTNLink`; nothing requires `GSTNHeld`. Native tags are defined in a Runtime module (EXT lesson).
Object type tags `Held.Type.*` and commands `Command.Link.Pickup/.Drop` are ini in `Plugins/GSTNHeld/Config/Tags/`.

## 3. Checkpoints (in order)

Cost column: rough, from the 2026-11-03 consumption review (about $6-8 per average thread). **S** ≈ $3-6, **M** ≈ $6-12, **L** ≈ $12-20,
Opus medium for design/hard code, Sonnet for data, tests and harness. Total M10 core (G0-G9, G12, G13) ≈ **$95-160**; optional G10-G11 ≈ $20-35.

| Gate | Content | Size | Cost | Machine |
|---|---|---|---|---|
| G0 | Seam audit + spikes | M | $6-12 | laptop |
| G1 | `GSTNLink`: non-fighter partner on the Hand channel | M | $8-12 | laptop |
| G2 | `GSTNLink`: record candidates in the interact probe | S | $4-6 | laptop |
| G3 | `GSTNCombat`: projectile source and detached credit | S | $3-6 | laptop |
| G4 | `GSTNHeld` skeleton + Core math | M | $6-10 | laptop or cloud |
| G5 | Objects, pickup, drop, break rules | L | $12-20 | desktop or laptop |
| G6 | Carry load and size bindings | S | $4-6 | same |
| G7 | Throw, projectile, impact, hits | L | $14-20 | same |
| G8 | Ghost, handover, listen host | M | $8-12 | same |
| G9 | Thrown enemies (fighters) | L | $12-20 | same |
| G10 | *Optional* held weapons (M10b) | L | $12-20 | - |
| G11 | *Optional* catch and deflect (M10c) | M | $8-15 | - |
| G12 | Presentation, debug, editor preview | M | $6-10 | desktop |
| G13 | Full sweep, byte table, perf, docs | M | $8-12 | desktop |

### G0 Seam audit and spikes (scratch code only; results in `docs/notes/m10-spike-report.md`)

- **S0 seam audit**: re-check Z1-Z14 on the starting HEAD (SD-14 and M7 G9 may have moved `GSTNLink`); record the pre-M10 baseline test
  counts (on / `GSTNLink` off) from named logs. A row worse than written is a stop condition (section 4).
- **S1 throw feel** (M10 §3.1): a scratch cosmetic sphere driven by `PositionAt` on the thrower at the release frame, a server-spawned
  replicated actor with the same segment; position error at handover for a lob and a flat throw, 60/15 ms, 0 and 2 %; host as thrower and
  client as thrower. Target ≤ 25 cm (hidden by the 6-frame blend).
- **S2 hold attach** (M10 §3.2): a static mesh attached to the hand socket of an **interpolated** proxy vs root + baked offset; jitter and
  pops under forced reconciles of the holder.
- **S3 Hand-slot bytes** (M10 §3.4): cost of a live Hand link in the sync state and `ShouldReconcile` noise when the partner object is
  created/destroyed (the object is not an NP entity).
- Pickup race (§3.5) and catch (§3.3) need G5/G11 code: measured there.
- Gate: build all on only (scratch is not committed); spike report committed.

### G1 `GSTNLink` follow-up 3 (generic): non-fighter partner on the Hand channel

- A **partner resolver per entity kind** (`IGSTN_LinkPartnerResolver`, registered in `UGSTN_LinkRegistry` by `GSTNEntityKinds` id): gives the
  watchdog the partner's position, alive/serial validity and team for a partner that has no snapshot entry. Fighter kind = today's snapshot
  path, unchanged.
- **Server-formed one-sided link**: a link type may declare `bPartnerIsExternal`; its request is not a hit on a victim inbox but a
  `LinkRequest` the resolver's owner (a server subsystem) answers by posting a `LinkResponse` entry to the initiator's inbox (kind exists,
  Z3). The initiator's predicted `Pending` uses the M7 G1 optimistic accept unchanged.
- **Hand channel live**: link tick, watchdog, role-tag projection and break rules run for the Hand slot; `ForbiddenWithBody` enforced in
  both directions (grabbed while holding = Hand breaks with `DropOnGrab`, M10 §12). The snapshot stays 32 B: observers learn a held object
  from the object's own replicated state, not from the snapshot (S3 decides; if a bit is needed, it replaces a reserved bit, never grows).
- Tests (`GSTNLink.NP.Hand.*`, `GSTNLink.NP.External.*`): resolver distance/death breaks, response forms the link, refusal rolls back,
  `ForbiddenWithBody` both ways, Body links byte-identical to the pre-G1 sequences.
- Gate: three build configs; `SUITE_LINK`, `SUITE_MOUNT` and the drill scenarios unchanged at 0/2 %.
- Coordination: tell the spin-drill and M7 threads before and after (files `GSTN_LinkTypeDefinition.h`, `GSTN_LinkTickExtension.*`,
  `GSTN_LinkRegistry.*`, watchdog).

### G2 `GSTNLink` follow-up 4 (generic): record candidates in the interact probe

- The in-simulation probe (`ResolveInteractTarget`) accepts candidates from **stamped records** of `GSTNInteract`'s `FGSTN_InteractRegistry`
  (objects at rest have no snapshot entry) beside snapshot entities. A record carries meaning tag, priority, handle (`FGSTN_EntityRef`).
  Still **exactly one** projected `Interact.Target.*` and one focus id: the pickup ability reads that id (IN §3.3), the hitbox only
  validates contact.
- This is the interim path the owner chooses in Q3 (section 6); the full I2 slice (`GSTNInteractCombat`, sticky focus in the sim) replaces
  it later with the same record data.
- Tests: `GSTNLink.NP.Interact.{RecordCandidate, GrabStillWinsOnTie, OneTagProjected, StaleRecordIgnored}`.
- Gate: three build configs; `SUITE_LINK` unchanged.

### G3 `GSTNCombat` neutral follow-up: projectile source and detached credit

- Native tags `Combat.Source.Thrown`, `Combat.Source.Projectile` (`GSTNCombatCore`, beside `Combat.HitType.Projectile`).
- A hit inbox entry may carry a **detached instigator** (`FGSTN_EntityRef` + team snapshot) used for credit when the attacker entity is
  gone or is not a fighter (a thrown object; a thrower who disconnected): `HitLanded`/`KO` name it, the M7 G3 `CreditPartner` copy is
  reused for the assist (`ChainDepth`). Without it the events are exactly the pre-G3 ones (test).
- `EGSTN_HitboxSpace::Projectile` stays not evaluated by the attacker step (documented: projectile hitboxes are swept by their owner).
- Tests: `GSTNCombat.Core.Credit.{DetachedInstigator, NoCreditUnchanged}`.
- Gate: three build configs; `SUITE_DEFAULT` unchanged.

### G4 `GSTNHeld` skeleton + `GSTNHeldCore` (pure)

- Plugin, four modules, README, tags (M10 §5, minus catch/weapon tags until G10/G11), ini families, grep gate rows.
- `FGSTN_BallisticSegment { StartFrame, Start, Velocity, GravityAccel (vector), Drag, AngularSpin }`, `PositionAt` closed form,
  `FGSTN_ProjectileStepper` (sweep step, bounce = new segment with restitution, **gravity change = new segment**), `FGSTN_ThrownDamageProfile`,
  aim math (stick, camera, pitch clamp), `ThrowSerial = hash(thrower ref, timeline start frame, key index)`.
- Gravity input: the thrower's simulation gravity at the release frame (Up = -GravityDir, owner rule §2d); an optional
  `IGSTN_GravitySampler` hook lets `GSTNGravity` cut segments later; without it gravity is constant per throw.
- Tests (`GSTNHeld.Core.*`): `PositionAt` vs stepping (1e5 random segments, ≤ 0.001 cm), repeatability, bounce speed × restitution,
  gravity cut continuity, aim math, damage curve, `ThrowSerial` stability.
- Gate: three build configs (`GSTNHeld` off = baseline + G1-G3); grep gate green.

### G5 Objects, pickup, drop, break rules (`GSTNHeldRuntime` part 1) + pickup race

- `UGSTN_ObjectDefinition` (M10 §6, without `HeldWeapon` until G10), `UGSTN_ObjectComponent` (replicated state: `Idle/Held/Flying/Landed/
  Broken`, holder ref, link serial, charges; dormant at rest), `UGSTN_ObjectSubsystem` (object table = the `Object` kind's resolver of G1,
  record registration of G2, pool).
- `Held` link type asset (channel Hand, no parked role, break rules: hit on holder → drop **per object** (owner, Q4: "depends on damage,
  knockback and the specific object"): `FGSTN_DropOnHitRule` on the object definition with `MinDamage`, `MinKnockback` and an optional
  hit-type/tag filter, evaluated by a small link hit step after `ComputeKnockback`; KO, grabbed, distance safety, object destroyed;
  `DropVelocity`), `UGSTN_LinkRule_Held` (state, kind allow list, `MaxCarryWeight`, free hand, re-pickup lock).
- `UGSTN_GA_Pickup` (`Command.Link.Pickup` = Hold(Interact, `HoldFrames` 15), requirements `Interact.Target.Pickup` and not carrying; target
  = focus id), predicted attach, refusal pops back. **Race**: earliest request frame, then lower entity id; loser fumble cue.
  `UGSTN_GA_Drop` (`Command.Link.Drop`), placement socket.
- Tests (`GSTNHeld.Runtime.*` with `FGSTN_FakeLinkHost`): lifecycle pickup → hold → drop → pickup, break rules table (drop-on-hit below and
  above each threshold, per object), race determinism,
  events once.
- Harness: `scenario_pickup_drop`, `scenario_pickup_race`, `scenario_drop_on_hit`, `scenario_held_grabbed` (matrix rows of M10 §12 that
  exist today: holder grabbed, holder grabs with Light/Medium, rider picks up, carrier refused), `scenario_object_dormancy`.
- Gate: three build configs; `SUITE_HELD` (these five) + all suites at 0/2 %, listen and dedicated.

### G6 Carry load and size bindings

- `UGSTN_GE_HoldLoad` + MMC (speed/jump scaled by `Weight` / `MaxCarryWeight`), applied by the M7 G1 link effect on the holder (no new
  executor); `Combat.Held.Carrying.<size>` projected; `FGSTN_HoldRules` per size (Light all, Medium no shield/air attacks, Heavy walk/drop/
  throw only, Huge needs `MaxCarryWeight`); validator lists missing size bindings per kit.
- Tests: MMC goldens, removed on every break reason, rules table.
- Harness: `scenario_carry_load` (run and jump with a Medium object: no correction after the link phase).
- Gate: as G5.

### G7 Throw, projectile, impact, hits (`GSTNHeldRuntime` part 2)

- `FGSTN_ProjectileTrack` (registered track type; editor draws it generically), `ReleaseLink` event, `UGSTN_GA_Throw`
  (`Combat.State.Throwing`, any attack command with `Combat.Link.Role.Holder` + size tag; fumble if interrupted before release).
- **Aim** (owner, Q5: "if we are aiming it follows the aim"): `AimPolicy = StickOrAimWhenAiming` (default) uses the simulated
  `AimYaw/AimPitch` of the current command (combat.md §11.6, converged shoulder aim of camera.md §6) while the holder is aiming
  (`Camera.Request.Aim` granted by the aim ability), else the stick direction. Origin = the character's baked hand socket, never the
  camera. The test kit sets `bSendCameraAim` (camera.md §6 validator).
- **Friendly fire** (owner, Q7): thrown objects hit player allies **by default**; the game mode's rules can turn it off (relationship
  variant of combat.md §12.3, data on `UGSTN_CombatRules`).
- Server: projectile spawned from the replicated release; `UGSTN_ObjectSubsystem` steps it after the fighters' frame, sweeps world and
  hurtboxes (step snapshot), posts hits with `PostHit` (Z8) stamped F, drained at F+1; per-throw hit ring, `MaxHits`; credit = detached
  instigator (G3); impact behaviours `Break/Bounce/Stick/Land` (`Explode` = a GE, data); `MaxActiveProjectiles` 32, pooled; events only on
  the wire (~36 B throw, ~16 B impact).
- Tests: throw lifecycle on the fake host, impact behaviours, credit after thrower leaves, pool cap.
- Harness: `scenario_pickup_throw_hit` (c1 throws at c2: state, damage, knockback, credit equal; one rollback on c2), `scenario_throw_aimed`
  (shoulder aim at a target off the stick direction: projectile follows the aim, equal on server and client), friendly fire on/off by mode,
  `scenario_throw_forced_reconcile` (forced rollback over the release frame: exactly one projectile, one event), `scenario_throw_bounce`.
- Gate: as G5, plus no extra reconcile on the thrower.

### G8 Ghost, handover, listen host

- Ghost projectile on the thrower's machine at the predicted release, matched by `ThrowSerial`, error blended over 6 frames, fades after
  `GhostTimeoutMs` if never confirmed; ghosts never hit. Host throws: authoritative spawn on the release frame, no ghost.
- Harness: `scenario_throw_ghost` (handover ≤ 25 cm, no duplicate actor), `scenario_throw_host` (host as thrower and as target).
- Gate: as G5; S1 numbers confirmed on real code.

### G9 Thrown enemies: fighters (M10 §6b, owner headline)

- `FGSTN_ThrownBodyExtension` in the M6 throw key's `ThrowHit.ExtensionData` (Z9; no edit of `GSTNLink`), `FGSTN_ThrownOutcome` written by
  the `Throw` resolution, applied by this plugin's `IGSTN_OutcomeExtensionApplier` (Z10): `Combat.State.Thrown` for the flight, a
  `ThrownBody` hitbox generated by the thrown entity's own tick extension (`Space = Root`), credit to the thrower, `Combat.Source.ThrownEnemy`;
  the thrown fighter ignores its own team while `ThrownBody` is active; not hittable by attacks while held or flying (owner, Q9); surface
  impact = M4's `EGSTN_ImpactOutcome` (Z11), not `ImpactBehaviour`, and **the thrown body takes damage from that impact** (owner, Q9: splat on
  the ground or bounce off a wall), the same surface-impact damage a launched enemy takes, data on the hit/entity/surface rules; damage weight from `UGSTN_PartMath::EffectiveWeight` when `GSTNParts` is on.
- The grab gate is untouched (`Combat.Part.GrabLocked`, `Combat.Immunity.Grab`).
- Tests: applier goldens, team ignore, effective weight, extension ignored with a warning when `GSTNHeld` is off (already in `GSTNLink`).
- Harness: `scenario_throw_enemy` (c1 grabs a part-broken test enemy and throws it at two others: hits with credit to c1, wall impact per
  data with self-damage on splat and on bounce, no damage from attacks in flight; same with host as thrower). The **minion** half waits for M8.
- Gate: as G5 + `SUITE_PARTS` with `GSTNHeld` on and off.

### G10 *Optional* held weapons (M10b; only if the owner answers Q2 "yes")

- `HeldWeapon` on the definition, `Combat.Weapon.Role.Held`, overlay layer **precompiled** into the loadout table per `AllowedHeldWeapons`
  (no recompile in simulation, `LoadoutRevision` unchanged), `HeldCharges`, break or turn into a Light prop at 0.
- Harness: `scenario_held_weapon` (swing 20 times, breaks; loadout table unchanged).

### G11 *Optional* catch and deflect (M10c; only if the owner answers Q2 "yes")

- `Combat.State.CatchWindow` from the timeline `Tags` track, pipeline step `Catch` before damage, judged with `PositionAt` at the catcher's
  rewound view time; deflect step `ProjectileInteraction`; re-ownership, `ChainDepth`, assists.
- Harness: `scenario_catch`, `scenario_deflect`.

### Deferred (owner hypothesis, 2026-11-03 15:51): throwable support weapons and single-use weapons

Not planned as a gate yet; recorded so G10 and the loadout work (M3 loadout, [combat-loadout-mods.md](../combat-loadout-mods.md)) leave room.

- **Throwable support weapons**, possibly through an ability: the support weapon becomes a projectile (`FGSTN_ProjectileTrack` with
  `bUseHeldObject` reading the weapon's object definition), so it reuses G7 entirely; what is new is the loadout side (the slot empties,
  and how the weapon is recovered).
- **Single-use weapons** (example: a special sword summoned as an object that vanishes after one move): it appears in the hand and
  occupies the **support-weapon slot** as usual. If a support weapon is already equipped, the single-use one is held anyway. **Switching**
  to the kit's main weapon or to the support weapon **drops** the single-use one. Likely shape: a held weapon (G10) with `HeldCharges` = 1
  and `bVanishOnEmpty`, summoned by an ability instead of picked up, and a loadout rule "weapon switch ends the Held link".
- **Stowing in the support slot** (owner, 15:53): some objects can be put away in the support-weapon slot (e.g. on the back) instead of
  held, **only if that slot is empty**. Likely shape: `bStowable` on the object definition; a stow ability ends the `Held` link and writes
  the object's ref into the support slot (presentation on a back socket); taking it out recreates the `Held` link; refused when a support
  weapon is already there.
- **With a support weapon already equipped** (owner, 15:54): the held object cannot be stowed and cannot be swapped with the support or
  the main weapon; **any weapon switch drops it on the ground** (the `Held` link ends with a drop, as on a hit).
- Needs G10 first and a short loadout note (slot ownership while a summoned weapon is held); ask the owner before it becomes a gate.

### G12 Presentation, debug, editor

- Hand-socket attach from S2's verdict, release blend; `gstn.Held.Debug 1` (volumes, segments by owner, `PositionAt` markers ±10 frames,
  ghost vs server); arc preview in `GSTNHeldEditor`; definition validators. Interact prompt: Pickup shows only the hold verb and hold
  progress (IN §5), `HoldFrames` validator against the binding.
- Gate: build all three configs; one composite image for Claude's review, the owner's in-game video for his own ok.

### G13 Full sweep, byte table, perf, docs

- Every scenario in listen (host as thrower and target) and dedicated, 0 and 2 %, all suites in the three plugin configurations; byte table
  (idle object 0 B/s, flying ~28 B per update); **32 active projectiles** ms/frame on the listen host (with rendering) and on a dedicated
  server; docs: combat.md (§13b → pointer, §18 M10 row), README "How to extend", testing.md, `m10-doc-deltas.md`, M10 §16 checklist.

## 4. Stop and ask

- S0 shows a Z row worse than written, or G1 would need a fighter-only assumption removed from `GSTNLink` beyond the resolver seam.
- S1 handover error above 25 cm at 2 % loss for every pairing (the ghost design is in question).
- Anything that would make `GSTNLink`, `GSTNCombat` or `GSTNInteract` name `GSTNHeld`, or change grab/latch/mount/drill behaviour in their suites.
- The link snapshot needs to grow beyond 32 B (S3).
- An edit to a shared Blueprint, the owner's level, the skeleton (sockets), or the project camera.

## 5. Defaults for unanswered owner questions (applied unless section 6 is answered otherwise)

| OQ | Default (all data unless marked) |
|---|---|
| M10.2 | Four size classes as tags; test content uses Light and Medium only |
| M10.3 | Held weapons not built in this plan (G10 optional) |
| M10.5 | Pickup race: earliest request, then lower id; loser fumble |
| M10.6 | **Answered (Q4)**: drop depends on damage, knockback and the object (`FGSTN_DropOnHitRule` per definition); test objects: Light drops on any hitstun, Medium above a knockback threshold |
| M10.7 | **Answered (Q5)**: follows the aim while aiming (camera.md §6), else the stick; no charge in the test kit |
| M10.8 | Damage = curve on impact speed × size multiplier, curve flat by default (reads as "fixed per type") |
| M10.10 | One hit per target per throw; `MaxHits` 1 (no pierce) |
| M10.11 (rest) | **Answered (Q7)**: thrown objects hit player allies by default; the mode can turn it off |
| M10.12 | Credit to the thrower, snapshot at release (survives death and disconnect) |
| M10.13-14 | Catch and deflect not built (G11 optional) |
| M10.15-16 | Rider may pick up and throw (Hand channel); carrier refused; grabbed holder drops; no grab while holding Medium+ |
| M10.18 | 32 active projectiles; objects at rest dormant; respawn per definition |
| M10.19 | Separate optional plugin (this plan) |
| M10.20 | Fighters in G9; minions after M8 |
| M10.21 | Thrown enemy uses M4 surface rules; props use `ImpactBehaviour` |
| M10.22 | **Answered (Q9)**: not hittable by attacks while held/flying, but damaged by its own surface impact (splat, wall bounce); hits any enemy of the thrower, its own team ignored |

## 6. Domande per il proprietario (max 10, in italiano)

Le prime tre sono strutturali (cambiano l'ordine o il perimetro); le altre sono dati e hanno già un default (sezione 5).

1. **Ordine.** Faccio prima gli oggetti di scena (G5-G8: raccogliere, portare, lanciare una cassa) e poi i nemici lanciati (G9), oppure
   vuoi i nemici lanciati subito dopo il primo lancio di una cassa (G7 → G9 → G8)? *Consiglio:* G7 → G9 → G8: i nemici lanciati sono la tua
   priorità e usano lo stesso danno e credito di G7; l'effetto "fantasma" del lancio (G8) è solo sensazione.
2. **Perimetro.** Per ora solo casse/oggetti di scena e nemici lanciati, rimandando armi raccolte (G10) e presa al volo/deviazione (G11)?
   *Consiglio:* sì, risparmia ~$20-35 e due checkpoint.
3. **Raccogliere con il tasto interazione.** Il sistema "un solo focus, un solo prompt" ha ancora solo il nucleo; la parte dentro la
   simulazione (I2) non c'è. Va bene un passo intermedio piccolo (G2: gli oggetti a terra entrano nel probe della presa che già esiste),
   oppure vuoi costruire I2 completo prima di M10 (circa $15-25 in più)? *Consiglio:* passo intermedio; I2 lo sostituisce poi con gli stessi dati.
4. Se chi porta un oggetto viene colpito, l'oggetto cade **sempre** (qualsiasi hitstun) o solo sopra una soglia di colpo? *Default:* sempre.
   **Risposta (03/11, 15:51):** dipende dal danno, dal knockback e dall'oggetto specifico. → soglie per oggetto (G5).
5. Il lancio va nella direzione dello stick, oppure vuoi anche lancio caricato (tenendo premuto) o mirato con la camera? *Default:* stick.
   **Risposta (03/11, 15:51):** se stiamo mirando segue la mira (già pianificata, camera.md §6, combat.md §11.6). → G7.
6. Il danno di un oggetto lanciato: fisso per tipo di oggetto, o cresce con la velocità e la taglia? *Default:* curva piatta = fisso per tipo,
   modificabile per oggetto.
7. Gli oggetti lanciati possono colpire **altri giocatori alleati** (fuoco amico)? *Default:* no, attivabile per modalità.
   **Risposta (03/11, 15:51):** sì di default, ma dipende dalla modalità. → G7.
8. Chi cavalca (M7) può raccogliere e lanciare oggetti? E chi porta un rider? *Default:* il rider sì, il carrier no.
9. Un nemico lanciato può essere colpito mentre è in mano o in volo? *Default:* no; colpisce tutti i nemici del lanciatore, anche i suoi alleati.
   **Risposta (03/11, 15:51):** no, ma può farsi danno quando impatta il suolo (splat) o rimbalza su una parete. → G9.
10. Quanti oggetti in volo insieme al massimo, e gli oggetti a terra spariscono o ricompaiono dopo un po'? *Default:* 32 in volo; a terra
    restano finché il pool non li ricicla; ricomparsa decisa per oggetto.

## 7. Interactions with other systems (noted, not planned here)

- **INT (`GSTNInteract`, one focus, one prompt):** Pickup is the meaning `Interact.Target.Pickup` with only a **hold** verb (a tap on an object
  does nothing in this version, M10 §8); it never competes with grab or mount: the probe projects one tag and the pickup ability reads the
  focus id. Objects at rest are **stamped records** (IN §3.2) with `PickupPriority` as priority and the object ref as handle; the prompt shows
  name and icon from `UGSTN_ObjectDefinition` through `IGSTN_InteractTarget`. When I2 lands, G2's interim path is removed and the records stay.
  Interact is blocked while carrying (M10 §8).
- **M8 minions (enemies as projectiles):** M10 keeps the projectile state keyed by `FGSTN_EntityRef`, so a minion store slot can fly as an
  `FGSTN_ProjectileState` with `Kind = Minion` and return to the store on landing (M10 §6b). Nothing minion-specific is built in M10; M8 adds
  its kind's resolver (G1 seam), the grab of a minion and the hand-off to the object subsystem. `scenario_throw_enemy`'s minion half is an M8
  gate.
- **M7 piggyback:** rider on the Hand channel may hold and throw (from the carrier's mount socket); carrier refused unless the kit allows Light.
  `GE_HoldLoad` is separate from `UGSTN_GE_CarryLoad` (no dependency on `GSTNPiggyback`).
- **GSTNGravity (not built):** segments already carry a gravity vector; the sampler hook of G4 is the only seam it needs.
- **M9 (later):** the stock adapter would add a `GSTNHeldStockGAS` module and `IGSTN_FrameSource`; not built now.

## 8. Coordination and machines

- **Spin/drill (SD-14, laptop)** and **M7 G9 (desktop)** both touch or read `GSTNLink`. G1 and G2 edit `GSTNLink` and G3 edits `GSTNCombatCore`:
  they start only when no other open checkpoint edits those files, or rebase onto them; tell those threads before and after. G4-G13 touch
  only `Plugins/GSTNHeld` plus the harness and are parallel-safe.
- **Machine:** one code thread per machine (project rule 7). G0-G4 fit the laptop; G5 onward need the editor for harness and content, on
  whichever machine is free; G12 needs the desktop for the video.
- **Delegation:** one fresh thread per checkpoint (or two small ones), handover ≤ 30 lines in `docs/notes/handovers/m10-held.md`, Opus medium
  for G1, G5, G7, G9; Sonnet for G2, G3, G4 tests, G6, harness scenarios; push to feature at each green gate.

## 9. Risks

- **G1 is the only change to how links form** (a non-fighter partner). A bug shows up in grabs. Mitigation: the type flag gates it; Body
  links byte-identical in `SUITE_LINK`.
- **Hand attach jitter on interpolated holders** (S2): fallback root + baked offset.
- **Analytic motion vs complex geometry:** odd rolls on slopes and stairs; mitigation: `Stick`/`Break` for awkward props, no Chaos.
- **Thrown fighter stays an NP entity:** its self-generated hitbox touches the victim/outcome path; the seam is shipped (Z9, Z10), so the risk
  is behaviour, covered by `SUITE_PARTS`/`SUITE_LINK` on and off.
- **Host advantage on throws** (zero latency for the host) is accepted by DD §6b unless the owner asks for parity.
- **Content explosion** (sizes × kits × directions): bindings keyed by size; validator lists missing ones.
