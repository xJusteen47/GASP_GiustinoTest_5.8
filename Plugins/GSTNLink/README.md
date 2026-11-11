# GSTNLink

Links between two combat entities for the GSTN combat on Unreal Engine 5.8: **grabs, pummels and throws**, and **latches onto large
enemies**, as data-driven *link types* with roles, request rules, break rules, exit placers and mash, all pure and integer-safe. An
entity whose hits and kits carry no link type behaves as it did before the plugin.

Status: **experimental (0.1)**, `EnabledByDefault: false`. Checkpoints F5a and F5b of
[m6-defence-links-plan.md](../../docs/architecture/plans/m6-defence-links-plan.md) are built (skeleton, tags, settings, link type
definitions, rules data, the outcome track, the request rules, exit placers, link state, mash and break math, and the fake host), and
F9 adds the GAS module (link type registry, request checks, the grab candidate provider of the interact probe, anim layers by role).
F10 adds the Network Prediction module part 1 (the link fragment, the parked movement mode, formation through the combat inbox, the
presentation transform, the debug draw); F11 adds part 2 (the watchdog, the break rules as it runs them, exit placement and its teleport,
`LinkBreakOnHit`); F12 adds the grab commands, pummel, throws, mash, immunity and the grab gate (F12a) and the latch on big enemies
(F12b). The design is
[combat-m6-defence-links.md](../../docs/architecture/combat-m6-defence-links.md).

M10 G2 (generic): the interact probe also reads **stamped records** of the world's GSTNInteract registry (`FGSTN_LinkRecordSource`; objects at rest
have no snapshot entry), for any kit; their meanings come from the GSTNInteract settings, merged into the registry's meaning table by tag. Still
one projected `Interact.Target.*`. Tests `GSTNLink.NP.Interact.{RecordCandidate, StaleRecordIgnored, OfferAndRecordOneTag}`.
The probe notes its focus id per entity and frame in the GSTNInteract subsystem (`NoteProbeFocus`, host data, never read by the simulation).

M10 G1 (generic): a link partner that is **not a fighter**. A `Form` command names the partner's kind and serial (`PartnerKind`, `PartnerSerial`;
default Fighter, so every earlier command is unchanged) and refuses a type that cannot coexist with the other channel's live link
(`ForbiddenWithBody`). The watchdog judges such a link through the resolver its kind registered (`IGSTN_LinkPartnerResolver`,
`UGSTN_LinkWatchdog::RegisterPartnerResolver`): partner gone -> one-sided Forced break after the orphan grace; else the own side's break rules
with the plain root distance; no resolver -> left alone. A Hand break without a placement leaves the Body exit record alone. First user:
GSTNHeld's Held type (tests `GSTNHeld.Runtime.{HandLink, WatchdogObjectPartner}`).
M10 G6: a `Form` command also carries the link's `Payload` byte (default 0), as an accepted link does, so a commanded link's role effect gets its
magnitude (GSTNHeld's carry load, test `GSTNHeld.Runtime.CarryLoad`).

M7 G1 (generic follow-ups for link types of other plugins; no type of theirs is named here): interact offers per link type compiled by the
registry (meaning, priority, reach, required kit extension) and the probe built from them; the team and the kit's extension array on each
request side; the optimistic initiator accept (`bInitiatorPredicts`, `PendingFrames`); a link effect per role and the rules' `Payload`
(`ComputePayload`); the generic team rule; the presentation shows a predicting initiator parked from its request and re-asserts the mesh
attachment every tick. Corrections to the M7 spec are in [m7-doc-deltas.md](../../docs/notes/m7-doc-deltas.md).

M7 G2 (generic, no type of another plugin named): **partner moves** (a link type's action provider, `IGSTN_LinkActionProvider`, registered per type in the registry, resolves an `int16` id to the partner's ability, requirements, start offset, outcome frame and kind, exit and `MaxDurationFrames`; a `PartnerMove` link outcome key on the initiator's move records it and posts the `PartnerMove` inbox kind to the partner, whose link tick starts its part as a Forced timeline on the target frame; the outcome breaks through the watchdog with the action's exit; failure paths abort with `Combat.Event.Link.PartnerMoveAborted`) and **parked facing** (`FGSTN_LinkRoleDef::YawLimitDeg`; a parked role that acts turns, `LocalYawQ`, from the stick at the start of its moves and while idle; the snapshot substitution and the presentation compose the turn).

## Modules

| Module | State | What it is |
|---|---|---|
| `GSTNLinkCore` | exists (Runtime, Default) | Native tags, settings, the link type definition asset and the rules structs (grab, latch, mash, break), the kit extension and the outcome track; the request rules, the exit placers, the link state types, the mash and break math, and the host interface with its test double. No GAS, Network Prediction or Mover, so it needs no fork. Covered by `GSTNLink.Core.*`. |
| `GSTNLinkGAS` | exists (Runtime, Default) | `UGSTN_LinkRegistry` (engine subsystem: every link type definition the asset manager finds plus code-added ones, type tag to wire index sorted by tag name, roles, channel, the break reason table with plugin reasons), `UGSTN_LinkRequirementLibrary` (the request checks: common checks then the definition's rules, other-channel resolution, kit rules, the roles that block actions), `FGSTN_LinkInteractProvider` (the `GSTNInteract` candidate provider of grabbable entities, pre-filtered by the type's own rules, and the one-tag projection), `UGSTN_LinkAnimSelection` (the anim layer of a role from the weapon's then the kit's link extension). No NP, no Mover. Covered by `GSTNLink.GAS.*`. |
| `GSTNLinkNP` | part 1 (F10; Runtime, Default) | `FGSTN_LinkFragment` (feature id 18: both channels, link actions, grab immunity, mash frame; idle = the list's one bit), `FGSTN_LinkSnapshotContributor` (the link summary, request tags as a mask over `FGSTN_LinkRequestTags`, the body content index; its finalize pass puts a parked role's hurtbox anchor on the partner's baked socket), `FGSTN_LinkTickExtension` (priority 120: request timeouts, the predicted flag of a grab-like hit, the action veto of a non-acting parked role and its timeline cancel, the parked modifier's edges, `Combat.Link.*` tags, `Interact.Target.Grab` through the GAS provider for kits with a link extension), the `LinkResponse` / `LinkCommand` inbox kinds, the `GrabImmunity` and `LinkRequest` steps with their outcome applier, `UGSTN_LinkedMovementMode` + `FGSTN_LinkedModifier` + `UGSTN_LinkedModeTransition`, `FGSTN_LinkAttach` (per-entity content: reference-pose sockets, kit link extension), `UGSTN_LinkPresentationComponent` (`GetLinkedTransform`, mesh on the partner's socket), `FGSTN_LinkDebugDraw` (`gstn.Combat.Debug.Links 1`), `UGSTN_LinkContentLibrary` (harness and content helpers). Part 2 (F11): `UGSTN_LinkWatchdog` (the only break authority: a world subsystem that works on the server only, after the actor tick of every engine frame; it reads every combat entity's finalized state and runs `FGSTN_LinkWatchdogLogic`: Serial-matched pairs through `UGSTN_LinkMath::EvaluateBreak` with the type's break rules, orphans after `OrphanGraceFrames`, the parked side's exit from the type's placer through `FGSTN_WorldExitCollisionQuery`, `LinkCommand{Break}` stamped F to both inboxes, events `LinkBroken` / `GrabEscaped`), the `LinkBreakOnHit` step and its applier (PendingBreak, reason Hit, per the type's `HitBreaks`), `FGSTN_LinkExitRequest` (the teleport as a Mover instant move through `UGSTN_GE_LinkExit` on the frame the command's exit record is written). Part 3 (F12): the commands in the link tick extension (`Release` through the
role's release reason, `Throw` as a direct hit posted to the partner, the mash step), `FGSTN_LinkThrow` (the throw entry and outcome: both
sides end in the simulation with reason Throw, the thrown side placed at the thrower's socket and launched, grab immunity, the thrower's
cooldown), `FGSTN_LinkMash` (presses and stick flicks bring the escape frame closer), `FGSTN_LinkShakeOff` (a role's `ReleaseBreakReason`), a hit's
`LinkFilter` (`LinkedPartnerOnly` / `ExcludeLinkedPartner`), pummel and `bNoReaction` on the partner, the type's `Alternatives` tried
before it (a grab on a fighter too heavy to grab forms the latch), the harness test abilities (`UGSTN_GA_LinkTest*`). Needs the GAS-NPP fork. Covered by `GSTNLink.NP.*`. |

The module class (`Private/GSTNLinkCoreModule.cpp`) registers the plugin's `Config/Tags` folder with the tag manager: the engine
reads `Config/Tags` only for the project and for Game Feature plugins, not for ordinary plugins, so the ini vocabulary would
otherwise be missing. It also registers the primary asset type `GSTN_LinkType` with the asset manager once the manager exists, over
the plugin's content and `LinkTypeDirectories` (always cooked), so the project's `DefaultGame.ini` does not list the type: a
`PrimaryAssetTypesToScan` line naming this plugin's class trips an engine ensure in every build where the plugin is disabled.

## Dependencies

- Plugins: `GSTNCombat` (`GSTNCombatCore`), `GSTNCombatGAS`, `GSTNCombatNP`, `GSTNInput` (`GSTNInputCore`), `GSTNInteract`
  (`GSTNInteractCore`, `GSTNInteractRuntime`), and the fork's `GameplayAbilities`, `NetworkPrediction` and `Mover`.
- `GSTNLinkCore` (`Build.cs`): public Core, CoreUObject, Engine, GameplayTags, DeveloperSettings, GSTNInputCore, GSTNCombatCore;
  private Projects (to find the plugin's own folder). F12 adds `UGSTN_LinkSlots` (the standard link input slots and bindings), `FGSTN_LinkHitData` (hit extension:
`bNoReaction`) and the `Command.Link` tier contribution to the input settings. No edge to the project module, to the sibling plugin `GSTNDefence`, or to GAS, NP
  and Mover.
- `GSTNLinkGAS` (`Build.cs`): public Core, CoreUObject, Engine, GameplayTags, GSTNInputCore, GSTNCombatCore, GSTNCombatGAS,
  GSTNInteractCore, GSTNLinkCore. No NetworkPrediction, AbilitySystemSimulation, Mover, GSTNCombatNP, GSTNDefence, Editor or project module.
- `GSTNLinkNP` (`Build.cs`): public Core, CoreUObject, Engine, GameplayTags, GameplayAbilities, NetworkPrediction, Mover, GSTNInputCore,
  GSTNCombatCore, GSTNCombatGAS, GSTNCombatNP, GSTNInteractCore, GSTNInteractRuntime, GSTNLinkCore, GSTNLinkGAS. No GSTNDefence, Editor or
  project module.
- `GSTNCombat` builds and passes its tests with this plugin absent: the plugin adds behaviour by registering into the combat
  extension surface, never by editing the combat core.

## Data

| Type | What it is |
|---|---|
| `UGSTN_LinkTypeDefinition` | A link type, a `PrimaryDataAsset` of primary asset type `GSTN_LinkType`: the type tag, the channel (`Body` parks the entity, `Hand` never does), two `FGSTN_LinkRoleDef`, `FGSTN_LinkBreakRules`, `RequestRequirements`, `ForbiddenWithBody`, `bInitiatorPredicts`, `MaxFrames`, `DebugColor`, `Alternatives` (F12: other type tags a request of this type tries first, in order; the grab lists the latch). A role's `ReleaseBreakReason` (F12) is the reason its Release command ends the link with (the latched side's is `ShakeOff`). `Validate` is pure; `IsDataValid` forwards it. The asset manager finds every definition; code never lists them. |
| `FGSTN_LinkRoleDef` | One role: its tag, `bParked`, `bActsWhileLinked`, `bCollisionWithPartner`, the socket of the partner it is placed on (by name) and the offset. A grab parks role 2; a latch parks role 1. G1: `LinkEffect` (`FGSTN_LinkEffectDef`: a gameplay effect class path and a SetByCaller tag) the role carries while Active, magnitude `Payload / 255`. G2: `YawLimitDeg` (360): the arc a parked role that acts may turn through (`LocalYawQ`, 1/256-turn steps; 0 = fixed). |
| `IGSTN_LinkActionProvider`, `FGSTN_LinkActionInfo`, `UGSTN_LinkActionTable` | G2: the partner moves of a link type. The provider (registered with `UGSTN_LinkRegistry::RegisterActionProvider(LinkType, Object)`, keyed by the type tag) resolves an id (1..32767) to `FGSTN_LinkActionInfo`: `PartnerAbility` (a timeline ability of the partner's kit), `PartnerRequirements` (on the partner's snapshot request tags), `PartnerStartOffsetFrames` (at least 1), `OutcomeFrame` (the initiator's timeline frame) and `Outcome` (`KeepLink`, `Break`, `SwapRoles` reserved), `BreakReason`, `ExitPlacer` (null = the type's), `bPartnerLocksCommands`, `MaxDurationFrames`. Content: the same answer on every machine. `UGSTN_LinkActionTable` is a table filled by code. |
| `EGSTN_LinkOutcome::PartnerMove` | G2: a link outcome key that starts the partner move `PartnerMoveId` (the initiator's record `FGSTN_LinkActions::Active` with `Side`, the partner's part through the `PartnerMove` inbox kind). |
| `FGSTN_LinkInteractOfferDef` | G1: how the interact button offers a type (the definition's `InteractOffer`): the `Interact.Target` meaning (empty = not offered), `Priority` (0..255, higher first; the grab is 100), `ReachCm`, `RequiredKitExtension` (the kits that look for it), `Relationships` (M7-02: the `Combat.Relationship.*` of a candidate the offer applies to, by GSTNCombat's default rule on the teams; empty = every one; GSTNLink's own grab offer lists Enemy and Neutral). `UGSTN_LinkRegistry::RegisterInteractOffer` gives a type whose asset names none an offer by code (GSTNLink does it for its grab). |
| `bInitiatorPredicts`, `PendingFrames` | G1, on the definition: the initiator's predicted request becomes Active (flag `Predicted` kept) `PendingFrames` (2) after the hit unless the target's snapshot entry shows it linked to someone else; the response confirms it. |
| `bInitiatorPlacesExit` | G6, on the definition: the parked initiator places its own exit from its view and ends the link by itself the next frame; the watchdog copies the exit (see How to extend). Off for grabs and latches. |
| `UGSTN_LinkRule_Team` | G1: a generic rule on the sides' teams (`Any`, `AlliesOnly`, `EnemiesOnly`); refuses with `Team`. |
| `FGSTN_LinkBreakRules` | `MaxDistanceCm`, `HitBreaks` per role (`MinKnockbackToBreak`, `bPartnerHitsBreak`), death flags, `PartnerMustNot` state tags. |
| `FGSTN_GrabRules`, `FGSTN_MashRule` | The grab data of a kit: requirements, weight limit, input timings, immunity frames, pummel delay, the mash. |
| `FGSTN_LatchRules` | The latch data of a kit: requirements, `MinLatchWeight` (the mirror of the grab's weight limit). |
| `FGSTN_LinkKitExtension` | Entry of a kit's extension array: its own grab and latch rules and the animation layer per role tag. |
| `FGSTN_LinkOutcomeTrack` | Timeline track with the grabber's release / throw keys. |
| `UGSTN_LinkRule` | Request rule: abstract, Blueprintable, `EditInlineNew`, one `BlueprintNativeEvent` `Evaluate(FGSTN_LinkRequestContext)` giving Accept or Refuse(reason), pure. A definition lists its rules (instanced); `CheckCommon` holds the checks every type shares. |
| `UGSTN_LinkRule_Grab`, `UGSTN_LinkRule_Latch` | The grab rule (requirements, `Combat.Part.GrabLocked` by name, weight at most `MaxGrabWeight`, immunity frame) and the latch rule (weight at least `MinLatchWeight`, `Combat.Immunity.Latch`, one latcher per partner by the busy-slot check). The kit's rules in the request context win over the rule's own copy. |
| `UGSTN_LinkExitPlacer`, `IGSTN_ExitCollisionQuery` | Where an entity lands when a link ends: abstract placer, one `BlueprintNativeEvent` `Place(FGSTN_ExitQuery)`, deterministic, asks the world only through the query interface. A definition holds its placer instanced; none = the ring. |
| `UGSTN_Placer_Ring`, `UGSTN_Placer_AtSocket` | The ring (preferred direction, seven turns of 45 degrees, above, then a reported-invalid fallback next to the partner) and the throw's exit (the partner's socket, raised retries, the throw hit's knockback as velocity). |
| `FGSTN_LinkState`, `FGSTN_LinkStates`, `FGSTN_LinkActions` | Plain sync value types: indices and ticks only, no tag; one state per channel; idle serialises to one byte; serial rules make every inbox entry idempotent. |
| `FGSTN_LinkRequestContext` | The const view a rule reads: both entities as values (tags, weight, link slots, immunity frames, G1: team and the kit's extension array, `FindKitExtension<T>()`), the definition, the kit rules. |
| `UGSTN_LinkMath` | `MashInitialFrames`, `MashProgress`, `MashEscaped`, `EvaluateBreak` (one reason wins, in a fixed order), `ParkedTransform` / `TryGetParkedTransform` (same derivation for whichever role is parked), reason tags. |
| `UGSTN_LinkSlots` | The standard link input slots (Grab and Release on Interact, Pummel, the four throws, `Latch.Attack`) at the `Command.Link` tier, each `bFallThroughWhenUnavailable`, role slots `RoleSlotPriorityOffset` (100) above, and `DefaultBindings()` (the Release bindings of each role ignore the other roles). |
| `FGSTN_LinkHitData` | Hit extension: `bNoReaction` (a hit on the linked partner deals its damage with no reaction, the latch attack and pummel). |
| `IGSTN_LinkHost`, `FGSTN_FakeLinkHost` | What a link needs from its host, and the double that holds entities and link states and executes requests, breaks and exits synchronously with no NP, for tests of plugins built on links. |

## Tags

All native tags are declared in `GSTNLinkCore` (`GSTN_LinkTags.h`), one doc line each. This is an accepted layout: the GAS and NP
modules define no tag, and the "owner" column of the spec names the module that writes a tag, not the declaring module.
Designer vocabulary (the `GameplayCue.Combat.Link.*`, `.Pummel` and `.Throw` cues) is ini only: `Config/Tags/GSTNLink.ini`.

| Tag | Written by |
|---|---|
| `Combat.Link.Linked`, `Combat.Link.Pending`, `Combat.Link.Role.Grabber`, `.Grabbed`, `.Latcher`, `.Latched` | Projected by the link tick extension from the link state, never granted |
| `Combat.Link.Type.Grab`, `Combat.Link.Type.Latch` | Definition assets, hits and events |
| `Combat.Link.BreakReason.*` (`ShakeOff`, `Timeout`, `Distance`, `OwnDeath`, `PartnerDeath`, `Hit`, `PartnerState`, `Escape`, `Release`, `Throw`, `Forced`) | The emitter of the break, carried by `LinkBroken` |
| `Combat.Immunity.Grab`, `Combat.Immunity.Latch`, `Combat.Lock.Link` | Kits, abilities, effects (ordinary counts) |
| `Combat.HitType.Throw`, `Combat.HitType.Pummel` | The hit data |
| `Combat.Mover.Linked` | The linked modifier |
| `Interact.Target.Grab` | The link tick extension, from the interact probe |
| `Command.Link.Grab`, `.Grab.Special` (M11 K2: special + crouch), `.Pummel`, `.Release`, `.Throw.Forward`, `.Back`, `.Up`, `.Down`, `.Latch.Attack` (F12), `Input.Lock.Command.Link` | The standard command set, abilities and effects |
| `Combat.Event.LinkFormed`, `.LinkBroken`, `.Throw`, `.GrabEscaped` | The watchdog and the combat outbox on the authority |
| `Combat.Event.Link.PartnerMoveAborted` | G2: the link tick's combat outbox on the authority (Magnitude = the action id) |

`Combat.HitType.Grab` is declared by `GSTNCombatCore`, so this plugin reads it and does not declare it. The link subtree tags reserved
for M7, M8, M10 and the drill (`Type.Mount/Ride/Held/Minion/Drill`, `Role.Carrier/Rider/Holder/HeldObject/Driller/Drilled`) are
declared by those plugins. The plugin declares no `Combat.State.*`, `Combat.Part.*` or defence tag and reads those by name. No tag is
ever stored in a sync state or a snapshot: only the link type index, the role index and the break reason index.

## How to extend

- **A new link type** (a mount, a held object, a drill): a new `UGSTN_LinkTypeDefinition` asset and the tags it needs, declared by the
  plugin that ships it. No change in this plugin. A definition whose tags are not registered fails validation and is skipped. The
  registry gives it a wire index by its tag name; a definition made by code joins through `UGSTN_LinkRegistry::AddDefinition`. Assets
  outside the plugin's content need their folder in `LinkTypeDirectories`.
- **A new break reason**: declare its tag in your plugin and call `UGSTN_LinkRegistry::RegisterPluginReason(TagName)` from your
  module startup; it takes an index from `EGSTN_LinkBreakReason::FirstPlugin` on, in tag name order.
- **A type the interact button forms** (a mount, the drill's latch variant): set the definition's `InteractOffer` (its own
  `Interact.Target` leaf, declared by your plugin, a priority below the grab's 100 if the grab should win, the reach, and the kit extension
  struct of the kits that may use it). The registry compiles it into the offer list and the probe's meaning table; the link tick extension
  projects whichever meaning the focus has. The type's rules pre-filter its candidates with the same request sides the hit path builds, team
  and kit data included, so an ally-only type (list `UGSTN_LinkRule_Team`) never offers an enemy (G1).
- **A team rule on every grab-like type** (M7 G5): the interact probe tries the offers on a candidate by priority and the first whose rules accept it names the candidate, so an offer whose rules ignore the team shadows every lower offer for that candidate (an unfiltered grab, 100, claimed allies the mount, 50, is meant for). List `UGSTN_LinkRule_Team` (EnemiesOnly for a grab or a latch, AlliesOnly for a mount; Any only in a friendly-fire mode). The test grab and latch carry it as their second rule (`Tools/GAS/setup_defence_link_content.py`; the harness edits `rules[0]`). Since M7-02 the offer data also filters by relationship before any rule runs (`FGSTN_LinkInteractOfferDef::Relationships`): the code-registered grab offer is for enemies and neutrals, so an unfiltered grab can no longer shadow an ally-only offer.
- **A rule that reads your kit's data**: put a `FGSTN_KitExtension` child in the kit's extension array and read it with
  `Context.Initiator.FindKitExtension<T>()` (or `Target`); `Context.*.TeamId` and `UGSTN_LinkRule::AreAllies` give the teams (G1).
- **A type-specific byte on the link** (a carry load): override `UGSTN_LinkRule::ComputePayload`; the first non-zero value of the rules is the
  link's `Payload` on both sides. A role's `LinkEffect` is applied with `Payload / 255` while the link is Active and removed on every end (G1).
- **An initiator that shows its link before the answer** (a mount): `bInitiatorPredicts` and `PendingFrames` on the definition; the
  presentation shows a parked initiator parked from the request on (G1).
- **A parked initiator that leaves on its own, predicted** (a rider's dismount): `bInitiatorPlacesExit` on the definition (M7 G6). On the
  frame its Body link gets a `PendingBreak`, the initiator's link tick places its exit (the playing partner move's placer, else the type's;
  no collision) around the partner in `IGSTN_CombatTickServices::GetViewSnapshot` (on the authority the partner rewound to the player's
  input time) and records it in `Actions.Exit` (flag `GSTNLinkExitFlags::InitiatorPlaced`, due the next frame); on that frame it ends the
  link by itself exactly as the watchdog's command would (`StepOwnExitBreak`). The watchdog copies the exit into its command when the
  world has room for it (the pair ignored), else places its own (one correction). A side whose initiator already ended the link on a frame
  the watchdog did not see breaks at once with the initiator's reason. The two machines' exits are compared within
  `PredictedPlacementToleranceCm`, so the rider's client takes no correction.
- **A per-link parked anchor** (a drill's latch point): `FGSTN_LinkState::SetAnchorOverride(LocalCm)` on the parked side's Body link (flag
  `GSTNLinkFlags::AnchorOverride`, three `int16` cm in the partner's socket space, on the wire only with the flag). It replaces the
  translation of the role's `SocketOffset` in the snapshot substitution, the watchdog's leash and the presentation. Decide it from
  `GetViewSnapshot` so the authority's lag-compensated hit and the player's machine agree; they are compared within the same tolerance.
- **A partner move** (a pair move: one partner throws the other, or jumps off it): implement `IGSTN_LinkActionProvider` (or fill a `UGSTN_LinkActionTable`) and register it for your type with `UGSTN_LinkRegistry::RegisterActionProvider`; put a `PartnerMove` key with the id on the initiator's move timeline (a link outcome track). The partner's ability must be a timeline ability of its kit. The partner's part starts on its own server state and reaches its client by correction (G2; M7 S4 measures the delay). Body channel only; one move at a time.
- **A parked role that faces where it acts**: leave `YawLimitDeg` at 360 or narrow the arc; 0 keeps the role on the placement's forward. The turn comes from the stick at the start of each move and while idle; no stick keeps it.
- **A role's animation**: an `AnimsByRole` entry in the kit's (or weapon's) `FGSTN_LinkKitExtension`.
- **A heavier or lighter grabber**: give the kit a `FGSTN_LinkKitExtension` with its own `FGSTN_GrabRules`.
- **A new request rule**: subclass `UGSTN_LinkRule` (C++ or Blueprint), override `Evaluate`, list it in the definition's `Rules`. Call
  `CheckCommon` first.
- **A second latch type** (the drill: a parked initiator on a possibly moving partner): a definition whose `InitiatorRole` is
  `bParked` (and `bActsWhileLinked`), its `SocketOnPartner` and offset, and its own `UGSTN_LinkRule` subclass. The `LinkRequest` step,
  the response, the parked mode, the anchor of the hurtboxes and the presentation are the same code for every type; nothing in
  `GSTNLinkNP` names a type. To have the interact tap form it, list its tag in another type's `Alternatives` (as the grab lists the
  latch) and give the latched role a `ReleaseBreakReason`; its attack is an ability whose hit carries `LinkFilter = LinkedPartnerOnly`.
- **Collision between the partners**: a type whose parked role says `bCollisionWithPartner` false ignores the pair's capsule collision
  while linked and for `ExitCollisionIgnoreFrames` (default 12) after the exit (F15). It is simulation state, not presentation:
  `FGSTN_LinkExitCollision` reads the link fragment (the live link, then the exit window both sides open from the same stamped frame) and
  the link tick extension writes the owner's own capsule ignore list before every simulated frame's Mover tick, so a client that still shows
  the released partner parked sweeps as the authority does, and a resimulation sweeps as the first pass. Tune the window on the definition;
  0 = collision returns on the exit frame. The exit placer's world query ignores only the pair, so a third pawn blocks a place.
- **A link command on another button**: a slot from `UGSTN_LinkSlots::Build()` with your own binding; keep it at the `Command.Link`
  tier (`CommandTiers` in the settings) so it is not shadowed by attack and strong commands of the same button.
- **Formation in the pipeline**: put `UGSTN_HitStep_GrabImmunity` then `UGSTN_HitStep_LinkRequest` after `HurtState` in the rules'
  `HitPipeline` (`UGSTN_LinkContentLibrary::AddLinkStepsToRules` does it); a hit whose data names a registered `LinkType` is a request.
- **The parked mode on a pawn**: author `UGSTN_LinkedModeTransition` as a global transition of the pawn's Mover (or let
  `FGSTN_LinkAttach` add the mode and an entry transition to every mode at attach, as the harness pawns get them).
- **A new exit**: subclass `UGSTN_LinkExitPlacer` (or tune `UGSTN_Placer_Ring` on the definition) and set it as the definition's `ExitPlacer`.
- **Worked example from another plugin**: `Plugins/GSTNExtensionTest` (test only, M6 F14) adds a carry type from outside this plugin through the
  public API: a definition made by code and `AddDefinition`, its own type and role tags, a `UGSTN_LinkRule` subclass, and its break rules.
  Its tests (`GSTNExtTest.Link.*`) form it through this plugin's `LinkRequest` step, outcome applier and response kind, park the target
  through the existing tick extension and measure the same 1 + 22 bytes as any live link: nothing here or in the combat model's tick
  changed (m6 spec 12 item 7).

## Extending in Blueprint

What a designer can add without C++ (m6 plan section 0, MT 3.8). Each extension class has ONE `BlueprintNativeEvent`, a const input and a
result out, and must be pure: read the input and nothing else (no actor, component, world or latent node), because it runs inside the
simulation on every machine and in every resimulation.

| Extension | Blueprint parent | Entry point | Where it goes |
|---|---|---|---|
| Request rule | `UGSTN_LinkRule` | `Evaluate(FGSTN_LinkRequestContext) -> FGSTN_LinkRuleResult` | the definition's `Rules` (instanced) |
| Exit placement | `UGSTN_LinkExitPlacer` | `Place(FGSTN_ExitQuery) -> FGSTN_ExitResult` | the definition's `ExitPlacer` (instanced) |
| Pipeline step | `UGSTN_HitResolutionStep` (GSTNCombatCore) | `Resolve(FGSTN_HitContextView, FGSTN_StepResult)` | the combat rules' `HitPipeline`; an inbox entry back to the attacker (`OutgoingEntries`) is C++ only |
| Link type | `UGSTN_LinkTypeDefinition` (data asset) | none: roles, sockets, break rules, requirements, rules, placer | the plugin's content or a folder in `LinkTypeDirectories` |
| Role animation | `FGSTN_LinkKitExtension::AnimsByRole` (data) | none | the kit or weapon |

`UGSTN_LinkMath` exposes the stateless math (`EvaluateBreak`, the mash, the parked placement, the reason tags) as `BlueprintPure`, and
`UGSTN_LinkRule::CheckCommon` and `ContainsTagNamed` are `BlueprintPure` statics (F15): a Blueprint rule's `Evaluate` calls `CheckCommon` on
the context it was given and returns its refusal first. The context's definition and link states are not reflected but travel with the
struct's native copy, so the Blueprint call decides as the native one (`GSTNLink.Core.Rule.CheckCommonFromBlueprint`); a context made in
Blueprint has no definition and is refused with `NoDefinition`. Not available to Blueprint: everything per frame (the tick extension, the
fragment, the inbox kinds, the watchdog), which stays C++.

## Settings

`Project Settings > Plugins > GSTN Link` (`UGSTN_LinkSettings`), defaults in `Config/DefaultGSTNLink.ini`:

| Setting | Meaning |
|---|---|
| `RequestTimeoutFrames` | Frames a predicted link waits for its response before the link tick clears it (12). |
| `OrphanGraceFrames` | Frames after a formation during which a one-sided link is not an orphan (3). |
| `PredictedPlacementToleranceCm` | M7 G6: how far an initiator-placed exit or a per-link anchor on the player's machine may be from the authority's before it is a misprediction (5 cm; the exit's yaw within 5 degrees). |
| `LinkTypeDirectories` | Content folders, besides the plugin's own, scanned for link type definitions (empty). Read when the asset manager is created. |
| `CommandTiers` | Command tiers this plugin adds to the input settings at startup (source `GSTNLink`, removed at shutdown): `Command.Link` = 360, above the attack and strong tiers. |
| `ThrowInputMode`, `bApplyKitThrows`, `FallbackThrows`, `bGrabWithSpecialCrouch`, `ThrowSlotOverrides` | M11 K2, see "Kit throws" below (Stick, on, the four `UGSTN_GA_LinkThrow*`, on, empty). |

## Kit throws (M11 K2)

Owner decisions 2-3 of 2026-11-11, subject to playtesting. A loadout extension of `GSTNLinkNP` (id `GSTNLinkThrows`) runs `UGSTN_LinkSlots::ApplyKitThrows`
on every kit ASSET that binds `Command.Link.Grab` to an ability, on every machine, so no kit is edited by hand (kits built in memory by code keep their
authored table, as with the kit swap binding):

- **Four throws per kit, common fallback**: `FGSTN_LinkKitExtension::Throws` (`FGSTN_LinkThrowSet`: Forward, Back, Up, Down ability classes). Per
  direction: the kit set's entry replaces the ability of the kit's own throw rows; with no row the kit set's entry, else `FallbackThrows`, is added
  (requires `Role.Grabber`); a row bound to nothing hides that throw. The fallback set is `Moves/GSTN_LinkThrowMoves.h` (placeholder values, no montage:
  forward 40 degrees, back 140 = up and behind the thrower, up 88, down 80 low; Blueprint children can add a montage).
- **The stick picks the throw** (`ThrowInputMode`, `EGSTN_LinkThrowInputMode`): `Stick` (default) = stick entering the forward or back facing sector at
  tilt (a one-element Motion: a stick already held when the grab lands does not throw; re-enter to throw), Jump = up, Crouch = down, Attack stays the
  pummel; `StickPlusSpecial` = Special + tilt, Special alone = up, Special + Crouch held = down; `Buttons` = the F12 rows of the slot set, unchanged.
  The shapes are binding slot overrides built by `BuildThrowSlot`; `ThrowSlotOverrides` replaces any of them (and the special grab's) by data. Every
  button is a logical `Combat.Button.*`, mapped to keys and pad by the input config.
- **Special + crouch grabs** (`bGrabWithSpecialCrouch`): a row `Command.Link.Grab.Special` (Press Special + Hold Crouch, offset 102) bound to the kit's
  grab ability with the grab's requirement. Its own command: a second `Command.Link.Grab` row with the same requirement makes the loadout validator
  refuse the kit's link rows (found in PIE).
- **Blockers**: other plugins register tags that switch throws off (`SetThrowBlockers`, by source; the link plugin names none of them). GSTNHeld
  registers `Combat.Held.CarryingEnemy` for the stick-only throws by default (a lifted enemy is thrown with the held throw, which owns the stick).

Tests `GSTNLink.Core.KitThrows.{StickPicksThrow, StickPlusSpecial, Fill, ProjectSettings}`; harness `kit_scenarios.scenario_kit_throw_stick_forward`,
`_back`, `scenario_kit_throw_lifted` (run with `Tools/GASNPPTest/batch_driver.py`, `BATCH_MODULE=kit_scenarios`).

## Predicted partner presentation (FP G1)

On the machine of the INITIATOR of a partner move (a client whose owner is locally controlled), the partner's authored reaction is shown from the initiator's own predicted fragment, about one network round trip earlier than the synced copy. `FGSTN_LinkPartnerWindow::BuildFromInitiator` (`Sim/GSTN_LinkPartnerWindow.h`, pure, deterministic) derives the window from the fragment's active move: start = S + max(PartnerStartOffsetFrames, 1), end = start + the partner timeline's TotalFrames (else MaxDurationFrames). G4 reuses the same window to promote the partner to forward prediction. The presentation component ticks an early function (TG_PrePhysics, after the Network Prediction finalize) that publishes the reaction on the partner's component (`GetPredictedPartnerMove`, `OnPredictedPartnerMove` Started / Ended / Dropped) and, when the partner timeline's montage is already loaded, leads it on the partner's mesh (the instance shows the later of its position and the predicted one; at the montage's end it is stopped and the late synced copy is kept off until the partner's shown state is past the move). A vanished initiator record before the partner's start, or an abort, drops it at once (a snap). Presentation only, no simulation state. cvar `gstn.Link.PredictPartnerPresentation` (default 1); never on a dedicated server, on the partner's own machine or on a listen host. Harness: `scenario_link_pair_move_delay` and `scenario_link_pair_move_delay_throw` in `Tools/GASNPPTest/mount_scenarios.py` (in `SUITE_MOUNT`).

## Tests

`GSTNLink.Core.*`: `Tags.*`, `Data.*` (F5a); `State.*`, `Mash.*`, `Break.*`, `Placer.*`, `Rule.Grab.*`, `Rule.Latch.*`, `FakeHost.*` (F5b). They run with only this plugin and `GSTNCombatCore` loaded: no GAS, no NP, no world.

`GSTNLink.NP.PartnerWindow.*` (FP G1): the window builder on fake fragments and an action table, and the pure playback rules.

`GSTNLink.GAS.*` (F9): `Registry.{IndicesStableAcrossLoadOrder, UnknownTagSkipped, ReasonTableAppend}`,
`Interact.{ProviderFiltersUngrabbable, OneTargetTagPerFrame}`, `Anim.LayerByRole`. No world: registries are fresh objects, definitions
transient. A test-only link type, its roles and two break reasons (`Combat.Link.*.GSTNTestOnly*`) are declared in the test file, in
builds with automation tests only, to prove a type from outside the plugin gets an index and roles.

`GSTNLink.NP.*` (F10): `Fragment.{RoundTripIdleOneBit, TwoChannelsBytes, Interpolate}`, `Formation.{AcceptedAtF1ResponseAtF2,
RefusedWhiffsNoHitstop, SimultaneousFirstWins, ChainRefused, PredictedClearedOnTimeout, SerialIdempotentAfterRollback, LatchParksInitiator}`,
`Parked.{ModeEnteredByTag, HurtboxFromPartnerSocket, NoInputNoGravity, InitiatorFollowsMovingPartner}`, `Tick.{GrabbedHasNoCandidates,
LatchedKeepsCandidates, TagsFromRole}`. No world: steps, applier and handlers on views and fragments, the Mover transition on a transient
component. Test-only link types `Combat.Link.Type.GSTNTestOnlyNPGrab/Latch` and their roles are declared in the test fixture (automation
builds only) and added to the global registry for the test's scope.

`GSTNLink.NP.*` (M7 G6, `GSTN_LinkNPTests_InitiatorExit.cpp`): `InitiatorExit.{PlacedFromView, OwnBreakEqualsCommand, BlockedPlaceFallsBack,
OnlyWhenAsked, MissedFrameBreaksPartner, ReconcileTolerance}`, `Anchor.Override`.

`GSTNLink.NP.*` (F11): `Watchdog.{BreakAtF1BothSides, OrphanAfterGrace, StaleSerialIgnored, OnePathForPipelineBreaks}`,
`Break.{Distance, Timeout, Death, PartnerState, HitByRole}`, `Exit.{NeverInsideGeometry, DeterministicForEqualInputs}`: the watchdog's pure
function on entity views, its commands applied through the real inbox rule, the exit placer with a fake world.

`GSTNLink.NP.*` (M7 G1, `GSTN_LinkNPTests_G1.cpp`): `Predict.{OptimisticAcceptAtPendingFrames, RefusalRollsBackToIdle, ConfirmClearsFlag,
GrabUnchanged}`, `LinkEffect.{AppliedOnActive, RemovedOnEveryBreakReason, IdempotentOnResim, PayloadToMagnitude}`,
`Interact.{SecondOfferProjectsItsMeaning, OfferPriorityOrder, AllyOnlyRulePreFilter}`, `Request.SideCarriesTeamAndKit`; and from outside
the plugin `GSTNExtTest.Link.{InteractOfferFromOutside, PayloadFromOwnRule}`. Every test file of `GSTNLinkNP` wraps its content in its own
namespace, so the module links in a unity build.

`GSTNLink.NP.*` (M7 G2, `GSTN_LinkNPTests_G2.cpp`): `PartnerMove.{StartsAtTargetFrame_0_1_12, SerialMismatchDropped, OutcomeBreaksBothSides, RollbackTrimsEntries, AbortOnPartnerKO}`, `Facing.{StickWithinLimit, AimedMoveSetsYaw, LatchUnchanged}`, `Fragment.PartnerMoveAndFacingRoundTrip`; from outside the plugin `GSTNExtTest.Link.PartnerMoveFromOutside`.

`GSTNLink.NP.*` (F12): `Grab.{WeightGate, GrabberCooldown, PummelKeepsLink, ThrowDirectHitNoOverlap, ThrowSkipsParryShield,
ImmunityAfterReleaseAndThrow, MashEscapeFrameDeterministic, GrabLockedByNameWithParts, GrabLockedAbsentWithoutParts}`,
`Latch.{WeightPicksLatchOverGrab, AttackHitsPartnerOnly, ShakeOffBreaksThroughWatchdog, LatchedEnemyKeepsActing}`.

Harness: `Tools/GASNPPTest/link_scenarios.py` (`SUITE_LINK`: `link_spike_*`, `link_rollback_*` (formation through a grab hit, a watchdog break
with forced reconciles over the break frames) and `link_break_distance_*` (a leash and the grabber moved away, then a knock-out of either side) in
three pairings, plus `grab_immunity`; F12: `grab_throw`, `grab_mash` (a forced reconcile mid-mash), `grab_interact`, `grab_gate` (with and
without the GSTNParts content), `throw_at_enemy`, `latch_big_enemy_*` in three pairings (the latched enemy's own client judged against the
authority at the same frame) and `latch_big_enemy_bystander_client_latches_client` (a bystander in the enemy's attack: judged on convergence);
`link_probe_*` are Task A's walk probes and `probe_*` debug aids, not in the suite; `defence_scenarios.scenario_grab_shield` is the real grab of a
shielding target), content from `Tools/GAS/setup_defence_link_content.py` (F13; idempotent, identical hashes on a rerun, validated by
`Tools/GAS/validate_defence_content.py`): `/GSTNLink/Test/DA_LinkType_Grab_Test` (grab rule at its defaults: the fallback of a kit without a link extension),
`DA_LinkType_Latch_Test` (the grab's alternative), the move Blueprints `GA_Grab_Test`, `GA_Pummel_Test`, `GA_Release_Test`,
`GA_Throw{Forward,Back,Up,Down}_Test`, `GA_LatchAttack_Test` (thin children of the native `UGSTN_GA_LinkTest*`), and the project kit
`/Game/GASNPPTest/Combat/KIT_Test_Link` (slots, moves and a link extension with the spec 11 test mash, base 90 frames, written by `UGSTN_LinkContentLibrary::WriteKitLinkContent`,
dirty only on a change) plus the link steps in the standard rules. The scenarios play `KIT_Test_Link`; `link_rollback` / `grab_immunity` still
turn the test weapon's attack into a grab in memory, and leash, immunity and weights are scenario numbers. The F10-F12 runtime path
(`AddLinkTestBindings` on `KIT_Test_Hits`) works with `link_scenarios.USE_CONTENT = False`. Debug: `gstn.Combat.Debug.Links 1` (console only)
draws the partner line, the parked capsule, the shown transform and the hurtbox anchor (socket gizmo), role, type, serial, phase and the
frames left to the link's end; no Gameplay Debugger rows (X19).

`GSTNLink.Content.SampleAssetsValid` (F13, editor context): both link types load, validate and are found by the registry; the grab rule keeps
the default mash and the type lists the latch; the move Blueprints derive from their native classes. `link_mp.py` runs the spike with the PIE clients in separate processes.
