# Combat M7 — Piggyback and pair moves (implementation specification)

Part of the combat design ([combat.md](combat.md)); **ships as its own plugin `GSTNPiggyback`** (section 2b). Read §5, §9.2, §10.2, §12.7 and §12b there, the M3 and M4
specifications, and the M6 specification (links). Expands the M7 outline of [combat-m5-m9.md](combat-m5-m9.md).
Section numbers below are local to this file. "Move" means a timeline ability (`UGSTN_GA_BaseCombat`, combat.md §11).

> **M6 assumptions** (the M6 specification was written in parallel; reconcile when it lands, and fix the names here if
> they differ): (A1) `FGSTN_LinkState { LinkType; Role; PartnerId; LinkFrame; LinkSerial }` exists in the combat sync state
> and is part of the per-step snapshot of every entity; (A2) `UGSTN_LinkedMovementMode` (parked, no collision, no
> velocity) with the Mover tag `Combat.Mover.Linked` and a combat request that enters it; (A3) a **link inbox** on the
> combat component (frame-stamped, drained like the hit inbox of M4 §7) carrying link requests (`Break`, `PartnerMove`
> are used here), and a `LinkRequest` hit-resolution step that dispatches by hit type to link rules; (A4) a server-side
> exit placement helper (`UGSTN_LinkUtils::FindExitPlacement`) that sweeps the capsule over candidate positions; (A5)
> role tags `Combat.Link.Role.*` projected by M6 (5.4) and the link spike result of M6 (parked transform is viable).
> If A3 has no rule interface, M7 introduces `UGSTN_LinkRule` (section 3) and M6's grab uses it.

## 1. Scope

**Goal:** a player rides a teammate. Mounting is predicted on the rider, confirmed or refused by the carrier's
simulation; while mounted the carrier moves normally under a weight penalty and each player has role-only actions;
two **pair moves** combine both players (carrier throws the rider, rider boost-jumps off the carrier); hits, credit,
animation and camera work for the pair.

Out: enemies as mounts, two riders and chains (*Proposal*: not allowed, section 12), held objects, forward-predicting
the partner of a pair move (combat.md §12b *Open*, kept as a decision gate in section 2 spike 4), minions (M8).

## 2. Spikes first (record results in combat.md §17)

1. **Anchor agreement**: the rider's client computes an **anchor** (carrier socket transform) from the interpolated
   carrier; the server computes it from the carrier rewound to the rider's view time (`RewindActors`, as M4 §9).
   Measure the divergence in cm at carrier speeds 0, walk, run, dash, 60/15 ms, 2 % loss. Decides whether rider-initiated
   exits, launches and rider hitboxes may be **predicted** (divergence < 10 cm, no reconcile at `ShouldReconcile`
   tolerance) or must fall back to server-given positions (section 6.4 fallback).
2. **Presentation attach**: attach the rider's *mesh* (and the camera anchor) to the carrier's mount socket vs. attach
   the whole actor root, with NP visual smoothing active, on the carrier's client (carrier predicted, rider
   interpolated), the rider's client (carrier interpolated) and the server. Check pops at link start/end and under
   forced reconciles of both. Expect: mesh + explicit camera anchor.
3. **Optimistic mount cost**: refused mount under 60/15 ms and 2 % loss: rollbacks on the rider's client (expect 1), the
   visible snap of the attached presentation, and the carrier's correction count on acceptance (expect 1, from the
   weight GE and the link state).
4. **Partner-move delay** (gate for the 12b *Open*): carrier throws rider and rider boost-jumps; measure on the partner's
   screen the time from the initiator's start to the partner's visible reaction (expect RTT/2 + interpolation or RTT
   + correction). If the median exceeds 150 ms with presentation hiding on, open a forward-prediction milestone for pair
   partners (ForwardPredict LOD for the pair only; needs the fork's per-entity LOD, combat.md §12.7).

## 2b. Plugin and module boundaries

**Plugin `GSTNPiggyback`** (`Plugins/GSTNPiggyback/`), optional and self-contained, built on the link system. Modules:

| Module | Type | Contents | Depends on |
|---|---|---|---|
| `GSTNPiggybackCore` | Runtime | section 3 (pure types, `UGSTN_MountUtils`), tags header | `GSTNCombatCore` (types, tag requirements) |
| `GSTNPiggybackRuntime` | Runtime | sections 4, 5: abilities, `UGSTN_PairMoveDefinition/Set/Registry`, `UGSTN_GE_CarryLoad`, `UGSTN_LinkRule_Mount`, `UGSTN_HitStep_MountHit`, presentation component, kit extension | `GSTNPiggybackCore`, `GSTNCombatGAS`, `GSTNCombatNP`, `GSTNInputCore` |
| `GSTNPiggybackEditor` | Editor | pair-move asset factory/validators, `Debug.Anchor` panel | `GSTNPiggybackRuntime`, `GSTNCombatEditor` (M5; optional) |
| `GSTNPiggybackTests` | Developer | Automation tests and harness content | all of the above |

Allowed dependencies: only downwards as listed. **Must not depend on piggyback**: `GSTNCombat*`, `GSTNInput*`, the project
module `GASP_Giustino2` core code (the project may add a thin integration module, `GASP_Giustino2Piggyback`, for camera
anchor and perception: the only place that references both). `GSTNPiggyback.uplugin` lists `GSTNCombat` as a required plugin;
nothing lists `GSTNPiggyback`. Type names in sections 3-5 keep the `GSTN_` prefix; files live in the modules above.

**Host extension points required** (small changes in `GSTNCombat`, specified and reviewed with M6; none contains
piggyback names): (1) **link rules** registered per link type tag (`UGSTN_LinkRule`, A3) and a `LinkRequest` step that
dispatches to them; (2) **kit extensions**: `UGSTN_PrimaryKitDefinition::Extensions (TArray<UGSTN_KitExtension*>
Instanced, soft classes)` read by the loadout compiler through `UGSTN_KitExtension::Compile(FGSTN_CompileContext&)` (here:
carrier profile, pair-move registry entries, disallowed-command compilation, anim layers); (3) **rules extensions**
(`UGSTN_CombatRulesExtension` list on `UGSTN_CombatRules`); (4) **pipeline steps** and **snapshot contributors**
(`IGSTN_SnapshotContributor`: per-entity `Weight`/`MaxCarryWeight`; the anchor substitution for parked roles is generic
in M6, M7 only supplies socket and offset); (5) the generic `FGSTN_LinkState`/`FGSTN_LinkActions` fields, 6 request slots
(section 10) and the generic request types `EnterLinked`, `ExitLink`, `ApplyLinkEffect`, `RemoveLinkEffect`,
`LaunchLinked`; (6) a **partner move** primitive in the link inbox (`PartnerMove{Id, TargetFrame}`) that calls a registered
`UGSTN_LinkActionProvider` (the pair-move registry implements it); (7) hitbox flags `bInteractionPickNearest`,
`bAffectsLinkPartner`. Each has a do-nothing default, so the host builds and runs with no piggyback code present.

**Enable/disable independently**: enabled or not in the `.uproject`. Disabled: no tags, `Command.Link.Mount` has no binding
(an unbound command is legal, 8.2), kits whose extension class cannot load warn and drop it, grabs and all other M6 features are
unaffected. Enabled: per game mode through the `FGSTN_MountRules` rules extension (absent = every mount refused) and per
kit through `CarrierProfile`/`RiderProfile`.

**Isolation tests**: (a) host build with the plugin disabled: M1-M6 Automation tests and scenarios unchanged (CI job
without the plugin); (b) `GSTNPiggybackCore` tests need no world or ASC; (c) `GSTNPiggybackTests` uses a minimal test kit and
the host's link-test fixtures (fake snapshots, fake link inbox), no project content; (d) the section 9 scenarios run with
only host + plugin content on a stock test level; (e) a CI script in `Tools/` checks that `GSTNCombat*` and `GSTNInput*`
contain no include or module reference to `GSTNPiggyback*`.

## 3. Core types (`GSTNPiggybackCore`, no UObject dependencies beyond data)

**Mount data** — `FGSTN_CarrierProfile { bool bCanBeMounted; FGameplayTagContainer DisallowedCommands (commands that
cannot run while carrying; section 7); FName MountSocket; FTransform MountOffset; float MountMaxDistance (interaction
range cm); FGSTN_CarryTuning Carry; EGSTN_ExitMode DefaultExit (BehindCarrier / SideOfCarrier / AtAnchor); int32
RemountCooldownFrames = 30; }`. `FGSTN_CarryTuning { float SpeedPenalty; float AccelPenalty; float JumpPenalty; int32
AirJumpsRemoved; }`: every value is the penalty at ratio 1 (rider weight = `MaxCarryWeight`), scaled linearly by the
ratio. `FGSTN_RiderProfile { bool bCanRide; FName RiderPoseSocketHands/Feet (IK, optional); float AnchorYawLimitDeg
(rider facing relative to the carrier, default 360 = free); }`. Both live on the primary kit (`UGSTN_PrimaryKitDefinition`
gets `CarrierProfile`, `RiderProfile`).

**`FGSTN_MountRules`** (a `UGSTN_CombatRulesExtension` entry on `UGSTN_CombatRules`, per game mode, 2b): `bAllyOnly = true`; `PendingFrames = 2` (rider's pending
phase, 6.2); `CarrierBreakKnockback` (a hit on the carrier whose computed knockback exceeds it breaks the link; default
= tumble threshold); `RiderBreakKnockback` (same for the rider; default = 2 × carrier's); `RiderHitstunScale = 0.5`,
`RiderKnockbackScale = 0` below the break threshold (the rider flinches and takes damage, is not launched);
`CarrierCoverFrontDeg = 0` (0 = off; else hits on the rider whose direction to the rider is within this arc around the
carrier's facing are redirected to the carrier); `bCarrierShieldCoversRider = false` (M6 shield: carrier's block arc also
blocks hits aimed at the rider); `MaxCarrierTeleportCm` (link breaks if the carrier's transform jumps more than this in a
frame, e.g. respawn); `PendingTimeoutFrames = 20` (server safety); `DefaultCredit (EGSTN_PairCredit)`.

**Pure functions** — `UGSTN_MountUtils` (stateless, doubles, identical on every machine):
- `EvaluateEligibility(const FGSTN_MountEligibilityInput&) → EGSTN_MountVerdict { Accept, NotAllowedKit, AlreadyLinked,
  CarrierHitstun, CarrierBusyLock, WeightTooHigh, RiderIneligible, OutOfRange, Cooldown }` — the single rule used by
  the rider's prediction and the carrier's resolution. Input: both entities' snapshot entries (link state, action
  state, `Weight`, `MaxCarryWeight`, kit profile ids, `RemountCooldownEndFrame`), frame, distance. `Weight ≤
  MaxCarryWeight` (equal allowed).
- `ComputeCarryRatio(RiderWeight, MaxCarryWeight) → uint8 (0..255)`, clamped; `ComposeAnchor(CarrierTransform, SocketLocal,
  MountOffset, LocalYawDeg) → FTransform`; `ExitCandidates(Anchor, CarrierFacing, EGSTN_ExitMode) → TArray<FTransform>`
  (ordered, fed to the M6 sweep helper A4); `SplitCredit(Share, Amount)`.
- `UGSTN_LinkRule` (abstract, EditInlineNew, const; only if M6 lacks it): `Evaluate(FGSTN_LinkRequestContext&)` → Accept /
  Refuse(reason). `UGSTN_LinkRule_Mount` (GSTNCombatNP) wraps `EvaluateEligibility`.

**Pair move** — `EGSTN_PairRole { Carrier, Rider }`, `EGSTN_LinkOutcome { Keep, Break, SwapRoles }`, `EGSTN_PairCredit {
OwnerOnly, InitiatorAndOwner, Split }`, `EGSTN_PairPartnerMode { Cooperative (partner cannot refuse; partner hit does not
cancel its part), Interruptible }`. The definition itself is a UObject (section 4).

## 4. GAS side (`GSTNPiggybackRuntime`, on `GSTNCombatGAS`)

- **`UGSTN_PairMoveDefinition`** (PrimaryDataAsset, type `GSTN_PairMove`): `PairTag (Combat.PairMove.*)`;
  `InitiatorRole`; `InitiatorAbility` + `InitiatorTimelineOverride`; `PartnerAbility` + `PartnerTimelineOverride`;
  `PartnerStartOffsetFrames` (≥ 0, from the initiator's start frame; effective minimum is 1: the partner drains its
  inbox one frame later, section 6.5); `InitiatorRequirements` and `PartnerRequirements` (`FGSTN_TagRequirements` on
  the entities' context tags, evaluated from the snapshot at selection time); `Outcome` + `OutcomeFrame` (timeline frame
  of the initiator's move when it applies; `INDEX_NONE` = when the initiator's move ends); `ExitMode`; `PartnerMode`;
  `Credit` + `PartnerShare` (0..1, default 0.5 for `Split`); `bPartnerLocksCommands` (grants `Input.Lock.Command` to the
  partner for the duration of its move; default true); `MaxDurationFrames` (fail-safe: the link breaks if the pair move
  is still active). `IsDataValid`: both abilities are timeline abilities in the kit's registry, roles differ from the
  link's roles in the right way, `OutcomeFrame` inside the initiator timeline, partner offset not beyond the
  initiator's duration.
- **`UGSTN_PairMoveSet`**: list of definitions a kit offers; the compiler assigns each an index (`int16 PairIdx`, the
  wire identity, like the timeline registry).
- **`UGSTN_GA_Mount`** (`UGSTN_GA_BaseCombat`): the rider's mount move, bound to `Command.Link.Mount`. Timeline:
  startup, active window with one interaction hitbox (`Combat.HitType.Interaction.Mount`, `RelationshipVariants`: `Ally`
  full shape, every other relationship `bIgnore`), recovery. `bInteractionPickNearest` on the hitbox spec (new flag,
  M4 `FGSTN_HitboxSpec`): among eligible overlapped carriers only the nearest (then lowest id) receives a request, once
  per execution. The move ends itself when the link phase becomes Active (cancel request, section 6.2).
- **`UGSTN_GA_Dismount`**: bound to `Command.Link.Dismount` for the rider (section 7); a short timeline (cosmetic
  jump-off, no hitbox) that issues the unlink; `UGSTN_GA_Drop` for the carrier (same, partner side).
- **`UGSTN_GA_PairMove`** (`UGSTN_GA_BaseCombat`): base for pair-move initiator abilities; property `PairMove`
  (definition). It adds nothing at runtime beyond identifying the definition to the compiler, which stores `PairIdx` and
  `PartnerRequirements` in the binding's compiled entry; designers subclass in Blueprint only for timeline content.
- **`UGSTN_GE_CarryLoad`**: infinite GE on the carrier, applied/removed by the combat model's requests (6.3).
  Modifiers (via `UGSTN_MMC_CarryLoad`, reads SetByCaller `Combat.SetByCaller.CarryLoad.Ratio` in [0,1] and the kit's
  `FGSTN_CarryTuning` through the kit's tuning data): `WalkSpeed`, `Acceleration`, `JumpVelocity` multiplied by
  `1 − Penalty × Ratio`, `AirJumpCount` reduced by `AirJumpsRemoved × Ratio` (round down). Mover sees them through the
  stats modifier (combat.md §10.2). Grants `Combat.State.Carrying` as an ordinary GE tag (a lock-style tag, not
  projected). `UGSTN_MovementAttributeSet::MaxCarryWeight` and `Weight` (M3 §5) are the inputs; a carrier with
  `MaxCarryWeight` = 0 cannot carry.
- **Kit additions**: `CarrierProfile`, `RiderProfile`, `PairMoves (UGSTN_PairMoveSet)`, `CarrierAnimLayer`,
  `RiderAnimLayer` (M3 `AnimLayer` pattern), role bindings in the usual `KitBindings` (section 7).

## 5. NP side additions (`GSTNPiggybackRuntime`, on `GSTNCombatNP`)

- **Snapshot entity** (M4 §5) gains: `FGSTN_LinkState Link`, `Weight`, `MaxCarryWeight`, `RemountCooldownEndFrame`,
  `ActivePairIdx`, and the **anchor substitution**: when the snapshot is built, an entity whose link is Active in a
  parked role (rider, grabbed) has its `Transform` replaced by `ComposeAnchor(carrier.Transform, …)` and its velocity by
  the carrier's. The builder processes carriers first (the substitution reads the carrier's raw entry, so no
  chain); a rider whose partner is itself a rider is a data error (logged, substitution skipped).
- **`UGSTN_LinkPresentationComponent`** (client, FinalizeFrame/Tick; no simulation reads): attaches the rider's visual
  mesh to the carrier's mount socket with `MountOffset` and the rider's local yaw while the link phase is Pending or
  Active (blend in `MountBlendFrames`, out `ExitBlendFrames` to the real simulated transform), publishes
  `GetPresentationTransform()` (the camera anchor and the position for AI perception/UI; project camera and
  perception read it, *tell the user*: shared Blueprints), links the role anim layers (6.8), plays refused/accepted cues.
  On the carrier's client the carrier is predicted and the rider interpolated; the attach is always to the local carrier mesh.
- **`UGSTN_HitStep_MountHit`** in the pipeline (after `ComputeKnockback`, before `ApplyDamage`): rules of section 9.
- **`UGSTN_HitStep_TeamFilter`** (M4) extension: hits between link partners are ignored unless the hitbox sets
  `bAffectsLinkPartner` (new flag, default false), so a carrier's moves never hit its rider by overlap.
- **Combat tick order** (extends M3 §7.2), new steps: (a) drain link inbox (Break, PartnerMove); (b) resolve mount
  pending phase; (c) pair-move scheduling (`PendingPartnerStart`); (d) role/context tag projection; (e) execute exit
  requests. Details in section 6. The fixed `Requests` array of M3 §7.1 grows from 4 to **6** entries; new request
  types: `EnterLinked`, `ExitLink`, `ApplyLinkEffect`, `RemoveLinkEffect`, `LaunchLinked` (executed in the ASC pre-tick
  like the others; `LaunchLinked` reads the launch from `PendingExit`).
- **Registry** `UGSTN_PairMoveRegistry` (compiled by `UGSTN_LoadoutComponent` with the table): `PairIdx →` abilities'
  registry indices, requirements, outcome data, credit.

## 6. Algorithms

### 6.1 Mount request (rider side, frame F)

1. The rider's mount move is in its active window; the interaction hitbox is evaluated by the M4 attacker step 6.1–6.4
   against the snapshot. Eligible candidates = overlapped entities with relationship `Ally`, passing
   `EvaluateEligibility` on the snapshot. None → nothing happens (the move plays out; optional "no target" cue).
2. Pick the nearest (then lowest id). Write `Link = { Mount, Rider, Partner = carrier, LinkFrame = F, LinkSerial =
   Hash(RiderId, F) & 0xFFFF (never 0), Phase = Pending }` into the rider's own sync state **on every machine that
   ticks the rider** (the same code on server and client: no authority branch).
3. Deliver the request to the carrier: a hit-style inbox entry `{ Frame = F, AttackerId, HitType Interaction.Mount,
   Serial }`, written only where the carrier ticks (server, M4 §7). The client therefore only predicts its own side.

### 6.2 Carrier resolution and rider confirmation

- **Carrier tick at F+1**: the `LinkRequest` step runs `UGSTN_LinkRule_Mount` on the end-of-F snapshot (rider already
  Pending, so a rider that was hit at F is `RiderIneligible`). Conditions, in order: kit `bCanBeMounted`; carrier not
  linked (`LinkType == None`, which also forbids chains and two riders); carrier not in hitstun/tumble/hitstop of a hit
  and without `Combat.Lock.Link` (a lock tag a kit/ability can grant); `Rider.Weight ≤ Carrier.MaxCarryWeight`; distance
  to the carrier's anchor ≤ `MountMaxDistance + slack` (slack covers the lag-compensated position, 1 s of walk speed ×
  RTT/2 is not allowed: use the rewound anchor when `bUseLagCompensation`); remount cooldown elapsed; rider eligible
  (`bCanRide`, not itself linked other than this Pending). **Accept**: write `Link = { Mount, Carrier, rider, F, Serial,
  Phase = Active }` and `CarryLoadQ = ComputeCarryRatio`; requests `ApplyLinkEffect` (carry load, Arg = ratio × 1000) and
  the carrier-context tags. **Refuse**: write `LastRefusedSerial = Serial`, `MountRefusedFrame = F+1`; no other state.
- **Rider tick at F+2** (`PendingFrames = 2` after F): `ResolvePending`: the rider becomes Active unless the carrier's
  snapshot entry shows `LastRefusedSerial == mine`. On the server the carrier is end-of-F+1 (accept or refuse is
  visible), on the rider's client the carrier is the interpolated, stale entry which shows neither, so the client
  **optimistically accepts** — same code, different data (5.3). Active: requests `EnterLinked` (rider's Mover to
  `UGSTN_LinkedMovementMode`, parked at the rider's current transform, collision off), `CancelTimeline` of the mount move.
  Refused: Phase None, `RemountCooldownEndFrame = F + RemountCooldownFrames/2`, the mount move continues (recovery).
- **Rollback behaviour**: the client's rider state matches the server's iff the server accepted; a refusal makes the
  rider's server state differ at F+2 → one reconcile when the state arrives (about RTT/16 ms frames), re-simulation
  runs `ResolvePending` against the snapshot of that pass (which now shows the refusal only if the carrier is a
  predicted entity; for an interpolated one the **server's** rider state is simply restored and then continues from it:
  after the rollback the rider has Phase None, and the replayed frames take the same path because the sync state, not the
  snapshot, carries the outcome). The carrier's client learns the link by correction (about one rollback). Neither
  the parked Mover state nor the presentation attach depend on the carrier's position, so the rollback replays nothing
  positional on the rider; the attach is visual only (detached with a blend on refusal).

### 6.3 Carry load and carrier context

On Accept, `ApplyLinkEffect` applies `UGSTN_GE_CarryLoad` with the stored ratio; on link end, `RemoveLinkEffect` removes
it (`RemoveActiveGameplayEffectBySourceEffect`, no handle stored; idempotent and deterministic). `CarryLoadQ` is part of
the carrier's `LinkState` extension so re-simulation re-requests identical magnitudes. The carrier gets
`Combat.Link.Role.Carrier` (projected, M6) and `Combat.State.Carrying`; the compiler makes the kit's `DisallowedCommands`
unavailable while it holds (7).

### 6.4 Rider anchor, parked transform and exit

- Parked: the rider's Mover is in `UGSTN_LinkedMovementMode` at the transform of its mount frame; the simulated transform
  never follows the carrier. Everything positional (hurtboxes, hitbox `Root` and `BakedSocket` spaces, aim origin, hit
  queries) comes from the snapshot's substituted transform = the **anchor**. `LocalYawQ` (int16, quantized degrees
  relative to the carrier, in `LinkState`) is the rider's facing, updated by its aimed moves and, when idle, by the
  stick within `AnchorYawLimitDeg`.
- **Exit** (dismount, throw, boost, link break): the server writes `PendingExit { Frame; Position (quantized cm); YawQ;
  Velocity (int16 × 3, cm/s / 4); Flags }` to the rider's state and `ExitLink`/`LaunchLinked` runs in the ASC pre-tick:
  teleport to `Position` (a Mover instant move through a GE), clear parked mode (Falling), launch with
  `FGSTN_LayeredMove_Knockback` (M4 §7; no hitstun unless set). `Position` = first free candidate of `ExitCandidates`
  (A4 sweep), or the anchor itself for launches.
- **Who computes it**: *rider-initiated* exits (dismount, boost jump) are **predicted by the rider's client** from its
  local anchor and carrier velocity, and the rider's server-side tick computes the same `PendingExit` from the
  anchor of the carrier **rewound to the rider's view time** (always on for riders, independent of
  `bUseLagCompensation`: one entity). If spike 1 fails, fallback: the rider's client does not predict the exit; it
  plays the cosmetic part immediately and the position arrives as a server correction (about RTT delay). *Carrier-
  initiated* exits (drop, throw, hit break) are server-authoritative; the rider's client sees them as a correction,
  hidden by `ExitBlendFrames` of the presentation (the visual mesh blends from the attach pose to the simulated pose).

### 6.5 Pair move

1. **Selection**: the command's binding (rider- or carrier-only, section 7) is chosen as usual; the compiled entry carries
   `PartnerRequirements`, checked against the partner's projected tags in the snapshot. Fails → the binding does not
   hold this frame (falls through per 8.2 rules).
2. **Initiator** (frame S): its timeline starts predicted; it writes `ActivePair = { PairIdx, S }` and a link-inbox entry
   to the partner `{ PartnerMove, Frame = S, PairIdx, LinkSerial, T = S + max(PartnerStartOffsetFrames, 1) }`, only
   where the partner ticks (server).
3. **Partner** (drain at frame F ≥ S+1): check `LinkSerial` equals the current link's and the partner's requirements;
   fail → drop silently and the initiator's pair move continues alone (its `Outcome` still applies). Pass: if `T ≤ F`
   request the partner ability (activation like a forced command, `ActiveSource = Forced`; `bPartnerLocksCommands` →
   command lock for the duration), else store `PendingPartnerStart = { T, PairIdx }` and request on frame T. The
   partner's own state is thus written on the server; its client learns it by correction. `Interruptible` partners
   drop their part on a hit; `Cooperative` ones do not (hitstun is queued behind the part).
4. **Outcome**: at `OutcomeFrame` of the initiator's timeline (frame stamp from its own timeline frame, hitstop-aware)
   the initiator writes `Link.Phase = Exiting` and posts `Break` (or `Swap`) to the partner; the partner's tick at the next
   frame applies it. `Break`: `RemoveLinkEffect`, role tags cleared, `RemountCooldownEndFrame`, exit placement per
   `ExitMode`. `SwapRoles`: roles swapped in both states (rider becomes carrier at the old carrier's position:
   reserved, not used by the reference moves).
5. **Failure paths**: link serial mismatch, partner KO, `MaxDurationFrames` exceeded → `Break` with the default
   exit; both sides emit `PairMoveAborted` (5.6 outbox) for debugging.

### 6.6 Reference pair moves (content, `Content/GSTNCombat/Kits/Test/PairMoves`)

- **`PM_ThrowRider`**: initiator Carrier, `GA_Carrier_ThrowRider` (startup 12 f, release at timeline frame 14, recovery
  18 f; aim = carrier's stick snapped to the movement plane, fixed pitch `ThrowAngleDeg`); partner Rider,
  `GA_Rider_Thrown` (`PartnerStartOffsetFrames = 12`: thrown pose before the release); `Outcome = Break` at frame 14,
  `ExitMode = AtAnchor`; at release the server fills `PendingExit.Velocity = ThrowSpeed × aim` (inherits
  `ThrowInheritCarrierVelocity` × the carrier's velocity). Thrown rider: Falling with M4 knockback layered move; its timeline
  has a sweeping capsule hitbox in `Root` space, `bSweepBetweenFrames`, `YawSource = HitboxVelocity`, `HitGroup` per target,
  active until landing or 40 f; after `ThrownActionableFrame` air actions are allowed; credit `InitiatorAndOwner`.
  `PartnerMode = Cooperative`.
- **`PM_BoostJump`**: initiator Rider, `GA_Rider_BoostJump` (crouch 6 f, launch at frame 6); partner Carrier,
  `GA_Carrier_Braced` (brief crouch, `Combat.Lock.Movement` for 10 f, no hitbox; offset 0 → starts at S+1);
  `Outcome = Break` at frame 6, `ExitMode = AtAnchor`; launch velocity = rider's `JumpVelocity × BoostMultiplier` upward +
  `InheritCarrierVelocity` × the carrier's horizontal velocity at the break frame (snapshot); rider air jumps refreshed
  (`RefundAirJumps`), credit `OwnerOnly`. Predicted on the rider (rider-initiated exit, 6.4).

### 6.7 Hits while mounted (`UGSTN_HitStep_MountHit`)

- **Hurtboxes**: both entities keep their set; the rider's is relative to the anchor. A hit that overlaps both hurts both
  (separate hit groups per target).
- **Rider is the victim**: damage and meter apply; `HitstunMultiplier × RiderHitstunScale`, knockback × `RiderKnockbackScale`
  (0: no launch, the rider flinches and the carrier is unaffected); a computed knockback over `RiderBreakKnockback`
  breaks the link and launches the rider by the computed vector (exit at the anchor). Optional cover:
  `CarrierCoverFrontDeg` (hit redirected to the carrier's hurtboxes) and `bCarrierShieldCoversRider` (M6 `Shield` step
  evaluated against the carrier's state and arc).
- **Carrier is the victim**: normal hitstun/knockback; above `CarrierBreakKnockback` the link breaks and the rider is
  dropped (exit `SideOfCarrier`, velocity = 0.5 × the carrier's launch); below it the rider rides the hit out and is
  not stunned. A carrier in hitstun refuses mounts (6.2) and cannot start pair moves.
- **KO** of either breaks the link on the server; `Combat.Event.Link.Broken` carries the reason.
- **Credit**: hit entries carry `CreditPartnerId` and `PartnerShare` taken from the attacker's link and `ActivePair`
  (`UGSTN_PairMoveDefinition::Credit`). `MeterGain` splits meter with `SplitCredit`; the authoritative `HitLanded`/`KO` events
  (5.6) gain an optional `Partner` field and the `Combat.Link.Credit.Partner` tag. Damage stays attributed to the hitbox
  owner; kill credit and assists follow the event consumers. Default `OwnerOnly` for non-pair moves from either role.

### 6.8 Animation

Kit-provided linked anim layers per role (`CarrierAnimLayer`: carrying locomotion; `RiderAnimLayer`: seat pose, hold
pose; the project's locomotion layer for the carrier stays underneath), linked by `UGSTN_LinkPresentationComponent` when
the role tag changes (a presentation reaction to replicated/interpolated state, not a sim callback). Optional hand/foot
IK: the rider's IK targets are carrier sockets `GSTN_MountHandL/R`, `GSTN_MountFootL/R` (RiderProfile). Montage
timelines of pair moves are ordinary synced montages (M3); `BakedSocket` hitboxes of the rider are baked relative to
its root, which is the anchor, so no animation is evaluated at runtime.

## 7. Bindings and inputs (*Proposal* for the outline's *Open*; the standard set already reserves `Command.Link.*`)

- New button **`Input.Button.Interact`** (designer tag in `GSTNCommands.ini`, one press, no chord) and slots
  `Command.Link.Mount`, `Command.Link.Dismount` = a press of Interact; `Command.Link.Drop` = Interact + crouch held (a
  `Hold(Crouch)` slot, more specific than Dismount). The same button means three things purely through bindings:
  | Command | Requirements | Ability |
  |---|---|---|
  | `Command.Link.Mount` | ignores `Combat.Link.Role.*` | `UGSTN_GA_Mount` |
  | `Command.Link.Dismount` | requires `Role.Rider` | `UGSTN_GA_Dismount` |
  | `Command.Link.Drop` | requires `Role.Carrier` | `UGSTN_GA_Drop` |
  | `Command.Movement.Jump` | requires `Role.Rider` | `GA_Rider_BoostJump` (pair move) |
  | `Command.Special.Directional` | requires `Role.Carrier` | `GA_Carrier_ThrowRider` (pair move) |
  | `Command.Attack.*`, `Command.Special.*` | requires `Role.Rider` | the rider's own attacks (aimed with its stick) |
- While riding, the context tags `Grounded` and `Airborne` are **absent** (the linked mode exposes neither), so every
  ordinary binding of the rider stops holding and every command is either a Rider binding or unbound; nothing needs
  to be removed. Carrier bindings with `Role.Carrier` + `Grounded` win by tag count (9.2).
- **`DisallowedCommands`** of the carrier profile: the compiler adds `Combat.Link.Role.Carrier` to the ignored tags of
  every binding of those commands that does not already require it. Validation: a disallowed command that is a pair
  move trigger is an error.
- Same-press hazards: after a dismount the remount cooldown (default 30 f) stops the same Interact press from
  re-mounting; the rider's Jump dismount/boost and the carrier's Jump (an ordinary binding) never conflict (different roles).

## 8. Link breaking rules (M6 list, M7 values)

Rider dismount; carrier drop/throw; pair-move `Outcome`; hits above thresholds (6.7); KO; death/EndPlay of either entity
(`UGSTN_CombatWorldSubsystem::NotifyEntityRemoved`, server); carrier teleport over `MaxCarrierTeleportCm` in one frame;
server timeout of Pending (`PendingTimeoutFrames`); carrier entering a Mover mode a kit lists in `CarrierProfile`
`ForbiddenModes` (*Proposal*: empty by default, wall modes allowed). Every break writes `PendingExit`, `Combat.Event.Link.Broken`
(reason tag), removes the carry load and sets the remount cooldown.

## 9. Content, tools, harness

- Test kit additions on the test character: `CarrierProfile` (socket `GSTN_Mount` on the mesh's spine, offset), `RiderProfile`,
  the two pair moves, a heavy variant (`MaxCarryWeight` below the other's `Weight`) for the refusal scenario; two
  dummies (hit the pair).
- Debug draw: `gstn.Combat.Debug.Links 1` (M6) shows the anchor, the socket, the interaction volume and the pending/active
  phase; `gstn.Combat.Debug.Anchor 1` draws server vs client anchor (spike 1 data); a log line per link transition.
- Harness (`gasnpp_test.py`; helper `place_pair(rider_client, carrier_client, distance)` puts both pawns facing each other;
  `link_tap(client, command)` sends the Interact button; all with 60/15 ms with and without 2 % loss):
  `scenario_mount_basic`, `scenario_mount_refused` (heavy rider: exactly one rollback on the rider's client, state back
  to Actionable, carrier unaffected), `scenario_mount_carry` (carrier runs, jumps and uses a dash for 3 s: rider's
  presentation transform within tolerance of the carrier's, **no corrections after the link phase**),
  `scenario_rider_attack` (rider attacks a dummy with its stick: hit equal on server and client; anchor divergence logged),
  `scenario_throw_rider`, `scenario_boost_jump`, `scenario_dismount_drop`, `scenario_mounted_hit` (dummy hits the carrier
  below/above `CarrierBreakKnockback`, then the rider), `scenario_mount_forced_reconcile` (forced reconciles on each client
  at the request frame, the Pending frame, the first Active frame, a pair-move start and its outcome frame). Clean = snapshots
  equal (combat state incl. link state, attributes, positions after settle) and no reconciles besides the forced ones and
  the **expected** ones: the carrier's client at accept (1), at each partner-side start and at each carrier-initiated
  exit; the rider's client at a refusal (1) and at each carrier-initiated exit.
- **Automation tests** (Core and NP): `EvaluateEligibility` for every verdict and the weight boundary; `ComputeCarryRatio`
  clamps and quantization; `ComposeAnchor` and `ExitCandidates` ordering; `SplitCredit`; `PairMoveDefinition` validation
  (missing abilities, offset past the initiator, `OutcomeFrame` outside the timeline, roles); binding compilation with
  `DisallowedCommands` and role requirements (rider context has no Grounded: selection results table); link inbox ordering,
  drain at `Frame + 1` and rollback trimming (entries with `Frame ≥` the restored frame dropped); snapshot anchor
  substitution (carriers first, chain rejected); `ResolvePending` accept/refuse with fake snapshots; `PendingPartnerStart`
  scheduling for offsets 0, 1 and 12; `NetSerialize`/`ShouldReconcile`/`Interpolate` round trips of every new field.

## 10. Sync state additions

The combat sync state belongs to the host, so M7 adds no piggyback-specific struct to it. Host prerequisites (raised with
M6, section 2b): `FGSTN_LinkState` (M6) carries a generic `uint8 Phase (None/Pending/Active/Exiting)`, a `uint8 Payload` and
`int16 LocalYawQ` (anchor-relative facing: grabs use it too); the combat state carries a generic **link-action block**
`FGSTN_LinkActions { uint16 LastRefusedSerial; int32 RefusedFrame; int32 CooldownEndFrame; FGSTN_ActiveLinkAction {
int16 Id; int32 StartFrame; } Active; FGSTN_PendingLinkAction { int32 Frame; int16 Id; } PendingStart; FGSTN_PendingExit {
int32 Frame; FVector_NetQuantize10 Position; int16 YawQ; int16 Velocity[3]; uint8 Flags; } Exit; }`. Piggyback's meanings:
`Payload` = `CarryLoadQ`, `Id` = `PairIdx`, `CooldownEndFrame` = remount cooldown. Link inbox entries gain a generic
`Id`/`TargetFrame` payload (`PartnerMove`). Request array 4 → 6 and five request types (section 5) are host changes too.
All fields are end-frame stamps or empty while idle (an unlinked entity's delta stays zero); every field in
`NetSerialize`, `ShouldReconcile`, `ToString`, `Interpolate`; `Phase` and `Role` compared exactly, positions with the
existing quantization tolerance. The carry load and the pair move's ability travel in the existing ASC state.

## 11. Gameplay tags

Convention (CLAUDE.md, combat.md §9): tags are **native** (`UE_DECLARE_GAMEPLAY_TAG_EXTERN` in the owning module's tag header,
`GSTNPiggybackCore/Public/GSTN_PiggybackTags.h`), except designer-made input tags, which live in the plugin's own
`Plugins/GSTNPiggyback/Config/Tags/GSTNPiggybackCommands.ini`. A tag is owned by the plugin that declares it; other plugins
may *use* a tag by name only through data, never include the header. "Host" = `GSTNCombat` (M6 declares the link tags).

| Tag | Kind | Owner | Meaning | Added/removed by |
|---|---|---|---|---|
| `Combat.Link.Mount` | native | host (M6) | link type of piggyback | link state (`LinkType`), not a gameplay tag on actors |
| `Combat.Link.Role.Carrier` / `.Rider` | native | host (M6) | role context tags | projected by the combat tick (absolute counts, 5.4) while the link is Active |
| `Combat.HitType.Interaction.Mount` | native | piggyback | interaction hitbox type that creates a mount request | hitbox data of `UGSTN_GA_Mount`; no actor tag |
| `Combat.State.Riding` | native | piggyback | rider's phase Active | projected by the combat tick (5.4), cleared on any break |
| `Combat.State.Carrying` | native | piggyback | carrier carries a rider | granted by `UGSTN_GE_CarryLoad` (ordinary GE tag), removed with it |
| `Combat.State.MountPending` | native | piggyback | rider's 2-frame pending phase (cosmetics, locks) | projected by the combat tick |
| `Combat.Lock.Link` | native | piggyback | prevents a carrier from accepting a mount | granted by abilities/GEs with ordinary counts (11.4 lock rule) |
| `Combat.PairMove.ThrowRider` / `.BoostJump` | native | piggyback | pair move identities (events, quests, validation) | definition data; carried in events |
| `Combat.Mover.Linked` | native | host (M6) | parked mode tag | Mover mode (A2) |
| `Combat.Event.Link.Mounted` / `.Broken` / `.MountRefused` | native | piggyback (`Broken` also host) | authoritative events (5.6); `Broken` carries a reason tag below | server outbox |
| `Combat.Event.PairMove.Started` / `.Aborted` | native | piggyback | pair-move events | server outbox |
| `Combat.Link.BreakReason.{Dismount,Drop,Throw,Outcome,CarrierHit,RiderHit,KO,Removed,Teleport,Timeout}` | native | piggyback (generic ones host) | reason on `Broken` | emitter of the break |
| `Combat.Link.Credit.Partner` | native | piggyback | event tag: credited partner present | attribution in `HitLanded`/`KO` events |
| `Combat.SetByCaller.CarryLoad.Ratio` | native | piggyback | SetByCaller of the carry load GE | `ApplyLinkEffect` request |
| `Input.Button.Interact` | ini | piggyback | designer button tag | input config |
| `Command.Link.Mount` / `.Dismount` / `.Drop` | ini | piggyback (the first two reserved by host) | slots of the Interact button | slot asset, bindings (7) |

Rules: no `AbilityTriggers` or tag-event listeners on projected tags (5.4); `Combat.Lock.Link` is the only lock-style tag
and is read, never projected; all `Combat.State.*` above are removed by the same projection that adds them, so a break
cannot leave one behind (test: every break reason in section 8 ends with none of them present). Validation: an
`IsDataValid` check that every tag a piggyback asset references is registered (the plugin disabled → no tags).

## 12. Resolution of the outline's *Open* items (all *Proposal*)

- **Inputs**: one `Interact` button, three role-resolved meanings (7). Alternative if the pad has no spare button: Mount
  and Dismount on a `Hold(Special)` slot; only slots change.
- **Two riders / chains**: no. One rider per carrier (`LinkType == None` is a mount condition), a rider cannot carry. The
  data model allows more later (`PartnerId` is per link; a list would be M6 work).
- **Mounting enemies**: no; `Combat.Link.Mount` is allies only. A mountable enemy is a different link type
  (`Combat.Link.Ride`) with its own rule and a server-side carrier, because minions (M8) have no NP model.
- **Chain of command on hits**: rider flinch only below thresholds (6.7), defaults chosen to keep the rider active.

## 13. Acceptance criteria

1. Spikes recorded with the decisions of 6.4 (predict or fall back) and spike 4's verdict.
2. Scenarios of section 9 clean as defined; M1–M4 (and M6) tests and scenarios still clean.
3. Mount: the rider is attached on its own screen at the request frame + 2 (no waiting for the server); acceptance costs
   the carrier's client one correction; a refusal costs the rider one rollback and leaves no residual state.
4. While mounted and the carrier moves, no corrections occur on either client; the rider's hitboxes/hurtboxes sit where
   `Debug.Anchor` shows, on server and clients (divergence under 10 cm or fallback applied).
5. Both reference pair moves play on both screens; corrections only at the partner-side start and at carrier-initiated exit;
   a rider cannot be hit by its own carrier; credit follows the definition.
6. Role and disallowed-command bindings validated at compile time; the rider cannot jump, dash or use ground attacks
   other than its own bindings.
7. combat.md updated (§3 modules, §12b, §17, roadmap link); plugin builds and M1-M6 pass with it disabled; the host has no reference to it.

## 14. How to extend

**New pair move**: (1) author the initiator and partner timeline abilities (`UGSTN_GA_PairMove` child for the initiator,
a plain timeline ability for the partner); (2) create a `UGSTN_PairMoveDefinition` (roles, offset, outcome frame, exit,
credit); (3) add it to the kit's `UGSTN_PairMoveSet`; (4) bind the initiator's ability under an existing command tag
with the role requirement (`Combat.Link.Role.Rider` or `.Carrier`); (5) validate (`IsDataValid` on the loadout), run
`scenario_throw_rider` adapted by naming the move. No code.
**New carrier restriction**: add the command to `DisallowedCommands`. **New link rule or exit mode**: a
`UGSTN_LinkRule` subclass / an `EGSTN_ExitMode` entry plus its `ExitCandidates` case (pure function with a test).
**Different weight rules**: change `MMC_CarryLoad` and the kit's `FGSTN_CarryTuning`, or the eligibility function (one
place for prediction and resolution).

## 15. Risks

- **Anchor divergence** between the rider's interpolated view and the server (spike 1): rider hits and exits can
  disagree; mitigated by rewind of the anchor and the fallback to server-given positions. Attackers predicting a hit that
  the server rejects are corrected like any M4 hit.
- **Partner delay** on throw/boost (spike 4): the design tolerates it (the partner is launched or plays a reaction), the
  forward-prediction decision is deferred until measured.
- **Parked rider and the rest of the project**: systems that read the rider's actor location (AI perception, triggers,
  streaming, minimap) see the parked position, not the anchor. Provide `GetPresentationTransform()`/anchor from
  `UGSTN_LinkUtils` and review the project's users; the camera needs the anchor (*tell the user*: shared Blueprints).
- **Optimistic accept**: a refusal after the rider already showed the attach costs one rollback and a visible snap
  (hidden by blend); keep `PendingFrames` and the pre-check identical on both sides so refusals stay rare (only races).
- **Same button, three meanings** depends on the role tags being projected before the binding selection of the same
  frame (5.4, M6): add an assert in development builds.
- **Carry load and Mover**: the stats modifier rewrites settings every frame; a weight change at accept must show on the
  carrier on the same frame on server and client (harness: speed compared at accept).
- Dependence on the unfinished M6 spike and names (assumptions A1–A5).
