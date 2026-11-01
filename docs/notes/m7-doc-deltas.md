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

## G2: partner moves and parked facing (HANDOVER 71)

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
