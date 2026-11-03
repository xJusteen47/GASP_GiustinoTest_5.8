# M7 doc deltas (piggyback and pair moves)

Corrections to the M7 plan (`docs/architecture/plans/m7-piggyback-plan.md`, branch `claude/project-thread-wzb12r`) and spec
(`docs/architecture/combat-m7-piggyback.md`) that the checkpoints found, collected per checkpoint, to be folded into `docs/` by G12. Where the
spec or plan and the shipped code differ, the shipped names win. Test counts come from named logs under `Saved/Logs/`.

## G0: seam audit corrections (HANDOVER 67, `docs/notes/m7-spike-report.md`)

- **Y4** (initiator prediction): the `Predicted` mark is put on the initiator of **every** registered link type's hit, not only grab-like ones;
  `bInitiatorPredicts` was read by nothing. G1 makes it gate the optimistic accept only (below).
- **Y12** (generic link request kinds): no executor exists for any of the five link kinds of `EGSTN_RequestType` (`EnterLinked`, `ExitLink`,
  `ApplyLinkEffect`, `RemoveLinkEffect`, `LaunchLinked`), and `GSTNCombatNP` has no seam that lets a plugin execute a request kind (the
  model's request switch ends in `default: break`). There is no "link GAS executor". G1 applies and removes link effects from the link tick
  extension, as the exit effect is applied; the request kinds stay unused.
- **Y16** (kit extensions): `FGSTN_KitExtension` is data only; there is no `FGSTN_KitExtension::Compile`. A plugin compiles kit data through
  `FGSTN_LoadoutExtensionRegistry::ContributeCompileInput`; since G1 a link rule reads its own kit data from the request side
  (`FGSTN_LinkRequestSide::FindKitExtension<T>()`).
- **Y8 and R1** (the two WORSE rows): fixed by G1 items A (interact offers) and the request side's team and kit array.

## G1: generic GSTNLink follow-ups (HANDOVER 69)

### Shipped names that differ from the plan and spec

| Plan / spec | Shipped |
|---|---|
| "interact offers registered per link type (meaning tag + priority, data on the type definition or the registry)" | Both: `UGSTN_LinkTypeDefinition::InteractOffer` (`FGSTN_LinkInteractOfferDef`: `Meaning`, `Priority` 0..255 higher first, `ReachCm`, `RequiredKitExtension`) and `UGSTN_LinkRegistry::RegisterInteractOffer(TypeTagName, Offer)` for a type whose asset names none (GSTNLink registers its grab this way: `Interact.Target.Grab`, 100, kits with `FGSTN_LinkKitExtension`). The registry compiles `GetInteractOffers()` (priority, then type tag name) and `GetInteractMeanings()` (one meaning per tag at its highest offer priority) |
| `MakeGrabOffers`, `GetLinkMeanings`, `ResolveGrabTarget` (tick extension) | `FGSTN_LinkInteractProvider::MakeOffers` (each registered offer preceded by its definition's `Alternatives`), `FGSTN_LinkTickExtension::MakeOffersFor`, `ResolveInteractTarget` (returns the projected meaning); the meaning table is the registry's. `FGSTN_LinkTickExtension::MakeRequestSide` is public (the debug probe uses it) |
| "`FGSTN_LinkRequestSide` gains the team id and the kit's extension list" | `TeamId` (uint8, 255 = no team, from the hit's `AttackerTeamId` / the victim view's `TeamId` on the hit path, `FGSTN_SnapshotEntry::Team` on the interact path), `KitExtensions` (`const FGSTN_KitExtensionList*`, from the body content of `FGSTN_LinkAttach`), `FindKitExtension<T>()`, `IsAllyOf()`. The typed `InitiatorGrabRules` / `InitiatorLatchRules` stay |
| ally-only interact candidates "for this offer" (G5) | Through a rule, not the offer: the generic `UGSTN_LinkRule_Team` (`Any`, `AlliesOnly`, `EnemiesOnly`, refusal `EGSTN_LinkRefusal::Team`) or `UGSTN_LinkRule::AreAllies` in the type's own rule. The probe's pre-filter runs the type's rules, so an ally-only type never offers an enemy |
| per-role `LinkEffect` "(soft GE class + SetByCaller tag)" | `FGSTN_LinkRoleDef::LinkEffect` (`FGSTN_LinkEffectDef`: `Effect` as an `FSoftClassPath` with `MetaClass` GameplayEffect, because `GSTNLinkCore` has no ability-system dependency; `SetByCallerTag`). Applied and removed by `FGSTN_LinkEffects` from the link tick extension at `BeforeTagProjection` |
| tests `GSTNLink.GAS.LinkEffect.*` | `GSTNLink.NP.LinkEffect.*` (the effects live in the NP module that runs them) |
| `UGSTN_LinkRule::ComputePayload` writes Payload "on accept" | The first non-zero `ComputePayload` of the definition's rules, in order (`UGSTN_LinkRequirementLibrary::ComputePayload`); carried by `FGSTN_LinkHitOutcome::Payload` and `FGSTN_LinkResponseEntry::Payload`, so both sides hold it |
| `LaunchLinked` executes the exit's velocity | Not built: deferred to G9 (the exit effect already carries a velocity; nothing in G1 needed a launch of the linked side) |
| presentation "attaches during Pending (blend in) and detaches with a blend on refusal" | Attaches during Pending for a predicting type whose parked role is the initiator; no blend (aesthetics later, G10) |

### Optimistic accept: how refusal is seen (correction of the plan's mechanism)

The plan reads the refusal from "the target's snapshot entry showing `LastRefusedSerial` = its serial". That cannot work as written: the link
snapshot extension is full (32 B) and carries no `LastRefusedSerial`, and a refused request never reaches the target's state (the
`LinkRequest` step makes a refused hit not land, and the combat victim step runs no outcome applier and posts no outgoing entry for a hit that
did not land, a `GSTNCombatNP` rule G1 does not change). Shipped (`FGSTN_LinkTickExtension::StepOptimisticAccept`):

- At `LinkFrame + PendingFrames` the initiator reads the target's link snapshot extension (end of the previous frame). Linked to someone else on
  that channel: cleared, as a refusal would. Holding role 2 with this entity (a machine that simulates the target): Active with the target's
  serial and link frame. Idle (the usual case on a client, where the target is an interpolated proxy): Active with the serial the target will
  form (`NextSerial(target, own)`) and link frame request + 1. The `Payload` and the formed type (an `Alternative`) were predicted at the hit
  (`PredictRequest`, the same request context on the same snapshot).
- On a machine that simulates the target (the authority always), the response is drained before `AfterClocks` of F+2, so with `PendingFrames`
  2 the link is already confirmed there and the optimistic path does not run.
- The response confirms an optimistic accept (it counts as the initiator's own request: flag cleared, serial and payload corrected).
  Elsewhere the flag never clears by itself; `FGSTN_LinkFragment::ShouldReconcile` treats a local Active role-1 link with the flag as equal to
  the authority's same link without it, so an accepted mount costs no rollback.
- A refusal the initiator could not see (the target refused on a disagreement, or the authority's attacker run refused) is the authority's
  idle state: one rollback on the initiator's client. On the authority itself, the one case left is an attacker run that accepts and a target
  step one frame later that refuses (its state changed in between): the authority's initiator becomes a one-sided optimistic link, which the
  watchdog breaks as an orphan after `OrphanGraceFrames` (its exit is its own capsule). Rare; not handled further.
- Hand-channel types: the snapshot has no Hand serial or partner, so a held Hand link of the target counts as a refusal and the predicted serial
  is `NextSerial(0, own)` (it may mispredict once).

### S2 findings A and B (presentation)

Both come from Mover writing its primary visual component every frame before the actors tick (`UMoverComponent::FinalizeFrame` resets the
relative transform to `BaseVisualComponentTransform`; `FinalizeSmoothingFrame` in `VisualComponentOffset` mode sets the world transform from the
owner's own sync state). `UGSTN_LinkPresentationComponent` attached the mesh once: the next frame Mover dropped the role offset (A: the fixed
second step equals the role's `SocketOffset`, 74 cm for the test latch's offset applied to the rider), and on the rider's own client Mover's
smoothing put the mesh back on its parked capsule (B). Fix: the attachment and the relative transform are asserted on every tick (the
component ticks in `TG_PostPhysics`, after both), and the mesh's original offset is Mover's base transform. Not re-measured: the S2 mirror tooling was throwaway, and
the one-session probe of G1 (`Saved/Logs/m7-g1/probe-b.log`) formed the test latch without the scenarios' link setup, so the pawns had no
presentation component and it says nothing about the fix. OPEN for G10 (measure the parked mesh on its own client, rendering on).

## G2: partner moves and parked facing (HANDOVER 76)

### Shipped names that differ from the plan and spec

| Plan / spec | Shipped |
|---|---|
| `UGSTN_LinkActionProvider` "resolves an int16 Id to the partner's ability and its rules" | `IGSTN_LinkActionProvider` (UInterface in `GSTNLinkCore`, C++ only: it hands out class and placer pointers) with `ResolveLinkAction(int16 Id, FGSTN_LinkActionInfo&)`. `FGSTN_LinkActionInfo` (plain struct): `PartnerAbility`, `PartnerRequirements` (`FGSTN_TagRequirements`), `PartnerStartOffsetFrames`, `OutcomeFrame` (initiator timeline frame), `Outcome` (`EGSTN_LinkActionOutcome`: `KeepLink`, `Break`, `SwapRoles` reserved), `BreakReason` (tag, empty = Release), `ExitPlacer` (null = the type's), `bPartnerLocksCommands` (M7's `PartnerMode = Cooperative`), `MaxDurationFrames`. `UGSTN_LinkActionTable`: a provider filled by code |
| "registered per link type in the registry" | `UGSTN_LinkRegistry::RegisterActionProvider(LinkType tag, UObject*)` / `UnregisterActionProvider` / `GetActionProvider(TypeIndex)` / `ResolveLinkAction(TypeIndex, Id, Info)`; keyed by tag so a table rebuild keeps it; not in the registry signature (content the provider's plugin owns) |
| spec 6.5 item 2 "it writes `ActivePair = { PairIdx, S }` and a link-inbox entry `{ PartnerMove, Frame = S, PairIdx, LinkSerial, T }`" | How the initiator posts: a **link outcome key** `EGSTN_LinkOutcome::PartnerMove` with `PartnerMoveId` on the initiator's move (the F12 track kind, evaluated in the combat tick, so deterministic and re-posted by a resimulation). The link tick writes `FGSTN_LinkActions::Active = { Id, S, Side = Initiator }` (`Side` is new, `GSTNLinkActionSide`) and posts `FGSTN_LinkPartnerMoveEntry { Serial, Id, TargetFrame = S + max(PartnerStartOffsetFrames, 1) }` as inbox kind `GSTNLinkInboxKinds::PartnerMove` (FirstLink + 2), stamped S, through `UGSTN_CombatWorldSubsystem::PostToInbox` (refused where the partner does not tick, as every inbox post). No API an ability calls: an ability cannot write the link fragment |
| spec 6.5 item 3 "request the partner ability (activation like a forced command)" | The inbox handler only schedules `Actions.PendingStart = { max(T, S + 1), Id }`; the link tick at AfterClocks of that frame starts it through `IGSTN_CombatTickServices::StartForcedTimeline` (source Forced, M6 F8's seam) and writes `Active = { Id, F, Partner }`. The command lock is `BlocksActions` while the partner's part plays and the action says `bPartnerLocksCommands`; it lifts when the forced timeline is replaced or over |
| partner requirements "checked against the partner's projected tags in the snapshot" | The partner's own snapshot entry's request tags (`FGSTN_LinkTickExtension::MakeRequestSide`). That mask holds only the tags of `FGSTN_LinkRequestTags` (the definitions' request requirements and the grab/latch rules): a tag only a provider names reads as absent (the F10 deviation of kit rules). OPEN for G8 |
| spec 6.5 item 4 "the initiator writes `Link.Phase = Exiting` and posts Break (or Swap) to the partner" | Through the watchdog, the only break authority: at the outcome frame (initiator's timeline, same run, hitstop-aware via `FGSTN_LinkOutcomeKeys::GetCrossedFrames`) the initiator sets `PendingBreak` with the action's reason; the record stays until the break, and the watchdog places the parked side with the action's `ExitPlacer` (`FGSTN_LinkWatchdogEntity::ActiveMove`, `FParams::ActionExitPlacer`, `PlaceExit(..., Placer)`). `LinkCommand{Break}` now also clears both sides' `Active` and `PendingStart`. `SwapRoles`: logged once, treated as KeepLink |
| failure paths "serial mismatch, partner KO, MaxDurationFrames -> Break with the default exit; both sides emit PairMoveAborted" | Partner KO (at its start or while it plays) and `MaxDurationFrames` (either side): the record is cleared first (so the watchdog uses the type's default placer), `PendingBreak` with `Forced`, event `Combat.Event.Link.PartnerMoveAborted` (native, GSTNLink; Magnitude = the id) from the side that aborts, through the combat outbox (authority only). **Serial mismatch** follows spec item 3, not the brief: the entry is dropped (no link of that serial exists on the partner to break; the initiator's own link breaks as an orphan if it really is one), no event (the inbox handler has no outbox). Requirements failing, an unknown id, a partner in a hit reaction or a kit without the ability: the part is dropped with the event, the link kept |
| `Actions.PendingStart` "or activates at once" when T <= F | Always through PendingStart: the handler runs before AfterClocks of the same frame, so a due part starts in the frame it is drained, which is the same thing |
| Partner moves on any channel | Body channel only (the outcome keys act on the Body link) |

### Parked facing

- `FGSTN_LinkRoleDef::YawLimitDeg` (360) is the **whole arc** centred on the placement's forward (360 = free, 90 = +-45 degrees, 0 = fixed).
  `StepFacing` runs at AfterExecute (after step 8, so a move that started this frame is known): on the start frame of a move (an "aimed move":
  `ActiveTimeline` or `TimelineStartFrame` changed, or a timeline began) and on every idle frame (`Actionable`), `LocalYawQ` faces the frame's
  stick on the plane relative to the parked placement from the step snapshot; a stick below a quarter tilt keeps the turn. Only for the Body
  link's parked role with `bActsWhileLinked`.
- `LocalYawQ` is written in quanta of 128 (1/256 turn, about 1.4 degrees), so the snapshot carries it exactly in one signed byte:
  `FGSTN_LinkSnapshotExt` (full at 32 B) narrowed its type word to one byte (`Magic` 0x4C) and took the other for `LocalYawQ8`.
  `FinalizeSnapshot` composes the turn (`UGSTN_LinkMath::ComposeLocalYaw`); `UGSTN_LinkPresentationComponent` composes it into its role offset.
  Not in the watchdog's distance (a turn about the parked root does not move it).
- **The latch at its defaults**: 360 means a latcher that pushes the stick while idle, or at the start of its attack, now turns; with no
  stick it is byte-identical (`GSTNLink.NP.Facing.LatchUnchanged`). A latch that must keep facing the enemy sets its latcher role's
  `YawLimitDeg` to 0 (data). SUITE_LINK's latch scenarios are the check on the shipped test content.

### Counts (laptop, `Saved/m7-g2`, `Select-String 'Test Completed. Result=\{Success\}'` / `\{Fail`)

- `headless-g2-on.log` 1212/0 = HANDOVER 69's 1199 + 4 GSTNInput tests of D3b (HANDOVER 70, on the feature branch since 2d871d2) + 9 new
  `GSTNLink.NP` (PartnerMove 5, Facing 3, Fragment 1). `headless-g2-linkoff.log` 1107/0 = 1103 + 4. `headless-g2-ext-2.log` 1222/0 = 1212 + 10
  `GSTNExtTest` (9 + `Link.PartnerMoveFromOutside`).
- `GSTNLink.Core.Tags.AllRegisteredAndDocumented` now counts 40 native tags (`Combat.Event.Link.PartnerMoveAborted`), and the owned-subtree check
  admits `Combat.Event.Link.*`.
- SUITE_LINK, listen host + 2 clients in one editor process, 60/15 ms: `Saved/Logs/m7-g2/link-listen-0pct.log` 19/19 PASS (latch scenarios included).
- SUITE_LINK at 2 % loss: `link-listen-2pct.log` 17/19; the two FAILs were `link_spike_client_grabs_host` (one observer correction while the link is held) and `link_spike_client_grabs_client` (3 counter steps in a forced window, limit 2), scenarios that run none of the G2 paths (a debug-formed grab; no partner move, a non-acting parked role). Rerun of both twice: `link-listen-2pct-rerun.log` 4/4 PASS.

## G4: GSTNPiggyback plugin skeleton and pure core (HANDOVER 77)

### Shipped names that differ from the plan and spec

| Plan / spec | Shipped |
|---|---|
| `GSTNPiggybackTests` module type "Developer" | `DeveloperTool` (the engine's current name for a module that never ships; "Developer" is its deprecated alias) |
| `.uplugin` "requires `GSTNLink` and `GSTNCombat`" | Also lists `GSTNInput`, `GSTNCombatGAS` and `GSTNCombatNP`: the Runtime module's plan dependencies (`GSTNInputCore`, `GSTNCombatGAS`, `GSTNCombatNP`) live in those plugins, and UBT warns when a module uses a plugin its descriptor does not list |
| `CarrierProfile`, `RiderProfile` "live on the primary kit (`UGSTN_PrimaryKitDefinition`)" | `FGSTN_MountKitExtension : FGSTN_KitExtension { Carrier, Rider }`, an entry of the kit's extension array (the M6 way; the kit asset never names piggyback). A kit without it can neither carry nor ride; inside it `bCanBeMounted` and `bCanRide` default true. G5's rule reads it with `FGSTN_LinkRequestSide::FindKitExtension<FGSTN_MountKitExtension>()` |
| `FGSTN_CarrierProfile` fields | Spec fields plus `ExitDistance` (90 cm, the ring of `ExitCandidates`). `MountMaxDistance` and `ExitDistance` are cm floats; `AnchorYawLimitDeg` on the rider profile is GSTNLink's whole-arc `YawLimitDeg` meaning (G2) |
| `FGSTN_MountRules` "as an `FGSTN_RulesExtension`" | Derives from `FGSTN_RulesExtension` directly (data inline, not a soft pointer to a rules asset as defence does). Defaults: carrier break 1800 cm/s (the reaction tuning's `TumbleThreshold`), rider 3600, `MaxCarrierTeleportCm` 500, `DefaultCredit` OwnerOnly |
| `EvaluateEligibility` "input: both entities' snapshot entries" | `FGSTN_MountEligibilityInput`: plain values (bools, weights, frame, distance, range, cooldown end frame), filled by G5 from the snapshot, so the Core module needs no NP type. Check order (documented, tested): carrier kit, rider kit, already linked, cooldown, hitstun, busy/lock, range, weight. The enum keeps the spec's order (the verdict is a byte on the wire). `bAllyOnly` is not an eligibility check: G5 lists GSTNLink's generic `UGSTN_LinkRule_Team` (AlliesOnly) on the mount type, as G1 provided |
| `ComputeCarryRatio -> uint8` | Clamped, round half up (`floor(x * 255 + 0.5)`); zero or negative capacity: any weight is 255; NaN is 0. Helpers `CarryRatioToUnit`, `ScaleCarryTuning` (air jumps in integer math) |
| `ExitCandidates(Anchor, CarrierFacing, Mode)` | `ExitCandidates(Anchor, CarrierTransform, Mode, DistanceCm)`: the whole transform (its up axis keeps the candidates on the carrier's plane under any gravity). Orders: BehindCarrier back, left, right, front; SideOfCarrier right, left, back, front; AtAnchor anchor, then BehindCarrier's |
| `SplitCredit(Share, Amount)` | `SplitCredit(uint8 PartnerShare, double Amount) -> FGSTN_CreditSplit { OwnerAmount, PartnerAmount }`, the partner's part is Amount * Share / 255; `MakePartnerShare(0.5)` = 128. G3's shipped `PartnerShare` must use the same 0..255 scale, or G7 converts |
| pair-move enum `EGSTN_LinkOutcome { Keep, Break, SwapRoles }` | Not declared: GSTNLink's `EGSTN_LinkActionOutcome { KeepLink, Break, SwapRoles }` (G2) is the one field; `EGSTN_LinkOutcome` is also already a GSTNLink enum (outcome keys), so the name would clash. `EGSTN_PairRole`, `EGSTN_PairCredit`, `EGSTN_PairPartnerMode` shipped as in the spec |
| tags: spec 11's own break reasons include `KO Removed Teleport` | Plan G4's list (`Dismount Drop Outcome CarrierHit RiderHit`): KO is GSTNLink's `OwnDeath`/`PartnerDeath`, and a teleport break uses `Distance` (no `Teleport` tag; add one in G5 only if the watchdog needs to tell them apart). 21 native tags; the three command slots in `Config/Tags/GSTNPiggybackCommands.ini`, registered by `GSTNPiggybackCore` at startup (`AddTagIniSearchPath`, as GSTNLink does) |
| `Combat.Event.Link.Mounted`/`.MountRefused` | Under `Combat.Event.Link.*`, the subtree GSTNLink's tag test admits for itself (G2). Not a conflict (that test checks only GSTNLink's own list), but G12 should say in combat.md that the subtree is shared |
| grep gate "extend the M6 script in `Tools/`" | There was none (M6's gate was run by hand, HANDOVER 61). New `Tools/Modules/check_modules.py` (plain Python): nothing outside the plugin names `GSTNPiggyback` (code, Build.cs, .uplugin, config, `.uproject`; `--allow-uproject` for a local enabled build), no `/Game/` or `ConstructorHelpers` in the plugin source, and the M6 module edges (Link vs Defence, the two ExtTest modules, Core without GAS/NP) |
| Tests in the module that owns the code (M6 habit) | In `GSTNPiggybackTests` (plan section 2), names `GSTNPiggyback.Core.*` as the plan lists |

### Counts (laptop worktree, `Saved/m7-g4`, `Select-String 'Test Completed. Result=\{Success\}'` / `\{Fail`)

- Configurations: "off" = `Saved/m7-g2/uproject.on` (GSTNLink and GSTNDefence on, HANDOVER 76's config, SHA256 DB15DE2B...), "on" = off plus `GSTNPiggyback`.
- `headless-g4-on.log` 1219/0 = 1212 + 7 `GSTNPiggyback.Core.*`; no ensure. `headless-g4-off.log` 1212/0 = HANDOVER 76's `headless-g2-on.log`; the log never names the plugin.
- Builds `build-g4-on.out` (the four plugin DLLs built, no warning from plugin files) and `build-g4-off.out` (makefile regenerated for the changed `.uproject`, nothing to recompile) both "Result: Succeeded".
- `python Tools/Modules/check_modules.py` on the committed tree: 0 problems. The committed `.uproject` was restored byte-identical (SHA256 9C4F29DE...) and does not list the plugin.

## G5: mount, dismount and drop (HANDOVER 78)

### Seam added to GSTNCombat (approved by the lead through the coordinator, 2026-11-01, option A)

The plan's mount is a hit on an ALLY, and GSTNCombat dropped every ally hit before the link step unless the match rules list Ally or turn on
friendly fire (attacker step and victim TeamFilter step; a hitbox variant could only ignore a relationship). Commit 307ca14
"GSTNCombat: per-relationship hittable override on hitbox variants": `FGSTN_HitVariant::bForceHittable` (off by default; `bIgnore` wins), read by
the attacker step (skips the rules' gate for that variant) and carried to the victim in bit 0x80 of the inbox entry's relationship byte
(`GSTNHitRelationship::ForcedBit`, `IsForced`; never on 255; `FromIndex` masks it), where `FGSTN_ResolvedHit::bRelationshipForced` lets the
TeamFilter step pass it. Tests `GSTNCombat.NP.Hits.Attacker.ForceHittableVariant`, `GSTNCombat.NP.Hits.TeamFilter.ForcedRelationship`.
Hunks in `GSTN_CombatTick_Attacker.cpp` (for the spin thread's D6/D7 merge): FromIndex switch (line 15), `FAttackerCandidate::bRelationshipForced`
(61), the relationship gate (219-221), the candidate's flag (279), the predicted pipeline's `Incoming.bRelationshipForced` (338), the entry's
forced bit (413-416).

### Shipped names that differ from the plan and spec

| Plan / spec | Shipped |
|---|---|
| `DA_LinkType_Mount` "in the plugin's Content" | Made by code: `FGSTN_MountLinkType::Build(Outer, FGSTN_MountRules)` and `Register()` (GSTNLink's `AddDefinition`, at PostEngineInit), as GSTNExtensionTest does. An asset would need the editor to author its instanced rule and placer, and the definition is fully decided by the rules struct and four constants (`RiderOffset` (-20, 0, 95) cm on the carrier's root, offer priority 50, reach 150 cm, leash 300 cm) |
| rider socket "from the carrier profile, else root + offset" | Root + `RiderOffset()` always: the definition is one per type and the request side carries no carrier profile to the placement. `MountSocket` / `MountOffset` of the profile are unread (OPEN, needs a per-entity placement seam in GSTNLink) |
| `PartnerMustNot` = KO | Empty: no KO state tag exists; deaths break through `bBreakOnOwnDeath` / `bBreakOnPartnerDeath` (GSTNLink's `OwnDeath` / `PartnerDeath`) |
| hit breaks "carrier above CarrierBreakKnockback, rider above RiderBreakKnockback" | `FGSTN_RoleHitBreak` per role from the rules' thresholds, `bPartnerHitsBreak` false; the reason is GSTNLink's `Hit` until G7's mount hit step names `CarrierHit` / `RiderHit` |
| `UGSTN_LinkRule_Mount` "weight, cooldown, Combat.Lock.Link, carrier not in hitstun, range" from the snapshot | From the request context only (a rule reads nothing else). The context carries neither the combat attributes `Weight` / `MaxCarryWeight` (the initiator side has no weight at all on the hit path) nor a position: the weights are kit data, `FGSTN_CarrierProfile::CarryCapacity` (150) and `FGSTN_RiderProfile::Weight` (100), new fields; range is the mount hitbox and the offer's reach (the rule's range check runs with distance 0); the cooldown is the rider's generic link `CooldownEndFrame`, which nothing writes after a mount (no remount cooldown yet, OPEN); hitstun is `Combat.State.Hitstun`, named in the type's `RequestRequirements` so the snapshot's request tag mask carries it (with the plugin on, the request tag table gains that tag) |
| `FGSTN_MountRules` "absent = every mount refused" | Not reachable from a rule (no world, no rules asset in the request context): the type is built from the default `FGSTN_MountRules`. OPEN: a request-context seam for the match's rules extensions |
| exit placer for `BehindCarrier / SideOfCarrier / AtAnchor` "per the carrier profile's DefaultExit" | `UGSTN_Placer_Mount { Mode, DistanceCm }` on the definition (BehindCarrier, 90 cm): the exit query carries no kit, so the profile's `DefaultExit` / `ExitDistance` are unread (OPEN). The anchor is the query's `SocketTransform` |
| `bInteractionPickNearest` on the hitbox spec | Not added (a GSTNCombat field): the mount hitbox has `MaxTargetsPerGroup` 1 and the interact probe already chose one focus |
| "interact candidates are allies only for this offer" | The type lists GSTNLink's `UGSTN_LinkRule_Team` (AlliesOnly) when `FGSTN_MountRules::bAllyOnly`; the probe's pre-filter runs it (G1) |
| bindings "by role (M7 7)", `DisallowedCommands` "compiled by the kit extension" | `FGSTN_MountLoadout`, a GSTNCombatGAS loadout extension (`GSTNPiggyback.Mount`): Mount, Dismount (role slot offset 100) and Drop (101, Crouch held), each with its own slot override that falls through when unavailable; `FGSTN_MountKitExtension::bAddDefaultBindings` and ability overrides (`MountAbility`, `DismountAbility`, `DropAbility`, empty = the native classes). `DisallowedCommands` add `Combat.Link.Role.Carrier` to the ignored tags of every kit and weapon binding of those commands that does not require the role |
| dismount / drop "issue the unlink" | A Release key of a link outcome track; the role's `ReleaseBreakReason` names `Dismount` (rider) or `Drop` (carrier). The plugin's five reasons are registered by name at module startup |
| rider exits "predicted by the rider's client" (spec 6.4) | SERVER-GIVEN in G5 (the watchdog on the authority places every exit). `FGSTN_MountRules::bPredictRiderExits` (default false) is the switch; true is logged and treated as false. The owner decided on 2026-11-01 (after S1) that a client rider's own exits are PREDICTED with the server rewinding the carrier to the rider's view time: first G6 item if not done here |
| test kits "in the plugin's test content" | `/GSTNPiggyback/Test/KIT_Test_Mount` and `KIT_Test_MountHeavy`, copies of the project's `/Game/GASNPPTest/Combat/KIT_Test_Link` (they reference its moves and slot set: test content only, nothing in the plugin's code loads them) with a mount entry written by `UGSTN_PiggybackContentLibrary::WriteMountKitData`; the plugin adds `/GSTNPiggyback` to the asset manager's `GSTN_PrimaryKit` scan |
| `scenario_mount_refused` with `KIT_Test_MountHeavy` | Kit data is the same on every machine, so a heavy rider is refused on its own screen (the probe offers no mount, no request): that is `scenario_mount_heavy_refused` (0 rollbacks). The rollback case needs a refusal the rider cannot see: `scenario_mount_refused` links the carrier on the server (a debug-formed grab by the third pawn) on the tick the rider taps |
| mount hitbox "active window" | One active frame (3): with a longer window the rider's client, restored by its rollback after a refusal, re-requested the carrier on the next active frame |
| bindings "Interact = Mount / Dismount / Drop" | Slot offsets -1 (Mount, below the grab's 0), 101 (Dismount, above the Release's 100) and 102 (Drop): the loadout validator refuses two slots with one input at one priority whatever their requirements. Mount also ignores `Interact.Target.Grab`, Dismount ignores the grabber, latcher and latched roles |

### GSTNLink changes G5 needed (approved by the lead through the coordinator)

- Commit "GSTNLink test: ReasonTableAppend relative to reasons other plugins registered" (bb2c516): the test assumed no plugin had registered a
  break reason; it now compares with a baseline build. Test only. The spin thread's drill (D9/D10) registers reasons too and needs it.
- The interact probe tries offers by priority and the first whose rules accept a candidate names it: the grab (100, no team rule) claimed allies,
  so the mount's offer (50) was never reached. `Tools/GAS/setup_defence_link_content.py` now gives `DA_LinkType_Grab_Test` and
  `DA_LinkType_Latch_Test` GSTNLink's `UGSTN_LinkRule_Team` (EnemiesOnly) as their SECOND rule (the harness edits the type's own rule as
  `rules[0]`); GSTNLink's README says a grab-like type should list a team rule. SUITE_LINK at 0 % is the check that grabs and latches of enemies
  are unchanged. OPEN (option B, not built): the probe could also skip relationships the match rules make unhittable, so a type without a team rule
  cannot shadow a lower offer. **Built in M7-02** as data, not from the match rules (the probe has no rules asset): `FGSTN_LinkInteractOfferDef::Relationships` (empty = every relationship, so existing offers are unchanged), the relationship from `UGSTN_RelationshipResolver::DefaultRule` on the two teams (a custom resolver of the match is not consulted); GSTNLink registers its grab offer for Enemy and Neutral, the mount type lists Ally. Test `GSTNLink.GAS.Interact.RelationshipFilter`.

### S3 (spike report section S3)

Accepted mount: 0 rollbacks on the rider's client, 1 correction on the carrier's. Unseen refusal: 3 rollbacks on the rider's client (not 1 as G1's
notes assumed) and a 117 cm visible snap for 3-4 ticks, at 0 % and 2 %. `scenario_mount_refused` records the count and accepts 1 to 4.

### OPEN after G5

1. **Predicted rider exits** (owner decision 2026-11-01: a client rider's own exits and hitboxes are predicted from its interpolated anchor, the
   server rewinds the carrier to the rider's view time): not built; `bPredictRiderExits` is false and logs if set. Needs a GSTNLink seam (a
   Release of a parked acting role that places itself on its own machine instead of waiting for the watchdog's command) and the rewind on the
   authority. First item of G6. Today's server-given dismount costs the rider's client 3 rollbacks and ends 0 cm from the authority.
2. Unseen refusal = 3 rollbacks (S3): a refusal memory on the rider, or no optimistic attach until the carrier's copy is newer than the request.
3. Remount cooldown: nothing writes the rider's link cooldown after a mount ends (no generic seam to set `CooldownEndFrame` on a break).
4. Per-carrier data the request context and the exit query do not carry: socket and offset, exit mode and distance, the combat attributes
   `Weight` / `MaxCarryWeight`, the match's `FGSTN_MountRules`.
5. The committed `.uproject` still does not list GSTNPiggyback (the brief: restore it byte-identical); enabling it in the project is the owner's call.

## G3: partner credit in GSTNCombat (M7-03)

### Shipped names that differ from the plan

| Plan | Shipped |
|---|---|
| "hit entries and the `HitLanded`/`KO` events gain an optional `FGSTN_EntityRef CreditPartner` and a quantised `PartnerShare`" | `FGSTN_HitCredit { int32 PartnerEntityId = INDEX_NONE; uint8 PartnerShare; }` in `FGSTN_HitOutcome::Credit` and `FGSTN_StepResult::Credit` (core). An entity id, not an `FGSTN_EntityRef`: the pipeline already names the attacker by its combat entity id (`AttackerEntityId`), and a partner is always a fighter. The **inbox entry is not changed** (no wire change): the step that credits reads the attacker's link state from the attacker's snapshot extensions (`FGSTN_ResolvedHit::AttackerExtensions`), on the victim's run and the attacker's prediction alike. Merge: a set credit replaces the outcome's (later step wins), an unset one changes nothing |
| `PartnerShare` scale | 0..255, the scale of the piggyback plugin's `SplitCredit` (`FGSTN_HitCredit::PartnerPart(Amount, Share) = Amount * Share / 255`); the attacker keeps the rest, so the two always add up |
| "the `MeterGain` request splits meter" | `UGSTN_HitStep_MeterGain` writes the attacker's part as before (`MeterGains` + an `Attacker` request) and the partner's part as a request with the new `EGSTN_HitRequestTarget::Partner`. The attacker's prediction sums only `Attacker` requests, so its own meter is the rest with no NP change. **Nobody applies `Partner` requests yet**: the partner's meter lives in the partner's simulation, so the owner of the partner (G7, the piggyback hit step, or a link inbox message) applies it. A credit step must run before MeterGain |
| "the event carries the partner" (`FGSTN_SimEvent` has no field for it; it is GSTNInput's) | A credited hit adds a **copy** of `HitLanded` right after the attacker's, instigated by the partner's actor and tagged `Combat.Event.Hit.CreditPartner` (new native tag of GSTNCombatNP); a credited Health KO adds the same copy of `KO` (a zone KO credits nobody). `UGSTN_CombatSimComponent::FindCreditPartnerActor` resolves the id through the world subsystem. Without a credit the events are exactly the pre-G3 ones (test), so "event size unchanged" holds as "event list unchanged" |
| Tests `GSTNCombat.Hits.Credit.{EmptyByDefault, MeterSplit, EventCarriesPartner}` | As planned, in `GSTNCombatNP/.../Tests/GSTN_CombatNPTests_Credit.cpp` (the events live in the NP module) |

## G6 item 1: predicted rider exits, design of the seam (M7-03; built in M7-04, deltas at the end of this section)

Owner decision S1 (2026-11-01): the rider predicts its own exit, the server rewinds the carrier so it places the same exit. Design for the next
thread (two edits, in this order, each its own commit):

1. **GSTNCombatNP (generic): the entity's lag-compensated view for tick extensions.** `IGSTN_CombatTickServices::GetViewSnapshot()` returns
   `ResolveAttackerSnapshot(TimeStep, StepSnapshot)`, built lazily once per frame (the overlay the attacker step already uses: on the
   authority, for a remote player's pawn, the other entities at the input's interpolation time, capped by `MaxRewindMs`; everywhere else the
   shared snapshot, which on a client already holds the interpolated proxies the player sees). Cost only when an extension asks.
2. **GSTNLink (generic): initiator-placed exits.** `UGSTN_LinkTypeDefinition::bInitiatorPlacesExit` (default false: grabs and latches
   unchanged). When the parked initiator releases (its own `Release`/`Escape` PendingBreak in the link tick), the tick runs the type's (or the
   move's) exit placer on the partner's entry of the **view** snapshot and writes `FGSTN_LinkActions::Exit` (Teleport, due at Frame + 1)
   in its own fragment, predicted. `FGSTN_LinkWatchdogEntity` gains that `Exit`; for a break whose exiting side already carries an exit
   for this serial, the watchdog copies it into the Break command instead of placing (so the rider's client receives what it predicted:
   no correction). Collision on the rider's client: `IGSTN_ExitCollisionQuery` from the world in the sim is not rollback-safe; the first
   version places without collision (placer's first candidate) and the watchdog checks the copied place with the world, falling back to
   its own placement (one correction) only when it overlaps.
3. **Shared with D9 part 2 (drill per-latch anchor):** the same view gives the driller its anchor: at the latch, the tip's hit location
   relative to the target's pose in the driller's view (`GetViewSnapshot`), quantised (int16 cm x3) into a new per-link
   `FGSTN_LinkState` anchor override (flag bit `GSTNLinkFlags::AnchorOverride`), which the snapshot contributor and the linked movement
   mode use instead of the definition's fixed offset. Server and client agree because the server's hit is lag-compensated by the same overlay.
   The anchor field changes the link state's wire (live states only; idle stays one byte): bump the fragment version.
- Harness for item 1: `dismount_predicted` (client rider dismounts while the carrier walks and runs: 0 corrections on the rider's
  client, exit within 5 cm of the authority's), plus SUITE_MOUNT and SUITE_LINK unchanged.

### Built in M7-04: what differs from the design above

| Design | Built |
|---|---|
| `GetViewSnapshot()` pure accessor | A virtual with a default returning null (so test doubles of `IGSTN_CombatTickServices`, e.g. the defence tests' fake, need no change); the caller falls back to `Context.Snapshot`. The component builds the view once per tick (`ResolveViewOnce`) and the attacker step shares it. |
| the rider writes `Exit` and the watchdog copies it | Plus: the rider ENDS its own link on the exit's frame (`StepOwnExitBreak`, first thing at AfterClocks, the same `ApplyCommand` with the same exit). Without it the rider's client would keep the link until the server's state arrived (a correction). On the authority the command is drained first, so the step finds the link idle. |
| watchdog copies "for this serial" | The copy condition is: type flag, side role 1 = parked, `Exit.Frame == F + 1`, flag `GSTNLinkExitFlags::InitiatorPlaced` (new, bit 2), `PendingBreak` set. The world check is `IsCapsuleFree` with the pair query; a blocked place falls back to the placer (flag Fallback when invalid, no InitiatorPlaced). |
| (not in the design) | Missed frame rule: a side whose partner's channel is idle with the same serial and a reason, for a type with the flag, breaks at once with that reason (not as an orphan after the grace). Covers a slow host (several fixed ticks per engine frame). |
| (not in the design) | `ShouldReconcile` accepts an initiator-placed exit (same frame and flags; position and velocity within `UGSTN_LinkSettings::PredictedPlacementToleranceCm` = 5 cm, yaw within 5 degrees) and an anchor override within the same tolerance: both machines place from interpolated poses and never agree bit for bit. |
| anchor in the link state, "bump the fragment version" | `int16 AnchorCm[3]` + `GSTNLinkFlags::AnchorOverride` (bit 2); serialised after `LocalYawQ` only with the flag, so the fragment has no version to bump: a state without it keeps its bytes. The snapshot extension carries it (`BodyAnchorCm`), which made it 38 B: GSTNCombatNP's `FGSTN_SnapshotExtensionBlob::MaxBytes` went 32 -> 40 (blob 48 B with its header; the parts test's 48 B bound holds). |
| "the linked movement mode uses it" | The linked mode never reads the offset (the capsule stays where it was, the snapshot substitution places the hurtboxes): the consumers are the snapshot contributor (`TryGetParkedTransform` overload with the parked state), the watchdog's leash (`ParkedDistance`) and the presentation component. Nothing writes an anchor yet: D9 part 2 calls `SetAnchorOverride` on the latcher's Body link. |
| exit capsule | The rider's capsule size comes from its root capsule component (content, fixed), as the watchdog reads it; `FExitSelf` defaults (34/88) in tests. |
| `bPredictRiderExits` default false | true (owner decision S1); the mount type copies it into `bInitiatorPlacesExit`. |

## G7: hits while mounted (M7-05, desktop)

| Plan / spec | Shipped |
|---|---|
| `UGSTN_HitStep_MountHit` "after ComputeKnockback" | Right after it (before ApplyDamage, MeterGain, LinkBreakOnHit), by `UGSTN_PiggybackContentLibrary::AddMountStepToRules` (idempotent) and `setup_defence_link_content.py`. Steps after ComputeKnockback cannot use the reaction scales (already applied), so the step replaces the outcome's knockback with the scaled one |
| break reasons `RiderHit` / `CarrierHit` | The step adds GSTNLink's `FGSTN_LinkBreakOutcome` with the plugin reason before LinkBreakOnHit; `MarkPendingBreak` keeps the first reason, so other link types are untouched. Thresholds are the registered definition's hit breaks (`QualifiesForBreak`), i.e. the default `FGSTN_MountRules` |
| `CarrierCoverFrontDeg` "hit redirected to the carrier's hurtboxes" | No redirect: a hit from inside the arc around the rider's facing does not land on the rider; the carrier takes it only if its own hurtboxes overlap. Off by default |
| `bCarrierShieldCoversRider` | NOT built: the rider's hit view has no carrier defence state. Off by default, unread (OPEN) |
| partners excluded "unless LinkedPartnerOnly" | Built: any mount partner hit with another filter is dropped (Stop), friendly fire or not |
| carrier above threshold "rider dropped, velocity 0.5 x launch" | The break only; exit velocity waits for G9's `LaunchLinked` (OPEN) |
| credit "from the attacker's link and ActivePair" | From the attacker's mount link per `FGSTN_MountRules::DefaultCredit` (OwnerOnly default = none; Split = share 128). ActivePair is G8. Partner meter requests still applied by nobody (M7-03 OPEN 1) |
| rules "absent = defaults" | `FindRules` reads the rules asset's `FGSTN_MountRules` extension for scales, arc and credit, else the defaults |

## G8: pair-move data (M7-07, desktop)

| Spec / plan | Built |
|---|---|
| `PairIdx` int16 "the compiler assigns each an index" | Authored `PairMoveId` (1..32767) on the definition: GSTNLink G2's provider is one per link type and the initiator's `PartnerMove` key names the id as data, so the id must exist at authoring time and must not depend on which kits a machine compiled. Validation checks the key exists |
| registry "compiled with the loadout" | `UGSTN_PairMoveRegistry` (module-owned, registered for `Combat.Link.Type.Mount` at post-engine-init); the mount loadout extension adds the kit's set (`FGSTN_MountKitExtension::PairMoves`, soft path: Core names no Runtime class). Ordered by id; same id from another definition refused (first wins, logged) |
| binding's compiled entry stores `PairIdx` and `PartnerRequirements` | Not needed: GSTNLink checks `PartnerRequirements` at the partner's start (G2). `InitiatorRequirements` is data only: the kit's own binding of the initiator move enforces it |
| `InitiatorTimelineOverride` / `PartnerTimelineOverride` | Not built: GSTNLink's action info has no timeline field; a Blueprint subclass of the ability is the per-kit timing |
| `OutcomeFrame` `INDEX_NONE` = "when the initiator's move ends" | `INDEX_NONE` = no outcome (G2 semantics); validation requires a frame for `Break`, refuses `SwapRoles` (reserved) and an outcome frame before the key |
| partner offset "from the initiator's start frame" | From the initiator's `PartnerMove` key frame (G2), effective minimum 1 |
| `ExitMode` | One `UGSTN_Placer_Mount` per definition (mode + `ExitDistanceCm`), owned by the registry; break reason `Combat.Link.BreakReason.Outcome` |
| `Interruptible` partner | `bPartnerLocksCommands` is forced false for it; "a hit replaces its part" is GSTNLink's forced-timeline behaviour (not re-tested here) |
| partner requirements read provider tags (OPEN since G2) | Still OPEN: tags only `PartnerRequirements` names are not in `FGSTN_LinkRequestTags` and read absent. Needed by G9 content only if a reference move requires such a tag |

## G9 part A: the exit launch of a pair move (M7-08, desktop)

G9 split in two: part A (this) is the runtime the reference moves miss; part B is the content (`PM_ThrowRider`, `PM_BoostJump`: four
abilities with timelines, the definitions, the test kit's set), `scenario_throw_rider` / `scenario_boost_jump` and spike 4.

| Spec / plan | Built |
|---|---|
| `LaunchLinked` executes the exit's velocity; "at release the server fills `PendingExit.Velocity`" | No new request: GSTNLink's exit already carries a velocity to the parked mode (G6, SD-12). The mount placer computes it: `UGSTN_Placer_Mount::Launch` (`FGSTN_PairExitLaunch`), filled by the registry from the definition's new `ExitLaunch`; zero = a put-down (every existing exit unchanged). Math in `UGSTN_MountUtils::ComputeExitLaunch` |
| throw aim "carrier's stick snapped to the movement plane, fixed pitch `ThrowAngleDeg`" | The query's `PreferredDirection` (the carrier's forward, which follows its stick while it moves) flattened onto the plane of the rider's up, pitched by `PitchDeg`. The stick itself is not read: the exit query has no input |
| `ThrowInheritCarrierVelocity` / `InheritCarrierVelocity` × the carrier's (horizontal) velocity | `InheritPartnerVelocity` (+ `bInheritHorizontalOnly` for the boost) × the new `FGSTN_ExitQuery::PartnerVelocity`: the watchdog fills it from the pawn on the authority (`FGSTN_LinkWatchdogEntity::Velocity`), an initiator placing its own exit from its view snapshot's `Velocity` (the boost jump, predicted) |
| boost "rider's `JumpVelocity × BoostMultiplier`" | One `UpSpeed` on the definition: the placer reads no Mover settings. Content authors the product |
| `RefundAirJumps`, thrown rider's sweeping hitbox, `ThrownActionableFrame` | Part B (abilities and timelines) |
