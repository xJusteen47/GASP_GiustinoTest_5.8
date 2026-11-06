# M7 implementation plan: piggyback and pair moves (plugin `GSTNPiggyback`)

Plan only. Checkpoints are named **G0..G12**. One checkpoint = one commit. The spec is
[`../combat-m7-piggyback.md`](../combat-m7-piggyback.md) (section numbers below are its local ones, "M7 §x"); the link system it builds on
is M6 as **shipped** on `feature/gas-npp-5.8` (M6 closed at 087f1bd, F15 follow-ups at 65cd8b5, HANDOVER 62), not M6 as specified: where
the two differ, the shipped names win and the spec is corrected in `docs/notes/m7-doc-deltas.md`.

Status: **self-approved** by the M7 thread on 2026-11-01 (project rule: a plan without architectural questions or new features is
approved by its thread). Section 5 lists the defaults taken for the owner questions M7.2-M7.19 that have no answer yet; all of them are
data (kit, rules or definition fields), so the owner can change any of them later without code.

Sources (short ids used below):

| Id | File | Used for |
|---|---|---|
| M7 | `docs/architecture/combat-m7-piggyback.md` | The spec of this milestone |
| M6 | `docs/architecture/combat-m6-defence-links.md`, `Plugins/GSTNLink/README.md` | Link types, roles, rules, inbox kinds, watchdog, exits, presentation |
| M6P | `docs/architecture/plans/m6-defence-links-plan.md` (PR #18 branch) | Format model, ground rules, approved shared-asset edits (its section 6) |
| EXT | `Plugins/GSTNExtensionTest` (M6 F14) | Proof that a link type and a rule can be added from outside `GSTNLink`: G4-G5 follow it |
| IN | `docs/architecture/interaction.md`, `Plugins/GSTNInteract` | The interact probe, `Interact.Target.*`, candidate providers (INT.5 priority) |
| MT | `docs/architecture/modularity-and-tags.md` | Layering §3, Blueprint §3.8, replaceable content §3.9, tags §4, extension surface §5b |
| DD | `docs/architecture/game-design-direction.md` | Owner direction (wins over M7): §3 interact = grab, §6b listen server |
| OQ | `docs/architecture/open-questions-m4-m10.md`, section M7 | Owner answers (only M7.1 answered) and open items |
| SDP | `docs/architecture/plans/spin-drill-plan.md` | The other thread that edits `GSTNLink`/`GSTNCombatCore` next (coordination, section 7) |

Owner direction folded in:

- **One interact button** (M7.1, DD §3): mount is a contextual meaning of the grab button, chosen by the probe through
  `Interact.Target.Mount`; it never competes with grab by binding.
- **Listen server is the reference topology** (DD §6b): the host is carrier and rider in turn in every spike and scenario.
- Conventions: Epic coding standard, native tags owned by the plugin, plugin usable alone (no `/Game/`, no project tags/ini), Blueprint
  extension classes with pure `BlueprintNativeEvent` hooks for designer logic, soft references to content, a human-readable README.

## 0. Ground rules for every checkpoint

Same as M6P §0, with the plugin configurations changed:

- Gate before each commit: full build (UBT, not Live Coding, for header/reflection changes) in **three configurations**: everything on;
  `GSTNPiggyback` off (must equal the M6 configuration: identical test counts to HANDOVER 62's `headless-f15-on.log` 1183 plus whatever
  G1-G3 add to `GSTNLink`/`GSTNCombat`); `GSTNLink` off (which forces `GSTNPiggyback` off: 1099 + G3's core tests). `Automation RunTests GSTN`
  green, every count measured from a named `-abslog` log (`Select-String` counts), never estimated.
- Harness scenarios named in the checkpoint clean at 60 ms / 15 ms jitter, with and without 2 % loss, once NP code is touched;
  SUITE_LINK and SUITE_DEFENCE still green with `GSTNPiggyback` on and off. Listen server (host + 2 clients) primary, dedicated + 2
  clients secondary. Frame-aligned presentation checks (HANDOVER 62 item 2: the shown frame is the record of N + 1).
- Simulation code has **no `IsServer()` and no topology branch**: the rider's optimistic accept and the server's resolution are the same
  code on different data (M7 §6.2).
- **No edit of shared test Blueprints** (`KIT_Test_*` of M3-M6): M7 adds its own `KIT_Test_Mount*` kits and data, as M6 F13 did.
- The main checkout on the desktop is never touched (the owner's uncommitted level); work in a worktree or on the laptop clone.
- Grep gate (CI script in `Tools/`, extended in G4): no include or module reference to `GSTNPiggyback*` from `GSTNCombat*`, `GSTNLink*`,
  `GSTNDefence*`, `GSTNInput*`, `GSTNInteract*`; no `/Game/` and no `ConstructorHelpers` in `Plugins/GSTNPiggyback/Source`.

## 1. What M6 already gives M7 (audit of `feature/gas-npp-5.8` at 9dcaeb2)

M6 was built generic enough that a mount is mostly **one more link type plus data**. Audited capabilities (**Y1..Y16**):

| Id | Capability M7 needs | M7 § | State on feature | Used by |
|---|---|---|---|---|
| Y1 | Link type as data (`UGSTN_LinkTypeDefinition`: channel, two `FGSTN_LinkRoleDef` with `bParked`, `bActsWhileLinked`, `bCollisionWithPartner`, `SocketOnPartner`, `SocketOffset`, `ReleaseBreakReason`; break rules; rules; exit placer; `ExitCollisionIgnoreFrames`; `ForbiddenWithBody`; `MaxFrames`) | 2b(1), 3 | **Present**. A mount is initiator = rider (parked, acts), target = carrier (acts): the latch's shape with an ally | G5 |
| Y2 | Registering a type and a rule from another plugin (`UGSTN_LinkRegistry::AddDefinition`, asset scan via `UGSTN_LinkSettings::LinkTypeDirectories`, `UGSTN_LinkRule` subclass, plugin break reasons from `EGSTN_LinkBreakReason::FirstPlugin`) | 2b, 8 | **Present**, proven by EXT | G4, G5 |
| Y3 | Link request through a hit and the `LinkRequest` step; `LinkResponse` and `LinkCommand` inbox kinds; serials; refusals (`LastRefusedSerial`, `RefusedFrame`) | 6.1, 6.2 | **Present** | G5 |
| Y4 | Initiator prediction: a hit marks the initiator `Pending` + `GSTNLinkFlags::Predicted`, cleared after `RequestTimeoutFrames` without response | 6.2 | **Partial**: the predicted state stays `Pending` until the response; M7's optimistic **accept at F+2** (rider attached on its own screen before the answer) is not built; `UGSTN_LinkTypeDefinition::bInitiatorPredicts` exists but nothing reads it | G1 |
| Y5 | Parked movement mode (`UGSTN_LinkedMovementMode`, `Combat.Mover.Linked`), snapshot substitution of the parked entity (`GSTN_LinkSnapshotExt`), action veto for roles that do not act | 5, 6.4 | **Present** | G5 |
| Y6 | Watchdog: break rules (distance, hits per role with `MinKnockbackToBreak`, deaths, partner state), commands to both sides on the same frame, exit placement (`UGSTN_LinkExitPlacer`, `IGSTN_ExitCollisionQuery`), exit request (`GSTN_LinkExitRequest`), exit collision window (F15) | 6.4, 8 | **Present** | G5, G7 |
| Y7 | Presentation: `UGSTN_LinkPresentationComponent` attaches the parked mesh to the partner's socket, `GetLinkedTransform()` for camera/aim, collision side from the simulated fragment | 5, 6.8 | **Present** (while Active; attach while `Pending` comes with G1) | G1, G10 |
| Y8 | Interact probe (**worse**, G0 S0: grab-only probe, fixed in G1): `UGSTN_LinkInteractProvider` offers link types as `FGSTN_LinkInteractOffer { Definition, MeaningTag, ReachCm }`, pre-filtered by the type's own request checks | 7 | **Present**. Mount = one more offer under `Interact.Target.Mount`; the probe's default priority already lists Mount last (INT.5) | G5 |
| Y9 | Link slots as data (`GSTN_LinkSlots`), grabber/latcher bindings by role | 7 | **Present**; M7 adds its slots in its own ini | G5 |
| Y10 | Hit `LinkFilter` (`Any`, `LinkedPartnerOnly`, `ExcludeLinkedPartner`) | 5 (`bAffectsLinkPartner`) | **Present**: replaces the spec's `bAffectsLinkPartner` flag; M7 sets `ExcludeLinkedPartner` on its moves and the hit step treats partners as excluded by default (G7) | G7 |
| Y11 | Weight and carry capacity (`UGSTN_CombatAttributeSet::Weight`, `MaxCarryWeight`) | 3, 4 | **Present** | G5, G6 |
| Y12 | Generic request kinds `EnterLinked`, `ExitLink`, `ApplyLinkEffect`, `RemoveLinkEffect`, `LaunchLinked` (`GSTN_CombatActionTypes.h`) | 5, 6.3 | **Partial**: the enum values exist, but no executor applies `ApplyLinkEffect`/`RemoveLinkEffect`/`LaunchLinked` (the M6 types never needed them) | G1 |
| Y13 | Link actions block `FGSTN_LinkActions::Active{Id, StartFrame}`, `PendingStart{Frame, Id}`, `Exit{...}`; `FGSTN_LinkState::Payload`, `LocalYawQ` | 10 | **Partial**: serialized, compared and printed, but **no writer**: no partner-move inbox kind, no `UGSTN_LinkActionProvider`, no facing written to `LocalYawQ` | G2 |
| Y14 | Credit to a partner in hit entries and `HitLanded`/`KO` events | 6.7 | **Missing** | G3 |
| Y15 | Pipeline steps added by a plugin (`UGSTN_HitResolutionStep`, inserted into a rules' `HitPipeline`; e.g. `UGSTN_HitStep_LinkBreakOnHit`, EXT's brace step) | 5, 6.7 | **Present** | G7 |
| Y16 | Kit extensions (`FGSTN_KitExtension::Compile`), loadout extension registry (`FGSTN_LoadoutExtensionRegistry`, M6 F6), rules extensions (`FGSTN_RulesExtension`) | 2b(2)(3) | **Present** | G5, G6 |

Consequence: three small **generic** follow-ups in `GSTNLink` (G1, G2) and one neutral field in `GSTNCombat` (G3) come first, in the M6
"follow-up" style (entry in the deltas file, no piggyback name in them, tests in the owning plugin). Everything else lives in
`GSTNPiggyback`.

## 2. Plugin layout (M7 §2b, adjusted to the shipped names)

| Module | Type | Contents | Depends on |
|---|---|---|---|
| `GSTNPiggybackCore` | Runtime | tags header, `FGSTN_CarrierProfile`, `FGSTN_RiderProfile`, `FGSTN_CarryTuning`, `FGSTN_MountRules`, `UGSTN_MountUtils`, pair-move enums | `GSTNCombatCore`, `GSTNLinkCore` |
| `GSTNPiggybackRuntime` | Runtime | `UGSTN_LinkRule_Mount`, the mount link type asset, abilities (`UGSTN_GA_Mount`, `_Dismount`, `_Drop`, `_PairMove`), `UGSTN_GE_CarryLoad` + `UGSTN_MMC_CarryLoad`, `UGSTN_PairMoveDefinition/Set/Registry`, `UGSTN_HitStep_MountHit`, kit extension, role anim-layer provider | `GSTNPiggybackCore`, `GSTNLinkGAS`, `GSTNLinkNP`, `GSTNCombatGAS`, `GSTNCombatNP`, `GSTNInputCore` |
| `GSTNPiggybackEditor` | Editor | pair-move factory and validators, `gstn.Combat.Debug.Anchor` panel | `GSTNPiggybackRuntime`, `GSTNCombatEditor` (optional) |
| `GSTNPiggybackTests` | Developer | Automation tests, harness content | all of the above |

Native tags are defined in a Runtime module (EXT lesson: an `UncookedOnly` module ensures). `GSTNPiggyback.uplugin` requires `GSTNLink`
and `GSTNCombat`; nothing requires `GSTNPiggyback`. The project integration module of M7 §2b (`GASP_Giustino2Piggyback`) is **not built**
unless G10 shows the camera or perception needs one (M6's `GetLinkedTransform()` already serves both through `GSTNLink`).

## 3. Checkpoints (in order)

### G0 Spikes (scratch code only; results in `docs/notes/m7-spike-report.md`)

- **S0 seam audit**: re-check the Y table on the HEAD the work starts from (the spin-drill thread may have moved `GSTNLink` or
  `GSTNCombatCore`); a row worse than written is a stop condition (section 4).
- **S1 anchor agreement** (M7 §2 spike 1, §2c): a scratch mount-like type formed by the `LinkCommand` Form debug command (no new code
  path), the rider parked on an ally fighter. Measure server vs client anchor of the rider (frame-aligned) at carrier speeds 0, walk, run,
  dash; 60/15 ms, 0 and 2 % loss; host as carrier, host as rider, client-client. The latch's F12-F15 measurements are the baseline. Verdict
  per pairing: rider-initiated exits and rider hitboxes **predicted** (p95 < 10 cm, no reconcile at `ShouldReconcile` tolerance) or
  **server-given** (M7 §6.4 fallback).
- **S2 presentation attach** (M7 §2 spike 2): mostly answered by M6 (mesh attach + `GetLinkedTransform()`); re-check only pops at link
  start/end under forced reconciles on an ally carrier that is *predicted* on its own client (the latch's partner was not a local player).
- S3 (optimistic mount cost) and S4 (partner-move delay) need G1/G2 code: they are measured in G5 and G9.
- Size: S.

### G1 `GSTNLink` follow-up 1 (generic): optimistic initiator accept, link-effect executor

- **Optimistic accept** (Y4): for a type with `bInitiatorPredicts`, the initiator's predicted `Pending` link becomes `Active` (flag
  `Predicted` kept) `PendingFrames` after the request unless the target's snapshot entry shows `LastRefusedSerial` = its serial; the real
  response then confirms (flag cleared) or the server's state rolls it back. New definition field `PendingFrames` (default 2). The
  presentation component attaches during `Pending` for such types (blend in, M7 §5) and detaches with a blend on refusal. Types without the
  flag (grab, latch) are unchanged: test that their state sequences are byte-identical to HANDOVER 62.
- **Link effects** (Y12): `UGSTN_LinkTypeDefinition` gains an optional per-role `LinkEffect` (soft `UGameplayEffect` class + SetByCaller
  tag); the link GAS executor applies it on `ApplyLinkEffect` (magnitude = `Payload` / 255) and removes it on `RemoveLinkEffect` by source
  effect (no stored handle: idempotent under resimulation), requested by the link tick when a role becomes Active and when it ends.
  `LaunchLinked` executes the exit's velocity as the existing knockback layered move. A rule may write the link's `Payload` on accept
  (`UGSTN_LinkRule::ComputePayload`, `BlueprintNativeEvent`, default 0).
- **Interact offers and request sides** (added after G0 S0, 2026-11-01, `docs/notes/m7-spike-report.md`): the link tick's in-simulation
  probe only offers the grab and its `Alternatives` under `Interact.Target.Grab`, and `FGSTN_LinkRequestSide` carries no team, no
  attributes and only typed grab/latch kit rules, so no other plugin can add a meaning or an ally-only rule. Fix, generic: interact offers
  registered per link type (meaning tag + priority, data on the type definition or the registry), the probe built from that list and its
  meaning table from the registered tags; `FGSTN_LinkRequestSide` gains the team id and the kit's extension list (rules read their own
  extension by type, as `FGSTN_LoadoutExtensionRegistry` does). Grab and latch keep their behaviour (SUITE_LINK gate). The drill's
  interact-formed latch uses the same seam (tell the spin-drill thread before and after).
- Link effects are applied by the link tick extension (as the exit GE is), since `GSTNCombatNP` has no executor seam for the five link
  request kinds; Y16's `FGSTN_KitExtension::Compile` is `FGSTN_LoadoutExtensionRegistry` in shipped code.
- Tests: `GSTNLink.NP.Predict.{OptimisticAcceptAtPendingFrames, RefusalRollsBackToIdle, ConfirmClearsFlag, GrabUnchanged}`,
  `GSTNLink.GAS.LinkEffect.{AppliedOnActive, RemovedOnEveryBreakReason, IdempotentOnResim, PayloadToMagnitude}`.
- Harness: SUITE_LINK unchanged (all 19), plus the EXT carry with `bInitiatorPredicts` on as a scratch scenario.
- Size: M.

### G2 `GSTNLink` follow-up 2 (generic): partner moves, parked facing

- **Partner moves** (Y13, M7 §6.5): `IGSTN_LinkActionProvider` (UInterface, registered per link type in the registry, implemented by
  M7's pair-move registry) resolves an `int16 Id` to the partner's ability and its rules. New inbox kind `PartnerMove { Serial, Id,
  TargetFrame }` inside `GSTNLink`'s range; the link tick writes `Actions.Active` on the initiator, drains the entry on the partner at
  F >= S + 1 (serial check, provider's partner requirements), schedules `Actions.PendingStart` or activates at once (forced activation,
  `ActiveSource = Forced`), and applies the provider's outcome at its frame (`Break` through the watchdog with the provider's exit, or
  `SwapRoles`, reserved). Failure paths (serial mismatch, partner KO, `MaxDurationFrames`) break with the default exit and post a
  generic `Combat.Event.Link.PartnerMoveAborted`.
- **Parked facing**: a parked role that acts (`bActsWhileLinked`) writes `LocalYawQ` from its aimed moves and, when idle, its stick within a
  per-role `YawLimitDeg` (new `FGSTN_LinkRoleDef` field, default 360); the snapshot substitution composes the yaw. The latch gets it for free
  (default keeps today's behaviour: test).
- Tests: `GSTNLink.NP.PartnerMove.{StartsAtTargetFrame_0_1_12, SerialMismatchDropped, OutcomeBreaksBothSides, RollbackTrimsEntries,
  AbortOnPartnerKO}`, `GSTNLink.NP.Facing.{StickWithinLimit, AimedMoveSetsYaw, LatchUnchanged}`, fragment round trips.
- Size: M.

### G3 `GSTNCombat` neutral follow-up: partner credit

- Hit entries and the authoritative `HitLanded`/`KO` outbox events gain an optional `FGSTN_EntityRef CreditPartner` and a quantised
  `PartnerShare` (uint8), filled by any pipeline step (none in core); the `MeterGain` request splits meter when a share is present. With no
  link plugin both stay empty and the event size is unchanged (byte test).
- Tests: `GSTNCombat.Hits.Credit.{EmptyByDefault, MeterSplit, EventCarriesPartner}`.
- Coordination: this is the only M7 edit in `GSTNCombatCore`; schedule it outside the spin-drill thread's D6/D7 window (section 7).
- Size: S.

### G4 `GSTNPiggyback` skeleton + `GSTNPiggybackCore` (pure)

- Four modules, `.uplugin`, README (purpose, modules, how to extend: M7 §14), native tags of M7 §11 that are piggyback's
  (`Combat.Link.Type.Mount`, `Combat.Link.Role.Carrier/Rider`, `Combat.HitType.Interaction.Mount`, `Combat.State.Riding/Carrying/MountPending`,
  `Combat.PairMove.ThrowRider/BoostJump`, `Combat.Event.Link.Mounted/MountRefused`, `Combat.Event.PairMove.Started/Aborted`, its break
  reasons `Dismount Drop Outcome CarrierHit RiderHit`, `Combat.Link.Credit.Partner`, `Combat.SetByCaller.CarryLoad.Ratio`,
  `Interact.Target.Mount`), the slot ini `Config/Tags/GSTNPiggybackCommands.ini` (`Command.Link.Mount/Dismount/Drop`). Generic reasons that
  `GSTNLink` already owns (KO, Removed, Timeout, Distance, Throw) are reused, never redeclared.
- Core types of M7 §3 and `UGSTN_MountUtils` (`EvaluateEligibility`, `ComputeCarryRatio`, `ComposeAnchor`, `ExitCandidates`,
  `SplitCredit`), doubles, no world.
- Grep gate script extended; plugin **disabled** in the committed `.uproject` until G5 passes (as `GSTNExtensionTest`).
- Tests: `GSTNPiggyback.Core.{Eligibility.EveryVerdict, Eligibility.WeightBoundary, CarryRatio.ClampAndQuantise, Anchor.Compose,
  Exit.CandidateOrder, Credit.Split, Tags.Registered}`.
- Size: M.

### G5 Mount, dismount, drop (`GSTNPiggybackRuntime` part 1) + spike S3

- `DA_LinkType_Mount` (in the plugin's Content): Body channel; initiator role Rider (`bParked`, `bActsWhileLinked`, no partner collision,
  socket from the carrier profile, else root + offset), target role Carrier (`bActsWhileLinked`); `bInitiatorPredicts`, `PendingFrames 2`;
  hit breaks from `FGSTN_MountRules` (carrier above `CarrierBreakKnockback`, rider above `RiderBreakKnockback`, partner hits never);
  `PartnerMustNot` = KO; `MaxDistanceCm` as a leash; exit placer subclass for `BehindCarrier / SideOfCarrier / AtAnchor`.
- `UGSTN_LinkRule_Mount` wraps `EvaluateEligibility` (ally only, kit `bCanBeMounted`/`bCanRide`, weight, cooldown, `Combat.Lock.Link`,
  carrier not in hitstun) and writes the carry ratio as `Payload` (G1).
- Interact offer `Interact.Target.Mount` (the provider's offer list gains the mount type with the carrier profile's reach); interact
  candidates are allies only for this offer.
- Abilities `UGSTN_GA_Mount` (interaction hitbox `Combat.HitType.Interaction.Mount`, Ally only, pick-nearest via the existing
  interaction-hit path), `UGSTN_GA_Dismount` (rider tap) and `UGSTN_GA_Drop` (carrier, Interact + crouch held), all Blueprintable with
  timeline content in data; bindings by role (M7 §7); `DisallowedCommands` compiled by the kit extension.
- Kit extension `FGSTN_PiggybackKitExtension` (carrier and rider profiles, pair-move set, anim layers), rules extension `FGSTN_MountRules`
  (absent = every mount refused).
- Content: `KIT_Test_Mount`, `KIT_Test_MountHeavy` (refusal), in the plugin's test content.
- Tests: `GSTNPiggyback.Runtime.{Rule.*, Bindings.RiderHasNoGroundContext, Bindings.DisallowedCommands, Break.EveryReasonClearsStates,
  Disabled.UnboundCommandIsLegal}`.
- Harness: `scenario_mount_basic`, `scenario_mount_refused` (exactly one rollback on the rider's client, carrier unaffected),
  `scenario_dismount_drop`, `scenario_mount_interact_priority` (enemy and ally in reach: grab wins, M7.1/INT.5); **S3 measured** here.
- Size: L.

### G6 Carry load

- `UGSTN_GE_CarryLoad` + `UGSTN_MMC_CarryLoad` (walk speed, acceleration, jump velocity × `1 − Penalty × Ratio`, air jumps reduced),
  applied through G1's link effect on the carrier; `Combat.State.Carrying` as its granted tag; Mover sees it through the stats modifier.
- Tests: MMC goldens at ratio 0, 0.5, 1; removed on every break reason.
- Harness: `scenario_mount_carry` (carrier runs, jumps, dashes for 3 s: no correction after the link phase; speed equal on server and
  client on the accept frame).
- Size: S.

### G7 Hits while mounted, credit

- `UGSTN_HitStep_MountHit` after `ComputeKnockback`: rider hitstun × `RiderHitstunScale`, knockback × `RiderKnockbackScale` below the
  break threshold; optional `CarrierCoverFrontDeg` and `bCarrierShieldCoversRider` (off by default); hits between partners excluded unless
  the hitbox says `LinkedPartnerOnly`; credit from the attacker's link and active pair move into G3's fields.
- Tests: step goldens per threshold side, cover on/off, partner exclusion with friendly fire on.
- Harness: `scenario_mounted_hit` (dummy hits carrier below/above threshold, then the rider), `scenario_rider_attack` (rider hits a dummy
  with its own stick; hit equal on server and client; anchor divergence logged against S1).
- Size: M.

### G8 Pair-move data

- `UGSTN_PairMoveDefinition` (PrimaryDataAsset `GSTN_PairMove`, M7 §4 fields), `UGSTN_PairMoveSet`, `UGSTN_PairMoveRegistry`
  (implements G2's `IGSTN_LinkActionProvider`, compiled with the loadout), `UGSTN_GA_PairMove`; `IsDataValid` rules of M7 §4.
- Tests: validation cases of M7 §9 (missing abilities, offset past the initiator, `OutcomeFrame` outside, roles), registry ordering.
- Size: S.

### G9 Reference pair moves + spike S4

- `PM_ThrowRider` (carrier initiator, rider thrown by the carrier's stick at release, sweeping hitbox while airborne) and `PM_BoostJump`
  (rider initiator, carrier braced, rider launched up with inherited velocity, air jumps refunded), as test content per M7 §6.6.
  Rider-initiated exit predicted or server-given per S1's verdict.
- Harness: `scenario_throw_rider`, `scenario_boost_jump`; **S4 measured**: median partner reaction delay on the partner's screen per
  pairing. Above 150 ms with presentation hiding on → stop and ask (forward prediction is a fork change, M7 §2 spike 4).
- Size: M.

### G10 Presentation, animation, camera

- Role anim layers through the GSTNAnimation pattern (abilities and role tags pick layers; skins via `Anim.Trait.*` rules; hitboxes never
  change), optional hand/foot IK targets on carrier sockets; refused/accepted cues; placeholder poses only (no aesthetics work before the
  mechanics are done, owner 2026-10-24).
- Camera: the rider's camera follows `GetLinkedTransform()` (the consumer change M6P section 6 approved for parked roles; no new shared
  Blueprint edit unless the camera needs a mount-specific rule, then ask).
- Depth marker: rider's marker hidden, carrier's shown (DM.7 assumption, section 5).
- Size: M.

### G11 Editor and debug

- `GSTNPiggybackEditor`: pair-move factory, validators, `gstn.Combat.Debug.Anchor 1` (server vs client anchor), link transition log
  line; `gstn.Combat.Debug.Links 1` already draws the link.
- Size: S.

### G12 Full sweep, byte table, docs

- `scenario_mount_forced_reconcile` (forced reconciles at request, Pending, first Active, pair-move start and outcome frames); every M7
  §9 scenario in listen (host as carrier and as rider) and dedicated, 0 and 2 %; SUITE_DEFAULT/PARTS/DEFENCE/LINK in the three plugin
  configurations; byte table (idle mount 0 B extra, live mount = one link channel); docs: combat.md (§3 modules, §12b, §17, roadmap),
  README "How to extend", testing.md, `m7-doc-deltas.md`, M7 §13 acceptance checklist.
- Size: M.

## 4. Stop and ask

- S0 shows a Y row worse than written (a needed seam is missing or hard-wired to a type tag).
- S1 fails for **every** pairing (no predicted rider exits at all): ask before shipping the fallback everywhere.
- S4 median above 150 ms (forward prediction decision, a fork change).
- Any change that would make `GSTNLink` or `GSTNCombat` name piggyback, or grab/latch behaviour change in SUITE_LINK.
- An edit to a shared Blueprint, the owner's level, the project camera beyond `GetLinkedTransform()`, or the skeleton assets (sockets).

## 5. Defaults for unanswered owner questions (all data; the owner can change them later)

| Q | Default |
|---|---|
| M7.2 | Rider's Jump = boost jump (pair move); plain dismount = Interact tap |
| M7.3 | Drop always available outside hitstun; no rider veto |
| M7.4 | `DisallowedCommands` per kit, empty in the test kit |
| M7.5 | Weapon switch allowed while riding; supers only through rider bindings |
| M7.6 | Optimistic 2-frame mount; refusal = one rollback hidden by a blend |
| M7.7 | Linear penalty on rider weight / `MaxCarryWeight`, per kit |
| M7.8 | Remount cooldown 30 frames (about 0.5 s at 62.5 Hz) |
| M7.9 | Break thresholds: carrier = tumble threshold, rider = twice that; below, the rider flinches and takes damage |
| M7.10 | No cover by default (arc 0, shield does not cover the rider); both options in the rules |
| M7.11 | Damage to the hitbox owner; meter 50/50 on the throw, owner only on the boost; partner recorded in events |
| M7.12 | Partners never hit each other unless the hitbox says `LinkedPartnerOnly` |
| M7.13 | Throw direction from the carrier's stick at release; fixed angle and speed per move; partial carrier velocity |
| M7.14 | Partner's part cooperative (cannot refuse), commands locked during it |
| M7.15 | Partner delay accepted; decided by S4 (150 ms) |
| M7.16 | Player allies with an NP model only; one rider, no chains |
| M7.17 | Wall and other modes allowed while carrying (`ForbiddenModes` per kit, empty) |
| M7.18 | Death, teleport or disconnect of the carrier breaks the link on the server; rider placed beside the carrier and regains control |
| M7.19 | Separate optional plugin, depends only on `GSTNLink`/`GSTNCombat` extension points (done this way) |
| DM.7 | Rider's depth marker hidden while mounted, carrier's shown |

## 6. Edits needing the owner's OK

None planned. Sockets on the test skeleton are avoided (root + offset); the camera uses M6's approved `GetLinkedTransform()` path.
If G10 needs a mount-specific camera rule or a skeleton socket, it asks first.

## 7. Coordination and machines

- **Spin and drill thread** (desktop, `GSTNSpinDrill`, SDP): its D6/D7 edit `GSTNCombatCore`; its D9/D10 add the drill latch type in its
  own plugin and only check `GSTNLink`. M7's G1/G2 edit `GSTNLink` and G3 edits `GSTNCombatCore`. Rule: G1/G2 tell that thread before and
  after (files: `GSTN_LinkTypeDefinition.h`, `GSTN_LinkTickExtension.*`, `GSTN_LinkInboxKinds.*`, link GAS executor,
  `GSTN_LinkPresentationComponent.*`); G3 runs only when D6/D7 are not open, or is rebased onto them. G4-G12 touch only
  `Plugins/GSTNPiggyback` plus the harness and are parallel-safe.
- **Machine**: one editor/build at a time per machine. With the desktop busy with spin and drill, M7 runs on the **laptop** clone in short
  Remote Control sessions (owner's choice, asked on 2026-11-01), branch `feature/gas-npp-5.8`, a HANDOVER per checkpoint in
  `docs/notes/takeover-state.md`.
- Delegation: each checkpoint is one RC session (Opus, effort sized to the checkpoint; pure-data subtasks on Sonnet), a fresh session per
  checkpoint to keep context small.

## 8. Risks

- The optimistic accept (G1) is the only new rollback path in `GSTNLink`; a bug there shows up in grabs too. Mitigation: the flag gates
  it per type, and SUITE_LINK byte-identical state sequences are a G1 gate.
- Partner moves (G2) add forced activations on the partner's server state: the partner's client learns them by correction; S4 decides
  whether that is acceptable.
- Spec drift: M7 was written against M6's spec names; `m7-doc-deltas.md` records each rename (e.g. `bAffectsLinkPartner` → `LinkFilter`,
  `UGSTN_LinkActionProvider` → `IGSTN_LinkActionProvider`, the request array of M7 §5 is already M6's).
