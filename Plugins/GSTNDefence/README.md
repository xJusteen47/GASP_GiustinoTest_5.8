# GSTNDefence

Defence for the GSTN combat on Unreal Engine 5.8: a **fixed-size shield bubble**, the **parry** (one resolution, two sources: the
shield's perfect shield and a move's parry window), the optional **tech**, the downed state and get-up, and the **DI and SDI**
rules, as data and pure integer-safe math. An entity whose kit and mode carry no defence data behaves as it did before the plugin.

Status: **experimental (0.1)**, `EnabledByDefault: false`. Checkpoints F3, F4 and F6 of
[m6-defence-links-plan.md](../../docs/architecture/plans/m6-defence-links-plan.md) are built (skeleton, data, tags, tracks, the pure
math with goldens, and the ability-system half: the attribute mirror, the shield effects, the loadout contribution and the
decisions the abilities make). The Network Prediction module (F7, F8), which also holds the ability classes, is not built. The design is
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
| `GSTNDefenceGAS` | exists (Runtime, Default) | `UGSTN_DefenceAttributeSet` (the shield mirror and the numbers the model reads), the effects `UGSTN_GE_ShieldDamage` (mirror), `UGSTN_GE_ShieldRestore` (request) and `UGSTN_GE_ShieldBreak` (cosmetic), the shield ability's timeline as data (`FGSTN_ShieldAbilitySpec`), and `UGSTN_DefenceLibrary`: availability, the shield `CanExecute` decision, the get-up requirement, the kit extension resolution and the loadout contribution (registered as a `FGSTN_LoadoutExtension` of `GSTNCombatGAS`). Stock GAS, no fork. Covered by `GSTNDefence.GAS.*`. |
| `GSTNDefenceNP` | not built (F7, F8) | Sync fragment, shield bubble contribution, `Parry` and `Shield` steps, tech and landing, DI and SDI wiring, and the ability classes (`UGSTN_GA_Shield`, `UGSTN_GA_Tech`, `UGSTN_GA_GroundBounce`, `UGSTN_GA_GetupRoll`, `UGSTN_GA_GetupNeutral`), which derive from `UGSTN_GA_BaseCombat` of `GSTNCombatNP` and plug the GAS module's data and decisions in. Needs the GAS-NPP fork. |

The module class (`Private/GSTNDefenceCoreModule.cpp`) registers the plugin's `Config/Tags` folder with the tag manager: the engine
reads `Config/Tags` only for the project and for Game Feature plugins, not for ordinary plugins, so the ini vocabulary would
otherwise be missing.

## Dependencies

- Plugins: `GSTNCombat` (`GSTNCombatCore`), `GSTNCombatGAS`, `GSTNInput` (`GSTNInputCore`) and `GameplayAbilities`.
- `GSTNDefenceCore` (`Build.cs`): public Core, CoreUObject, Engine, GameplayTags, DeveloperSettings, GSTNInputCore, GSTNCombatCore;
  private Projects (to find the plugin's own folder). Engine, DeveloperSettings and Projects are engine modules the data assets and
  settings need; the project's own rule (Core lists no GAS, NP, Mover or Link module) holds. No edge to the project module, to
  the sibling plugin `GSTNLink`, or to GAS, NP and Mover.
- `GSTNDefenceGAS` (`Build.cs`): public Core, CoreUObject, Engine, GameplayTags, GameplayAbilities, GSTNInputCore, GSTNCombatCore,
  GSTNCombatGAS, GSTNDefenceCore. No NetworkPrediction, AbilitySystemSimulation, Mover, GSTNCombatNP, GSTNLink, Editor or project
  module. The ability classes therefore live in the NP module: `UGSTN_GA_BaseCombat` is a `GSTNCombatNP` class.
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
| `FGSTN_GetupSet` | The landing and get-up classes of a kit: `GroundBounce` (started Forced at the contact), `Roll` (Forced from the stick), `Neutral` (bound to `Command.Movement.Jump` while downed), `Attacks` (0..N, granted). |

### Ability-system half (`GSTNDefenceGAS`)

| Type | What it is |
|---|---|
| `UGSTN_DefenceAttributeSet` | `ShieldHealth` (a MIRROR of the model's analytic value, written by the model at sparse events), `MaxShieldHealth`, `ShieldRegenRate`, `ShieldDrainRate` (read by the model at the end of F-1), `IncomingShieldDamageMultiplier`, `GrabResistance`. Clamped in `PreAttributeChange` and `PreAttributeBaseChange` (`ShieldHealth` in `[0, Max]`, the rest at least 0); a dropping maximum pulls the health down. Granted by the loadout only where `IsShieldAvailable` holds, initialised from the kit's or the mode's rules. |
| `UGSTN_GE_ShieldDamage` | Instant, one additive modifier on `ShieldHealth` of minus the set-by-caller `Combat.SetByCaller.ShieldDamage` (through `UGSTN_ShieldDamageMagnitude` with a -1 coefficient). `Apply(ASC, Loss)` and `ApplyMirror(ASC, Value)` (the loss is base minus value); the mirror only, never the simulation's value. A Blueprint child adds the hit cue. |
| `UGSTN_GE_ShieldRestore` | Instant, additive on `ShieldHealth` by the set-by-caller NAME key `GetAmountKey()` (no new tag). A pickup applies it; the model reads `RequestedDelta(ASC, LastMirrored)` at its next tick (the mirror's base value minus the last value it mirrored) and applies that as a request, then mirrors the result. |
| `UGSTN_GE_ShieldBreak` | Instant, empty: a Blueprint child adds the break cue and presentation tags. The break state itself is sync state (`Combat.State.ShieldBroken` is projected). |
| `FGSTN_ShieldAbilitySpec`, `FGSTN_OutOfShieldOption`, `EGSTN_ShieldRefusal` | The shield ability's timeline as data: `BuildTimeline(ShieldRules)` gives startup frames, ONE `FGSTN_HoldWindowTrack` on `Input.Button.Shield` with `bEndOnZeroShield`, release frames, and one M3 cancel window per out-of-shield option over the hold window. The refusal reasons of `CanExecuteShield`. |
| `UGSTN_DefenceLibrary` | `ResolveRules` (kit extension, else the match's combat rules extension, else the settings' default), `IsShieldAvailableForKit`, `CanExecuteShield` (available, value > 0, not breaking, grounded), `GetupNeutralRequirements` (`Combat.State.Knockdown`), `ResolveAbilityClass` / `ResolveTechSet` / `ResolveGetupSet` (class-checked, a non-ability is dropped with a warning), `MakeShieldBindingInput` (`Command.Defence.Shield`, grounded, slot override `Press(Shield)` + `Hold(Shield, 0)`), `MakeGetupNeutralBindingInput`, and the loadout extension: `ContributeToLoadout`, `GrantDefence`, `RevokeDefence`. |

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

## Loadout contribution (spec 1d)

The GAS module registers a `FGSTN_LoadoutExtension` (id `GSTNDefence`) with `GSTNCombatGAS`'s `FGSTN_LoadoutExtensionRegistry` at
module startup. For a kit whose extension list carries a `FGSTN_DefenceKitExtension`:

- `ContributeToLoadout` adds the `Command.Defence.Shield` binding **only where `IsShieldAvailable(rules, kit has a shield)`** (the mode's
  `bShieldEnabled` and the kit's `ShieldAbility`); otherwise `Input.Button.Shield` stays free for the kit's own binding. It adds the
  neutral get-up binding (`Command.Movement.Jump` requiring `Combat.State.Knockdown`) and the Forced-started classes (ground bounce,
  roll, get-up attacks, the tech set) as triggered abilities, so they are granted and enter the timeline registry.
- `GrantDefence` adds `UGSTN_DefenceAttributeSet` initialised from the rules where the shield is available, and nothing otherwise
  (`RevokeDefence` undoes it). A kit without the extension adds nothing; a disabled plugin registers nothing.

Every contribution depends only on the resolved assets, so every machine compiles the same table.

## How to extend

- **A different shield** (size, drain, perfect-shield window, none): give the kit a `FGSTN_DefenceKitExtension` with its own
  `UGSTN_DefenceRules`; `WindowFrames = 0` removes the perfect shield. No ability, step or code change.
- **A counter stance**: a timeline ability with a `FGSTN_ParryWindowTrack` (own profile in `ParryProfiles`) and an `OnParry` cancel
  window into the counter-attack.
- **A game without a shield** (PvE): a rules asset with `bShieldEnabled = false`. Parry still works.
- **A new tech kind or get-up attack**: an entry in the kit's `FGSTN_TechSet` or `FGSTN_GetupSet` (the loadout grants it).
- **A shield pickup**: apply `UGSTN_GE_ShieldRestore` (`Apply(ASC, Amount, Instigator)`); the model consumes the request.
- **Shield presentation**: Blueprint children of `UGSTN_GE_ShieldDamage` and `UGSTN_GE_ShieldBreak` with the ini cues
  (`GameplayCue.Combat.Shield.Hit`, `.Break`); the rules asset names them.

## Settings

`Project Settings > Plugins > GSTN Defence` (`UGSTN_DefenceSettings`), defaults in `Config/DefaultGSTNDefence.ini`:

| Setting | Meaning |
|---|---|
| `DefaultRules` | Rules asset for a mode whose combat rules carry no defence extension. Empty = no defence rules. |

## Tests

`GSTNDefence.GAS.*` (F6): `AttributeClamp`, `ShieldBindingOmittedWhenUnavailable`, `ShieldAbilityRefusesWhenUnavailable`, `TechSetPerKit`,
`GetupNeutralRequiresKnockdown`, `ShieldRestoreSeenAsRequest`: on a stock ability system in a transient world, no NP, no asset from disk.

`GSTNDefence.Core.*`: `Tags.*`, `Data.*` (F3); F4 adds `Shield.*`, `Parry.*`, `Tech.*`, `DI.*`, `SDI.*`, `Bounce.*`. They run with only
this plugin and `GSTNCombatCore` loaded: no GAS, no NP, no world.
