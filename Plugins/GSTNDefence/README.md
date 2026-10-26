# GSTNDefence

Defence for the GSTN combat on Unreal Engine 5.8: a **fixed-size shield bubble**, the **parry** (one resolution, two sources: the
shield's perfect shield and a move's parry window), the optional **tech**, the downed state and get-up, and the **DI and SDI**
rules, as data and pure integer-safe math. An entity whose kit and mode carry no defence data behaves as it did before the plugin.

Status: **experimental (0.1)**, `EnabledByDefault: false`. Checkpoints F3 and F4 of
[m6-defence-links-plan.md](../../docs/architecture/plans/m6-defence-links-plan.md) are built (skeleton, data, tags, tracks, and the pure
math with goldens). The GAS module (F6) and the Network Prediction module (F7, F8) are not built. The design is
[combat-m6-defence-links.md](../../docs/architecture/combat-m6-defence-links.md).

## The shield decision (owner, 2026-10-26)

The shield is a **spherical bubble of fixed size**, as in Rivals of Aether 2. There is **no stick tilt, no shrink with the
shield's health, no directional arc, no poke mechanic and no `ShieldPoked` event**. The shield's health only decides when the shield
breaks and how hard a blocked hit stuns and pushes. The bubble stays an extra hurtbox of the entity (the extra-hurtbox seam of
`GSTNCombat`), so the bubble at any past frame follows from the rules and the entity transform alone. The fields that would have
carried tilt, shrink or arc (`ArcDegrees`, `ElevationLimit`, `TurnRate`, `RadiusByFraction`, `Tilt*`) do not exist.

## Modules

| Module | State | What it is |
|---|---|---|
| `GSTNDefenceCore` | exists (Runtime, Default) | Native tags, settings, the rules asset and its structs (shield, parry, tech, DI, SDI, landing), the integer curve, the kit and rules extensions, the timeline tracks, and the pure math (shield value and bubble, parry, tech, DI, SDI, ground bounce). No GAS, Network Prediction or Mover, so it needs no fork. Covered by `GSTNDefence.Core.*`. |
| `GSTNDefenceGAS` | not built (F6) | Attribute mirror, shield / tech / ground bounce / get-up abilities, effects. Stock GAS. |
| `GSTNDefenceNP` | not built (F7, F8) | Sync fragment, shield bubble contribution, `Parry` and `Shield` steps, tech and landing, DI and SDI wiring. Needs the GAS-NPP fork. |

The module class (`Private/GSTNDefenceCoreModule.cpp`) registers the plugin's `Config/Tags` folder with the tag manager: the engine
reads `Config/Tags` only for the project and for Game Feature plugins, not for ordinary plugins, so the ini vocabulary would
otherwise be missing.

## Dependencies

- Plugins: `GSTNCombat` (`GSTNCombatCore`) and `GSTNInput` (`GSTNInputCore`).
- `GSTNDefenceCore` (`Build.cs`): public Core, CoreUObject, Engine, GameplayTags, DeveloperSettings, GSTNInputCore, GSTNCombatCore;
  private Projects (to find the plugin's own folder). Engine, DeveloperSettings and Projects are engine modules the data assets and
  settings need; the project's own rule (Core lists no GAS, NP, Mover or Link module) holds. No edge to the project module, to
  the sibling plugin `GSTNLink`, or to GAS, NP and Mover.
- `GSTNCombat` builds and passes its tests with this plugin absent: the plugin adds behaviour by registering into the combat
  extension surface (modularity-and-tags.md 5b), never by editing the combat core.

## Data

| Type | What it is |
|---|---|
| `UGSTN_DefenceRules` | The defence data of a game mode: `bShieldEnabled` (PvP yes, PvE no), `FGSTN_ShieldRules`, `FGSTN_ParryRules`, `ParryProfiles` (never empty; profile 0 is the shield's), `FGSTN_TechRules`, `FGSTN_DIRules`, `FGSTN_SDIRules`, `FGSTN_GroundBounceRules`. `Validate` is pure; `IsDataValid` forwards it. |
| `FGSTN_ShieldRules` | The bubble (`RadiusCm`, `CentreHeightCm`), timing, health (`MaxShield`, drain, regen and its delay), the break, and the reaction to a blocked hit (stun, hitstop, pushback). |
| `FGSTN_ParryRules`, `FGSTN_ParryProfile` | The shield's perfect shield (anchor `Release` or `Press`, window, minimum held frames) and what a parry gives, shared by both parry sources. |
| `FGSTN_TechRules` | The optional tech; matters only to a kit with a non-empty `FGSTN_TechSet`. |
| `FGSTN_DIRules`, `FGSTN_SDIRules` | DI parameters, strength curves, victim scales; the held-direction SDI drift. |
| `FGSTN_GroundBounceRules` | The landing: stick-steered mini bounce, then the downed state; `MaxDownedFrames` 0 = never. |
| `FGSTN_Curve1D` | Piecewise-linear curve with integer points (X in 1/4096, Y in Q2.14), integer interpolation; empty = identity. No engine curve. |
| `FGSTN_DefenceKitExtension` | Entry of a kit's extension array: own rules, `ShieldAbility` (empty = no shield), `FGSTN_TechSet` (empty = no tech), `FGSTN_GetupSet`. Ability classes are soft `UObject` classes because Core has no GameplayAbilities dependency. |
| `FGSTN_DefenceRulesExtension` | Entry of the combat rules asset's extension array naming the mode's `UGSTN_DefenceRules`. |
| `FGSTN_HoldWindowTrack`, `FGSTN_ParryWindowTrack` | Timeline track structs (children of `FGSTN_TimelineTrack`). The parry track's `ProfileName` is baked to `ProfileIndex` by `Bake`; the core's closed track kind enum is untouched (the combat clock finds them by struct). |

## Math

Stateless, deterministic functions (doubles and integers, no world, actor or component reads); the same inputs give the same result
on every machine and in a resimulation. Views (`GSTN_DefenceViews.h`) are plain structs holding frames and flags, never tags.

| Library | What it does |
|---|---|
| `UGSTN_DefenceMath` | `IsShieldAvailable` (mode flag x kit), the analytic shield value (`ShieldValueAt`, `RaiseShield`, `ReleaseShield`, `ApplyShieldDamage`, `RestampShield` for a rate change), `ShieldDamageOf`, the fixed bubble (`ShieldCentreAt` along the entity's local up, `MakeShieldBubble`, `MakeShieldBubbleHurtbox` as the extra hurtbox), `ShieldStunFrames`, `PushbackSpeedOf`, `EvaluateParryWindow` (anchor `Release` or `Press`), `ResolveParry` (one decision for the sources shield and move, shield first), `MoveWindowEndAfterParry`, `AttackerStunFramesOf`, the by-name tag reads (`HasParryImmunity`, `IsGrabLikeHitType`), the tech press rule and `EvaluateTech`, and `GroundBounceVelocity`. |
| `UGSTN_DIRulesMath` | The rules overload of M4's `ApplyDI`: strength curve, elevation curve, minimum knockback, victim scales, the hit's DI switch. |
| `UGSTN_SDIMath` | `NextDisplacement`: one frame of held-direction drift, limited to `MaxDistanceCmPerHit`. |

The bubble functions take no shield value and no stick: the radius is `RadiusCm` at any value (the fixed-bubble decision).
Deferred to F8, which has the contact record: picking the tech kind (in place, roll, wall) from the stick and the contact normal.

## Tags

All native tags are declared in `GSTNDefenceCore` (`GSTN_DefenceTags.h`), one doc line each. This is an accepted layout: the GAS and
NP modules define no tag, and the "owner" column of the spec names the module that writes a tag, not the declaring module.
Designer vocabulary (the `GameplayCue.Combat.*` cues) is ini only: `Config/Tags/GSTNDefence.ini`.

| Tag | Written by |
|---|---|
| `Combat.State.Shielding`, `.ShieldStun`, `.ShieldBroken`, `.ParryWindow`, `.ParryRecovery`, `.Parried`, `.Teching`, `.Knockdown`, `.Tech.Lockout` | Projected by the defence tick extension (F7), never granted |
| `Combat.Mover.Tumble` | The tumble contact modifier (F8) |
| `Combat.Immunity.Parry`, `Combat.Immunity.Shield` | Kits, abilities, effects (ordinary counts) |
| `Command.Defence.Shield`, `Input.Lock.Command.Defence` | The standard command set (F6), abilities and effects |
| `Combat.Event.Blocked`, `.Parried`, `.ShieldBroken`, `.Teched`, `.KnockedDown` | The combat outbox on the authority |
| `Combat.SetByCaller.ShieldDamage` | The damage code (effect magnitude key) |

The plugin declares no `Combat.Link.*`, `Combat.Part.*` or `Interact.*` tag and reads those by name. No tag is ever stored in a sync
state or a snapshot: only rules, profile and tech indices.

## How to extend

- **A different shield** (size, drain, perfect-shield window, none): give the kit a `FGSTN_DefenceKitExtension` with its own
  `UGSTN_DefenceRules`; `WindowFrames = 0` removes the perfect shield. No ability, step or code change.
- **A counter stance**: a timeline ability with a `FGSTN_ParryWindowTrack` (own profile in `ParryProfiles`) and an `OnParry` cancel
  window into the counter-attack.
- **A game without a shield** (PvE): a rules asset with `bShieldEnabled = false`. Parry still works.
- **A new tech kind or get-up attack**: an entry in the kit's `FGSTN_TechSet` or `FGSTN_GetupSet`.

## Settings

`Project Settings > Plugins > GSTN Defence` (`UGSTN_DefenceSettings`), defaults in `Config/DefaultGSTNDefence.ini`:

| Setting | Meaning |
|---|---|
| `DefaultRules` | Rules asset for a mode whose combat rules carry no defence extension. Empty = no defence rules. |

## Tests

`GSTNDefence.Core.*`: `Tags.*`, `Data.*` (F3); F4 adds `Shield.*`, `Parry.*`, `Tech.*`, `DI.*`, `SDI.*`, `Bounce.*`. They run with only
this plugin and `GSTNCombatCore` loaded: no GAS, no NP, no world.
