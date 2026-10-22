# Traversal surfaces — wall-slideable, launching and other surface behaviours on level objects and enemy body parts (specification)

Status: draft for owner review, 2026-10-22 evening. Everything marked *Proposal* is decided here under the owner's
self-approval rule (no new feature beyond the request, no architectural question); the owner questions are in
[open-questions-m4-m10.md](open-questions-m4-m10.md) § "Superfici di attraversamento sulle parti dei nemici (22/10 sera)"
(TS.1–TS.8). The plan is [plans/traversal-surfaces-plan.md](plans/traversal-surfaces-plan.md) (T0..T9).

Owner request (2026-10-22, verbatim): "alcuni nemici potrebbero avere porzioni di corpo su cui è possibile fare wall slide,
oppure parti come trampolini etc..." — some enemies have body portions a player can wall-slide on, or parts that act as
trampolines, and so on.

Sources: [game-design-direction.md](game-design-direction.md) §2d (gravity, trampolines), §2f, §4, §5b (parts);
[wall-movement.md](wall-movement.md); [combat-m4-hits.md](combat-m4-hits.md) §7b and plan H11/H11b;
[combat-parts.md](combat-parts.md) §3.1, §5.1 (`BrokenMask`), plan P7/P7b; [combat-m8-minions.md](combat-m8-minions.md)
§4d.5 and [many-enemies-replication.md](many-enemies-replication.md) §4 (`GSTNPlatform`, pose history, spikes P1–P4);
[modularity-and-tags.md](modularity-and-tags.md) §3.7–3.9, §4, §5b.

## 0. Summary

- A **surface behaviour** is static data attached to a collider: which traversal mechanics it allows (`Surface.Behaviour.*`
  tags), its impact class (`Surface.Type.*`), and the launch parameters when it launches. Level objects (walls, the H11b
  trampoline) and **enemy body parts** carry the same data; nothing in the movement or combat code knows which it is.
- The **only** way an enemy offers a surface is the mechanism M8 already defines for enemies as platforms: a **kinematic,
  unattached collider** moved at one defined point per fixed step from the enemy's end-of-frame state (host) or its
  interpolation sample (clients). A wall-slideable part is a platform shape with a `WallSlide` behaviour; a trampoline part
  is a platform shape with `Launch`. The player's Mover already rides moving walls (`bUseMovementBases`) and moving floors, so
  the movement side is reuse plus a surface filter. **A part breaking disables its shapes** (BrokenMask → shape mask), with
  the platform's grace telegraph; a restore (P7b) re-enables them.
- Simulation code never reads the broken mask, a bone, or an animated mesh: it sweeps against colliders whose pose and
  enabled state are a *projection of confirmed state*, exactly as the platform collider. Rollback safety is therefore the
  platform's (one correction at mount/drop, zero on the host), not a new problem.
- Plugins (*Proposal*): a new tiny `GSTNSurface` plugin (`GSTNSurfaceCore`, no GAS/NP/Mover) owns the `Surface.*` tag root and
  the surface data contract; `GSTNCombatCore` (H11/H11b), `GSTNPlatform` and the wall-movement code depend on it; the bridge
  `GSTNPlatformParts` maps parts to shapes. Platform minions get it through `GSTNPlatformMinion` later.

## 1. Scope

In scope: (a) the surface data model and tag family; (b) wall slide and wall climb on surfaces that allow them, including
moving surfaces (enemies), and surfaces that forbid them; (c) launch ("trampoline") from static and moving surfaces for
non-tumbling characters, and the bounce row for tumbling victims (H11); (d) enemy body parts as surfaces, their activation
by part state, and what happens when the enemy moves, is hit, launched, frozen, dies or is grabbed while a player uses the
surface; (e) networking under the listen-server topology.

Out of scope (reserved hooks only): grabbing/climbing *onto* an enemy as a link (M6/M7 own that); minions wall-sliding on
anything (minions have no Mover wall modes, M8 §6); per-surface friction tuning of the wall slide (`Slippery` is a tag today,
no reader; TS.8); detached chunks as surfaces (presentation only, §6).

Who uses surfaces: **players and fighters** (NP entities with Mover). Minions as *users* of a trampoline follow M8's
lightweight launch for static trampolines only (H11 note); moving trampoline parts never launch minions in this spec.

## 2. Data model (`GSTNSurfaceCore`)

### 2.1 `FGSTN_SurfaceBehaviour` (USTRUCT, BlueprintType)

| Field | Meaning |
|---|---|
| `FGameplayTagContainer Behaviours` | `Surface.Behaviour.*` leaves this surface offers: `WallSlide`, `WallClimb`, `Launch`, `Stand` (walkable platform top, M8 §4d.5) ; `NoWallSlide` is **not** a tag: absence of `WallSlide` is the negative (one way to say it, no contradictions to validate) |
| `FGameplayTag SurfaceType` | `Surface.Type.*` for H11 impact resolution (`Wall`, `Glass`, `Trampoline`, ...). Empty = from the physical material table (§2.3) |
| `FGSTN_SurfaceLaunchParams Launch` | Used when `Behaviours` has `Launch`: the H11b struct, unchanged (direction mode `SurfaceNormal | LocalDirection | Tilt`, speed mode `Fixed | ScaledByIncoming`, `bKeepTangentialVelocity`, `bResetAirJumps`, `bResetCoyote`, `LaunchTag`) **plus** `bAddSurfaceVelocity` (moving surfaces, §4.3) |
| `FGameplayTagContainer DesignerTags` | Free identity for kits/abilities/AI queries (e.g. a boss "back plate"); never read by core code |

Everything here is **static configuration**: read by simulation code as data that is identical on all machines and never changes
at runtime (a validator rejects runtime writes in Blueprint: the component's fields are `EditAnywhere, BlueprintReadOnly`).
What *does* change at runtime is only whether the collider exists for a sweep (enabled, posed), §4.

### 2.2 Where a behaviour lives: the resolution order (`UGSTN_SurfaceUtils::Resolve(const FHitResult&) → FGSTN_SurfaceView`)

A sweep or landing contact yields a hit; the behaviour is resolved, in this order, first match wins:

1. `UGSTN_SurfaceBehaviourComponent` on the **hit component's owner, keyed by component** (one component per surface
   collider; a level actor with three faces that behave differently has three colliders and three components, or one component
   with a `TMap<FName, FGSTN_SurfaceBehaviour> PerComponent` override, *Proposal*: the map, fewer components);
2. `IGSTN_SurfaceProvider` on the hit component (platform shapes implement it) or on the actor (H11b trampoline);
3. the **physical material table** (`UGSTN_SurfaceSettings`, `UDeveloperSettings`, `TMap<TSoftObjectPtr<UPhysicalMaterial>,
   FGSTN_SurfaceBehaviour>`, baked at load into indices as H11 does for `Surface.Type.*`);
4. the default (`UGSTN_SurfaceSettings::Default`: `{WallSlide, WallClimb, Stand}`, `Surface.Type.Wall`): **unknown geometry
   behaves as today**, so enabling the plugin changes nothing until a designer marks a surface.

`FGSTN_SurfaceView` is a const view (`BlueprintReadOnly`): behaviours, type index, launch params, the surface's velocity at the
contact (from the base, §4.3), `bMoving`. It is what every consumer reads; none keeps it across frames.

`IGSTN_SurfaceProvider` (UINTERFACE, `BlueprintNativeEvent GetSurfaceBehaviour(const FGSTN_SurfaceQuery&) const`,
`Break()` for H11 `BreakSurface`): **replaces** H11's `IGSTN_ImpactSurface` and H11b's `IGSTN_LaunchSurface` with one interface
declared in `GSTNSurfaceCore` (both plans edited 2026-10-22; neither has started). `FGSTN_SurfaceQuery` carries the hit component
name and contact point so a provider can answer per face.

### 2.3 Tags (owner `GSTNSurface`, native unless stated)

| Tag | Source | Meaning |
|---|---|---|
| `Surface.Type.*` | **ini** leaves (`Plugins/GSTNSurface/Config/Tags/GSTNSurface.ini`: `Wall Glass Crate Trampoline`), root native | impact class (H11); ownership moves from `GSTNCombat` to `GSTNSurface` before H11 lands (modularity §4.2 row updated when the owner approves TS.1) |
| `Surface.Behaviour.WallSlide`, `.WallClimb`, `.Launch`, `.Stand` | native | what the surface offers; read through `FGameplayTagQuery` fields on consumers (modularity §3.7 rule 4), never hard-coded in a consumer's logic |
| `Surface.Behaviour.Slippery`, `.OneWay` | native, **reserved** | declared with a comment "no reader yet" so content can be authored; readers are later extensions (TS.8) |
| `Surface.Event.Launched` | native | local presentation/cue trigger when a launch is applied (the combat event for tumbling victims stays `Combat.Event.Impact.Bounce`) |

No tag is in any sync state (indices only, as H11's `SurfaceTypeIndex`). `Platform.*` stays in `GSTNPlatform`; `Combat.Part.*` in
`GSTNParts`; `Movement.*` with the wall modes.

### 2.4 `UGSTN_SurfaceBehaviourComponent` (ActorComponent, Blueprint-spawnable)

One `FGSTN_SurfaceBehaviour Default` and the optional `PerComponent` map. `IsDataValid`: named components exist and are
`UPrimitiveComponent`s; `Launch` present ⇒ launch params finite; `Stand` only on components with a walkable face (warning).
For the H11b trampoline: `AGSTN_Trampoline` keeps its `FGSTN_SurfaceLaunchParams Launch` property and implements the provider,
or (simpler, *Proposal*) just carries this component with `{Launch}` and `Surface.Type.Trampoline`; H11b's tests keep their names.

## 3. Plugin placement (*Proposal*; owner question TS.1)

| Plugin / module | Layer | Depends on | Owns |
|---|---|---|---|
| **`GSTNSurface`** (`GSTNSurfaceCore` only) | L2/L3, optional, `EnabledByDefault: false` | Engine, GameplayTags, PhysicsCore | `Surface.*`, §2 types, resolve utils, settings |
| `GSTNCombatCore` (H11/H11b) | L3 | + `GSTNSurfaceCore` (Core → Core, allowed by §3.7 rule 2) | impact resolution reads `FGSTN_SurfaceView`; `GSTNLevelObjects` (H11b) depends on `GSTNSurfaceCore` and `GSTNCombatCore` |
| wall movement (`GSTNMovement`, project today) | L3 | + `GSTNSurfaceCore` (private) | the surface filter extension point (§5.2, owner OK TS.2 given 2026-10-22 22:14) |
| `GSTNPlatform` (`GSTNPlatformCore`, runtime) | L4 optional | + `GSTNSurfaceCore` | shapes with behaviours, per-shape activation, provider on the shape component (§4) |
| **`GSTNPlatformParts`** (one module, bridge) | bridge | `GSTNPlatformCore`, `GSTNPartsCore`, `GSTNPartsNP` (reads the confirmed mask, as `GSTNPartsMinion` does with the store) | `PartName → shapes` mapping, mask projection (§4.4) |
| `GSTNPlatformMinion` (M8 §4d.5) | bridge | unchanged + the shape mask column (§4.5) | platform minions' shapes |

Why a separate plugin and not `GSTNLevelObjects` or `GSTNPlatform`: three independent consumers (combat impact, wall movement,
platforms) need the same data contract and one tag owner; `GSTNLevelObjects` is content-level and depends on combat,
`GSTNPlatform` depends on Mover and will depend on minion seams, and the wall-movement code must not depend on either. A project
that has no enemies-as-platforms still wants "this wall cannot be slid on" and trampolines. Canary rule §3.7.7 applies: `GSTNSurface`
builds and tests alone. Alternative considered: fold `GSTNSurfaceCore` into the proposed `GSTNCore` helper plugin (modularity §3.6);
rejected because `GSTNCore` is reserved for helpers with no gameplay vocabulary.

Dependency edges to list in the layers file (`check_modules.py`): `GSTNCombatCore → GSTNSurfaceCore` (impact types, launch params);
`GSTNMovement → GSTNSurfaceCore` (wall filter); `GSTNPlatformCore → GSTNSurfaceCore` (shape behaviours);
`GSTNPlatformParts → {GSTNPlatformCore, GSTNPartsCore, GSTNPartsNP}` (bridge).

## 4. Enemy body parts as surfaces (`GSTNPlatform` + `GSTNPlatformParts`)

### 4.1 Data: shapes with behaviours

`FGSTN_PlatformSpec::Shapes[]` (many-enemies §4.2) becomes an array of `FGSTN_PlatformShape`:

| Field | Meaning |
|---|---|
| `FGSTN_HitShape`-style box/capsule/convex in the enemy's local frame, stable `ShapeId` 0..7 (component names `Surface0..7`; was `Platform0..3`, cap raised to 8 for parts) | the collider |
| `FGSTN_SurfaceBehaviour Behaviour` | `Stand` for the classic platform top, `WallSlide`/`WallClimb` for a flank, `Launch` for a trampoline part; combinations allowed |
| `FName PartName` | `NAME_None` = body (always active while the platform is); otherwise the part of the entity's part set whose state gates this shape (resolved to an index by the bridge at registration; a name unknown to the part set is a validator error in the bridge, a warning in `GSTNPlatform` alone) |
| `uint8 ActiveActionStates` override | per shape; default = the spec's mask (never `Launched`, `Dying`, `Spawning`) |

Collision per shape is derived from `Behaviour` at construction (no per-instance responses to get wrong): `Stand` ⇒ Block the
Mover capsule on `GSTN_EnemyPlatform`; `WallSlide`/`WallClimb` ⇒ Block the capsule **and** the wall probe channel the wall modes
use; `Launch` ⇒ Block the capsule (the contact is a landing or a tumble contact); a shape with **no** traversal behaviour and
no `Stand` is Ignore for the capsule (it exists only as an impact surface, e.g. a `Surface.Type.Glass` crystal plate). All shapes
Ignore minion sweeps, projectiles and hit queries (M8 §4.3; hits use hurtboxes, not these shapes).

Validation (`IsDataValid`, as M8 §4d.5): flat-topped box or convex for `Stand`; **never a bone, the animated mesh or the
capsule**; a `WallSlide` shape face must be within the wall modes' `[MinSurfaceAngle, MaxSurfaceAngle]` in the enemy's upright
pose (warning: "players will never grab it"); `Launch` shape's launch face is its local up.

### 4.2 Pose: one defined point per step, never a bone

Unchanged from many-enemies §4.3/§4.5: the shapes are kinematic, Movable, unattached primitives moved (and enabled/disabled) at
**one defined point per fixed step**: host, from the end-of-F authoritative pose through the post-fixed-tick hook; clients, from the
interpolation sample once per game tick before physics. Approach B (base-local movement, Mover's own `MovementBase` handling) is
v1; approach D (lag-compensated base pose lookup `IGSTN_PlatformPoseSource::PoseForFrame`, `ViewOffsetFrames` as an NP input) is
stage 2 if spike P1 fails, and it serves wall sliding exactly as standing, since the wall mode's `ApplyMovementBase` is the
same seam (one call at the start of its tick).

Consequence for animation: a wall-slideable flank is a **rigid box in the enemy's frame**. An enemy whose flank visibly deforms
(breathing, idle sway) shows a few cm of mismatch; the design answer is shapes slightly inside the visible surface and
`WallGap`, not bone-following. A `PlatformModifier` timeline track (shape pose per frame, deterministic from `MoveId + frame`,
many-enemies §4.2 "later hook") covers a part that moves as part of an attack (a swinging arm one can slide on): the track is
data on the move, so it is identical everywhere and rollback-safe; it is **not** in the first delivery.

### 4.3 Moving surfaces and the player's simulation

- **Wall slide / climb**: the wall modes already ride dynamic bases (`bUseMovementBases`, `ApplyMovementBase`, `UpdateMovementBase`,
  `BaseRotation`, `bAddBaseVelocityOnExit`; wall-movement.md "walls can move/rotate"). The enemy shape is a movement base like any
  moving wall. The entry transition's `SameWallPolicy = HeightCap` compares world heights, meaningless on a moving base: a subclass
  `UGSTN_SurfaceWallEntryTransition : UGSTN_WallEntryTransition` (wall-movement.md "How to extend": subclass, never edit the core)
  treats a hit on a moving base as `ReentryCooldown` only; wall memory itself is unchanged.
- **Launch**: on landing (walking-mode floor contact) or on H11's tumble contact with a shape whose view has `Launch`, `GSTNCombatNP`
  queues `FGSTN_LayeredMove_Launch` through the GE path (H11b) with direction from the **shape's current normal** (the posed
  collider, so a tilted or rotated enemy launches along its part) and, when `bAddSurfaceVelocity`, the base velocity at the
  contact from `FGSTN_PoseHistory` (the same sample `bImpartVelocityOnLeave` uses). The pose and velocity are the ones the player's
  simulation used for the contact (B: the collider as posed for that step; D: the view-frame pose), so a resimulated frame
  reproduces the same launch. Tumbling victims take the `Surface.Type.Trampoline` bounce row, unchanged.
- **Hitting the surface** is not touching it: the H11 contact probe runs only for the tumbling victim's own capsule contact, and
  hits on the enemy use hurtboxes. A player attacking the trampoline part is never launched.
- **Host** plays with zero error (its collider is the authoritative pose). Clients: a wall slide on a moving enemy is base-relative,
  so sliding and climbing never reconcile; errors appear at grab (mount) and at shape loss, the same two events as platforms, with
  the same mitigations: `MaxSpeedWhileOccupied` and brace timeline **as per-part, per-enemy data** (owner TS.4, 2026-10-22 21:58:
  "depends on the part and the enemy"): `FGSTN_OccupantResponse { bCountsAsOccupant, MaxSpeedWhileOccupied (0 = no limit),
  BraceTimeline (soft, optional), bApplyToWallSlide, bApplyToStand }` on each platform shape, with an enemy-level default on the
  platform definition and a `BlueprintNativeEvent OnOccupantChanged` for designer logic (e.g. a boss that shakes the player off);
  no response = the enemy moves at full speed. Generous entry
  (`UGSTN_GA_WallSlide::EntryGraceFrames`), shapes slightly larger than the visual.

### 4.4 Part state → shape state (bridge `GSTNPlatformParts`)

The platform component exposes one input, `SetShapeMask(uint8 EnabledShapes)` (and the per-shape `Platform.State` of M8: `Active`,
`Braced`, `Breaking`, `Inactive`), applied at the **same defined point** as the pose. The bridge computes it:

- at registration it maps each `PartName` to a part index of the entity's `UGSTN_PartSetDefinition`;
- each frame on the host, from the **confirmed** `FGSTN_PartsFragment::BrokenMask` (end of F) and on clients from the
  interpolated/confirmed fragment that presentation already uses (combat-parts §6.5: what may be shown when), it derives
  `EnabledShapes = shapes whose part bit is clear`; a part that breaks puts its shapes in `Breaking` for `DropGraceFrames`
  (default 20 ≈ 320 ms), then disables them; a **restore** (P7b, `Combat.Event.Part.Restored`) re-enables them immediately
  (no telegraph needed: gaining a surface never hurts the player).
- Children break with their parent in the same frame (combat-parts §3.1), so a torso break drops every child part's shape in the
  same step; the mask handles it with no extra rule.

Nothing in the simulation reads the mask for traversal: the player's sweep simply stops finding the collider. The one-RTT-late
loss on the client is a single correction into Falling (platform rule "server-announced loss", many-enemies §4.6), made
invisible by the grace telegraph. The simulation of parts (`GSTNPartsNP`) is untouched: the bridge is a reader.

### 4.5 Platform minions (`GSTNPlatformMinion`, late)

Same model; the mask comes from the minion store's part mask column (`GSTNPartsMinion`, combat-parts §5.5) and travels with the
proxy actor's state (+1 byte per update: `EnabledShapes` 8 bits; the proxy already carries active state and frame stamp). The
`IGSTN_MinionPublishObserver` payload gains the slot's part mask. Cap unchanged (≤ 8 platform minions, `gstn.Platform.MaxActors`);
traversal parts do not raise it (TS.3).

## 5. Wall movement: what changes

### 5.1 Nothing, for the mechanics

Sliding, climbing, corners, adhesion, orientation and base riding are the existing pipeline. A shape with `WallSlide` that
responds Block on the wall probe channel is a wall. **First delivery uses collision responses only** (§4.1): no edit to the
wall-movement core, so enemies with slideable flanks work before TS.2 is answered.

### 5.2 The surface filter (core edit, owner OK TS.2 given 2026-10-22 22:14: some surfaces allow wall slide, others do not)

Collision responses cannot express "this *static* wall forbids wall slide but still blocks the player", nor "climbable but not
slideable". *Proposal*: `FGSTN_WallCheckSettings` gains `FGSTN_WallSurfaceFilter { FGameplayTagQuery RequiredBehaviours;
bool bUnknownSurfacesPass = true }`, and `UGSTN_WallQueryUtils::IsHitOnValidWall` (and the entry transition's `SweepForWall`)
evaluate the query against `UGSTN_SurfaceUtils::Resolve(Hit).Behaviours` after the angle test. Defaults keep today's behaviour
(empty query = everything passes). The slide mode's query is `Surface.Behaviour.WallSlide`, the climb mode's
`Surface.Behaviour.WallClimb`; a designer-only `NoWallSlide` is just a `UGSTN_SurfaceBehaviourComponent` with `Behaviours`
lacking `WallSlide`. Cost: one component lookup per accepted probe hit (cached per component pointer within the tick state).
This is the same class of edit as the approved `IsMovementLocked` (one extension point in the base, default = old behaviour).
Rollback: the resolve reads static configuration only.

`UGSTN_GA_WallSlide` needs no change: the ability activates when the transition accepts a wall; with the filter, the transition
simply never accepts a forbidden surface.

## 6. Lifecycle while a player is on an enemy surface

| Event on the enemy | Rule (shape state, player) |
|---|---|
| Moves, turns | base riding (§4.3); AI limiter `MaxSpeedWhileOccupied` while a player is on any shape or within mount range |
| Stagger in place, light push back | shapes stay `Active` (slow pose change, base-relative) |
| Launched (knockback), juggled | shapes `Breaking` after `DropGraceFrames`, then off, unless the launch *is* the telegraph (M8 §4d.5); the sliding player detaches with the base velocity (`bAddBaseVelocityOnExit`) into Falling; a player standing on a `Stand` shape likewise |
| Hitstop (`Combat.Mover.Frozen` on the enemy) | pose frozen; the sliding player keeps sliding on a still wall (the player's own hitstop is H7's `IsMovementLocked`) |
| Part broken (§4.4) | that part's shapes `Breaking` → off; others unaffected; wall memory of the player untouched (a different wall) |
| Part restored (P7b) | shapes back on immediately |
| Death, shatter, pool release, promotion | all shapes `Breaking` → off before the actor goes; never released with an occupant |
| Grabbed, thrown (M6/M10) | all shapes off after grace (platform rule); a link on the enemy locks traversal |
| Gravity change (§2d) | shapes authored in the enemy's frame; wall angle test uses gravity-up, so a flank can stop being a wall and become a floor (`FloorModeName`) or a ceiling (`NoWallModeName`) by the existing rules |
| Detached chunk (combat-parts §6.2) | presentation only; never a surface |

Player-side events are unchanged: a hit on the sliding player applies knockback through the normal path, and the wall mode exits
through Mover's queued move as today.

## 7. Networking and rollback

- **Where state lives**: surfaces add **no sync state** anywhere. The enemy's pose and part mask are already replicated/confirmed
  (fighter: actor replication + parts fragment; platform minion: proxy state); the player's wall state is the existing
  `FGSTN_WallMovementSyncState` plus Mover's `MovementBase`. `bAddSurfaceVelocity` reads the pose history sample, which is derived
  data of replicated poses.
- **Determinism**: every read in the player's simulation is a sweep against a collider that is posed and enabled by a projection
  of confirmed state at one defined point per step; the behaviour data is static. No `IsServer()`, no "is resimulating", no reading
  of the enemy's live components beyond the sweep (CLAUDE.md rule 1).
- **Listen host**: the host's collider is the authoritative pose, so the host has zero error and no OnRep; host-local presentation
  (`Surface.Event.Launched`, the brace cue) fires from the same confirmed-state projection that clients use, once.
- **Lag compensation (H10)**: a sliding or standing player stores the **composed world transform** in the hit history, never
  recomputed from the current base (many-enemies §4.7); the enemy's shapes are not hit geometry, so they are not rewound.
- **Expected corrections** (acceptance): 0 while sliding/standing on a shape that stays active; ≤ 1 at mount and ≤ 1 at loss per
  event on clients; 0 on the host.
- **Spikes** (plan T0, plus platform P1–P4 which this spec reuses): TS-A wall slide on a kinematic unattached movable collider at
  150/300/500 cm/s with base rotation; TS-B collider disabled under a sliding player (drop behaviour, no stuck base, no
  penetration); TS-C launch from a moving shape (velocity composition equal on all machines). Stop conditions in the plan.

## 8. Presentation (never in the simulation)

Surface highlight while within mount range (material parameter, from the same confirmed-state projection), brace timeline on mount
(`BraceTimelineOnMount`), launch cue on `Surface.Event.Launched`, breaking telegraph during `Breaking` (shape state is readable
by presentation components). All Blueprint-free of C++ through gameplay cues and `BlueprintImplementableEvent`s on the platform
presentation component.

## 9. Tests and acceptance

- Automation `GSTNSurface.Core.*`: `ResolveOrderComponentBeforeProviderBeforeMaterial`, `DefaultPassesEverything`,
  `PerComponentOverride`, `LaunchParamsValidated`, `TagsDeclaredOnce`. `GSTNPlatform.Shapes.*`: `MaskDisablesShapes`,
  `CollisionDerivedFromBehaviour`, `PartNameResolved`, `BreakingGraceThenOff`. `GSTNPlatformParts.*`: `MaskFollowsBrokenMask`,
  `RestoreReenables`, `ChildrenDropWithParent`. Wall movement (project, with the filter): `WallFilter.{ForbiddenSurfaceRejected,
  UnknownPasses, ClimbOnlyRejectsSlide}`.
- Harness (listen primary, dedicated secondary, 60/15 ms, 2 % loss): `scenario_surface_wallslide_enemy` (host and a client slide
  on a moving fighter's flank, jump off: 0 reconciles while sliding, ≤ 1 at mount, velocity on exit includes the base),
  `scenario_surface_part_break` (c2 breaks the part under c1: one correction, falls with base velocity, telegraph seen),
  `scenario_surface_launch_part` (trampoline part on a moving enemy: launch velocity equal everywhere), `scenario_surface_nowallslide`
  (a static wall marked without `WallSlide` is never grabbed), plus `scenario_trampoline` (H11b) unchanged.
- Acceptance: a designer marks a level wall as non-slideable and gives a fighter a slideable flank and a trampoline back **without
  C++**; R-B canary for `GSTNSurface` alone and for the project with `GSTNSurface`/`GSTNPlatform`/`GSTNPlatformParts` removed
  (walls behave as today, enemies have no surfaces, everything builds).

## 10. Risks

| Risk | Mitigation |
|---|---|
| Rigid shapes versus animated flanks look wrong on big breathing enemies | shapes inside the visual, `WallGap`, later `PlatformModifier` track; validator warning when a shape's face is far from the mesh |
| Wall-mode base riding not tested on kinematic unattached colliders (only on moving-wall meshes) | spike TS-A first; the base path is the one the walking mode uses for platforms (P1) |
| Shape loss under a sliding player gives a penetration or a stuck `MovementBase` | TS-B; `NoWallGraceTime` → `NoWallModeName` already handles "no wall"; the grace telegraph |
| Approach D needed for wall slide too | same seam (`ApplyMovementBase` start of tick); the cost per player per step is counted once (P4) |
| Dependency creep: parts ↔ platform ↔ surface | only Core→Core edges plus the one bridge; parts and wall movement never know platforms |
| The wall-core edit (filter) is refused | §5.1 works without it; only static "no wall slide" surfaces and climb-only surfaces wait |
| Platform plugin not yet planned (M8 "when this is built") | plan T3 delivers the minimum of `GSTNPlatform` for fighters if no platform plan has started, and the platform plan builds on it |

## 11. How to extend

- A new behaviour: declare a `Surface.Behaviour.<Name>` leaf in `GSTNSurface` (or in the plugin that introduces the mechanic, same
  open-subtree rule as `Combat.State.*`), add a `FGameplayTagQuery` field on the consumer, never a branch on the tag name in core
  code.
- A new provider: implement `IGSTN_SurfaceProvider` (C++ or Blueprint) on an actor or component.
- A surface that changes with the enemy's move: the `PlatformModifier` track (data on the move) once built; never a bone.
- A per-surface wall-slide tuning (`Slippery`): a subclass of `UGSTN_WallSlideMovementMode` overriding `ComputeVerticalSpeed` and
  reading `FGSTN_SurfaceView` of the current contact, kept outside the core.
