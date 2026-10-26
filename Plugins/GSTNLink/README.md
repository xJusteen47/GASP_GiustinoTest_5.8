# GSTNLink

Links between two combat entities for the GSTN combat on Unreal Engine 5.8: **grabs, pummels and throws**, and **latches onto large
enemies**, as data-driven *link types* with roles, request rules, break rules, exit placers and mash, all pure and integer-safe. An
entity whose hits and kits carry no link type behaves as it did before the plugin.

Status: **experimental (0.1)**, `EnabledByDefault: false`. Checkpoint F5a of
[m6-defence-links-plan.md](../../docs/architecture/plans/m6-defence-links-plan.md) is built (skeleton, tags, settings, link type
definitions, rules data, the outcome track). The rules, placers, state types, math and the fake host (F5b), the GAS module (F9) and
the Network Prediction module (F10 onwards) are not built. The design is
[combat-m6-defence-links.md](../../docs/architecture/combat-m6-defence-links.md).

## Modules

| Module | State | What it is |
|---|---|---|
| `GSTNLinkCore` | exists (Runtime, Default) | Native tags, settings, the link type definition asset and the rules structs (grab, latch, mash, break), the kit extension and the outcome track. No GAS, Network Prediction or Mover, so it needs no fork. Covered by `GSTNLink.Core.*`. |
| `GSTNLinkGAS` | not built (F9) | Link registry (type tag to wire index), requirement checks, interact provider, animation selection. Stock GAS. |
| `GSTNLinkNP` | not built (F10 onwards) | Link sync fragment, the `LinkRequest` / `GrabImmunity` / `LinkBreakOnHit` steps, the parked movement mode, the watchdog. Needs the GAS-NPP fork. |

The module class (`Private/GSTNLinkCoreModule.cpp`) registers the plugin's `Config/Tags` folder with the tag manager: the engine
reads `Config/Tags` only for the project and for Game Feature plugins, not for ordinary plugins, so the ini vocabulary would
otherwise be missing.

## Dependencies

- Plugins: `GSTNCombat` (`GSTNCombatCore`) and `GSTNInput` (`GSTNInputCore`).
- `GSTNLinkCore` (`Build.cs`): public Core, CoreUObject, Engine, GameplayTags, DeveloperSettings, GSTNInputCore, GSTNCombatCore;
  private Projects (to find the plugin's own folder). No edge to the project module, to the sibling plugin `GSTNDefence`, or to GAS, NP
  and Mover.
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
  plugin that ships it. No change in this plugin. A definition whose tags are not registered fails validation and is skipped.
- **A heavier or lighter grabber**: give the kit a `FGSTN_LinkKitExtension` with its own `FGSTN_GrabRules`.

## Settings

`Project Settings > Plugins > GSTN Link` (`UGSTN_LinkSettings`), defaults in `Config/DefaultGSTNLink.ini`:

| Setting | Meaning |
|---|---|
| `RequestTimeoutFrames` | Frames a predicted link waits for its response (12). |
| `OrphanGraceFrames` | Frames after a formation during which a one-sided link is not an orphan (3). |

## Tests

`GSTNLink.Core.*`: `Tags.*`, `Data.*` (F5a). They run with only this plugin and `GSTNCombatCore` loaded: no GAS, no NP, no world.
