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
`LinkBreakOnHit`). The grab commands, pummel, throws and mash (F12) are not built. The design is
[combat-m6-defence-links.md](../../docs/architecture/combat-m6-defence-links.md).

## Modules

| Module | State | What it is |
|---|---|---|
| `GSTNLinkCore` | exists (Runtime, Default) | Native tags, settings, the link type definition asset and the rules structs (grab, latch, mash, break), the kit extension and the outcome track; the request rules, the exit placers, the link state types, the mash and break math, and the host interface with its test double. No GAS, Network Prediction or Mover, so it needs no fork. Covered by `GSTNLink.Core.*`. |
| `GSTNLinkGAS` | exists (Runtime, Default) | `UGSTN_LinkRegistry` (engine subsystem: every link type definition the asset manager finds plus code-added ones, type tag to wire index sorted by tag name, roles, channel, the break reason table with plugin reasons), `UGSTN_LinkRequirementLibrary` (the request checks: common checks then the definition's rules, other-channel resolution, kit rules, the roles that block actions), `FGSTN_LinkInteractProvider` (the `GSTNInteract` candidate provider of grabbable entities, pre-filtered by the type's own rules, and the one-tag projection), `UGSTN_LinkAnimSelection` (the anim layer of a role from the weapon's then the kit's link extension). No NP, no Mover. Covered by `GSTNLink.GAS.*`. |
| `GSTNLinkNP` | part 1 (F10; Runtime, Default) | `FGSTN_LinkFragment` (feature id 18: both channels, link actions, grab immunity, mash frame; idle = the list's one bit), `FGSTN_LinkSnapshotContributor` (the link summary, request tags as a mask over `FGSTN_LinkRequestTags`, the body content index; its finalize pass puts a parked role's hurtbox anchor on the partner's baked socket), `FGSTN_LinkTickExtension` (priority 120: request timeouts, the predicted flag of a grab-like hit, the action veto of a non-acting parked role and its timeline cancel, the parked modifier's edges, `Combat.Link.*` tags, `Interact.Target.Grab` through the GAS provider for kits with a link extension), the `LinkResponse` / `LinkCommand` inbox kinds, the `GrabImmunity` and `LinkRequest` steps with their outcome applier, `UGSTN_LinkedMovementMode` + `FGSTN_LinkedModifier` + `UGSTN_LinkedModeTransition`, `FGSTN_LinkAttach` (per-entity content: reference-pose sockets, kit link extension), `UGSTN_LinkPresentationComponent` (`GetLinkedTransform`, mesh on the partner's socket), `FGSTN_LinkDebugDraw` (`gstn.Combat.Debug.Links 1`), `UGSTN_LinkContentLibrary` (harness and content helpers). Part 2 (F11): `UGSTN_LinkWatchdog` (the only break authority: a world subsystem that works on the server only, after the actor tick of every engine frame; it reads every combat entity's finalized state and runs `FGSTN_LinkWatchdogLogic`: Serial-matched pairs through `UGSTN_LinkMath::EvaluateBreak` with the type's break rules, orphans after `OrphanGraceFrames`, the parked side's exit from the type's placer through `FGSTN_WorldExitCollisionQuery`, `LinkCommand{Break}` stamped F to both inboxes, events `LinkBroken` / `GrabEscaped`), the `LinkBreakOnHit` step and its applier (PendingBreak, reason Hit, per the type's `HitBreaks`), `FGSTN_LinkExitRequest` (the teleport as a Mover instant move through `UGSTN_GE_LinkExit` on the frame the command's exit record is written). Needs the GAS-NPP fork. Covered by `GSTNLink.NP.*`. |

The module class (`Private/GSTNLinkCoreModule.cpp`) registers the plugin's `Config/Tags` folder with the tag manager: the engine
reads `Config/Tags` only for the project and for Game Feature plugins, not for ordinary plugins, so the ini vocabulary would
otherwise be missing. It also registers the primary asset type `GSTN_LinkType` with the asset manager once the manager exists, over
the plugin's content and `LinkTypeDirectories` (always cooked), so the project's `DefaultGame.ini` does not list the type: a
`PrimaryAssetTypesToScan` line naming this plugin's class trips an engine ensure in every build where the plugin is disabled.

## Dependencies

- Plugins: `GSTNCombat` (`GSTNCombatCore`), `GSTNCombatGAS`, `GSTNCombatNP`, `GSTNInput` (`GSTNInputCore`), `GSTNInteract`
  (`GSTNInteractCore`, `GSTNInteractRuntime`), and the fork's `GameplayAbilities`, `NetworkPrediction` and `Mover`.
- `GSTNLinkCore` (`Build.cs`): public Core, CoreUObject, Engine, GameplayTags, DeveloperSettings, GSTNInputCore, GSTNCombatCore;
  private Projects (to find the plugin's own folder). No edge to the project module, to the sibling plugin `GSTNDefence`, or to GAS, NP
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
| `UGSTN_LinkTypeDefinition` | A link type, a `PrimaryDataAsset` of primary asset type `GSTN_LinkType`: the type tag, the channel (`Body` parks the entity, `Hand` never does), two `FGSTN_LinkRoleDef`, `FGSTN_LinkBreakRules`, `RequestRequirements`, `ForbiddenWithBody`, `bInitiatorPredicts`, `MaxFrames`, `DebugColor`. `Validate` is pure; `IsDataValid` forwards it. The asset manager finds every definition; code never lists them. |
| `FGSTN_LinkRoleDef` | One role: its tag, `bParked`, `bActsWhileLinked`, `bCollisionWithPartner`, the socket of the partner it is placed on (by name) and the offset. A grab parks role 2; a latch parks role 1. |
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
| `FGSTN_LinkRequestContext` | The const view a rule reads: both entities as values (tags, weight, link slots, immunity frames), the definition, the kit rules. |
| `UGSTN_LinkMath` | `MashInitialFrames`, `MashProgress`, `MashEscaped`, `EvaluateBreak` (one reason wins, in a fixed order), `ParkedTransform` / `TryGetParkedTransform` (same derivation for whichever role is parked), reason tags. |
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
| `Command.Link.Grab`, `.Pummel`, `.Release`, `.Throw.Forward`, `.Back`, `.Up`, `.Down`, `Input.Lock.Command.Link` | The standard command set, abilities and effects |
| `Combat.Event.LinkFormed`, `.LinkBroken`, `.Throw`, `.GrabEscaped` | The watchdog and the combat outbox on the authority |

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
- **A type the interact button forms** (the drill's latch variant): one more `FGSTN_LinkInteractOffer` (definition, `Interact.Target`
  meaning, reach) in the source the link tick extension hands the probe; the offer's rules pre-filter its candidates.
- **A role's animation**: an `AnimsByRole` entry in the kit's (or weapon's) `FGSTN_LinkKitExtension`.
- **A heavier or lighter grabber**: give the kit a `FGSTN_LinkKitExtension` with its own `FGSTN_GrabRules`.
- **A new request rule**: subclass `UGSTN_LinkRule` (C++ or Blueprint), override `Evaluate`, list it in the definition's `Rules`. Call
  `CheckCommon` first.
- **A second latch type** (the drill: a parked initiator on a possibly moving partner): a definition whose `InitiatorRole` is
  `bParked` (and `bActsWhileLinked`), its `SocketOnPartner` and offset, and its own `UGSTN_LinkRule` subclass. The `LinkRequest` step,
  the response, the parked mode, the anchor of the hurtboxes and the presentation are the same code for every type; nothing in
  `GSTNLinkNP` names a type.
- **Formation in the pipeline**: put `UGSTN_HitStep_GrabImmunity` then `UGSTN_HitStep_LinkRequest` after `HurtState` in the rules'
  `HitPipeline` (`UGSTN_LinkContentLibrary::AddLinkStepsToRules` does it); a hit whose data names a registered `LinkType` is a request.
- **The parked mode on a pawn**: author `UGSTN_LinkedModeTransition` as a global transition of the pawn's Mover (or let
  `FGSTN_LinkAttach` add the mode and an entry transition to every mode at attach, as the harness pawns get them).
- **A new exit**: subclass `UGSTN_LinkExitPlacer` (or tune `UGSTN_Placer_Ring` on the definition) and set it as the definition's `ExitPlacer`.

## Settings

`Project Settings > Plugins > GSTN Link` (`UGSTN_LinkSettings`), defaults in `Config/DefaultGSTNLink.ini`:

| Setting | Meaning |
|---|---|
| `RequestTimeoutFrames` | Frames a predicted link waits for its response before the link tick clears it (12). |
| `OrphanGraceFrames` | Frames after a formation during which a one-sided link is not an orphan (3). |
| `LinkTypeDirectories` | Content folders, besides the plugin's own, scanned for link type definitions (empty). Read when the asset manager is created. |

## Tests

`GSTNLink.Core.*`: `Tags.*`, `Data.*` (F5a); `State.*`, `Mash.*`, `Break.*`, `Placer.*`, `Rule.Grab.*`, `Rule.Latch.*`, `FakeHost.*` (F5b). They run with only this plugin and `GSTNCombatCore` loaded: no GAS, no NP, no world.

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

`GSTNLink.NP.*` (F11): `Watchdog.{BreakAtF1BothSides, OrphanAfterGrace, StaleSerialIgnored, OnePathForPipelineBreaks}`,
`Break.{Distance, Timeout, Death, PartnerState, HitByRole}`, `Exit.{NeverInsideGeometry, DeterministicForEqualInputs}`: the watchdog's pure
function on entity views, its commands applied through the real inbox rule, the exit placer with a fake world.

Harness: `Tools/GASNPPTest/link_scenarios.py` (`SUITE_LINK`: `link_spike_*`, `link_rollback_*` (formation through a grab hit, a watchdog break
with forced reconciles over the break frames) and `link_break_distance_*` (a leash and the grabber moved away, then a knock-out of either side) in
three pairings, plus `grab_immunity`; `link_probe_*` are Task A's walk probes, not in the suite), content from
`Tools/GAS/setup_defence_link_content.py` `run_link()` (`/GSTNLink/Test/DA_LinkType_Grab_Test`).
