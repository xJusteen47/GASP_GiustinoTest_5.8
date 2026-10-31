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
| `GSTNDefenceNP` | exists (Runtime, Default; F7a; F7b adds the steps, F8 the landing) | Sync fragment (feature id 17), shield bubble contribution, defence clocks and tag projection, the hold and parry track hooks, `Parry` and `Shield` steps, tech and landing, DI and SDI wiring, and the ability classes (`UGSTN_GA_Shield`, `UGSTN_GA_Tech`, `UGSTN_GA_GroundBounce`, `UGSTN_GA_GetupRoll`, `UGSTN_GA_GetupNeutral`), which derive from `UGSTN_GA_BaseCombat` of `GSTNCombatNP` and plug the GAS module's data and decisions in. Needs the GAS-NPP fork. | Covered by `GSTNDefence.NP.*`.

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
- `GSTNDefenceNP` (`Build.cs`): public Core, CoreUObject, Engine, GameplayTags, GameplayAbilities, NetworkPrediction,
  AbilitySystemSimulation, Mover, GSTNInputCore, GSTNInputNP, GSTNCombatCore, GSTNCombatGAS, GSTNCombatNP, GSTNDefenceCore, GSTNDefenceGAS.
  Needs the GAS-NPP fork. No GSTNLink, Editor or project module.
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
| `UGSTN_DefenceLibrary` | `ResolveRules` (kit extension, else the match's combat rules extension, else the settings' default), `IsShieldAvailableForKit`, `CanExecuteShield` (available, value > 0, not breaking, grounded), `GetupNeutralRequirements` (`Combat.State.Knockdown`), `ResolveAbilityClass` / `ResolveTechSet` / `ResolveGetupSet` (class-checked, a non-ability is dropped with a warning), `MakeShieldBindingInput` (`Command.Defence.Shield`, grounded, slot override `Press(Shield)`; F7 dropped the `Hold(Shield, 0)` of F6, which the loadout validator refused), `MakeGetupNeutralBindingInput`, and the loadout extension: `ContributeToLoadout`, `GrantDefence`, `RevokeDefence`. |

### Network Prediction half (`GSTNDefenceNP`)

| Type | What it is |
|---|---|
| `FGSTN_DefenceFragment` | Combat sync fragment `GSTNFeatureIds::Defence` (17): the analytic shield (value in quarter points at a stamp frame, `Raised`/`Breaking`/`Empty` flags, press and release frames; no yaw, no tilt), the move parry window and its profile and hurtbox mask, `LastParryFrame`, the stuns as end-frame stamps (shield stun, parry recovery, break, punish), tech, knockdown and grab immunity stamps (F8, F12), a queued pushback and the mirror bookkeeping. Zero bytes at its defaults, a presence mask and the live groups otherwise. |
| `FGSTN_DefenceSnapshotExt`, `FGSTN_DefenceSnapshotContributor` | The 32 byte snapshot POD (rules index, availability, raised/blocks/bubble/breaking flags, the value at the snapshot frame, press and release frames, the move window, immunity bits) and its contributor; while the bubble exists (press to release + `ReleaseFrames`, not in a hit reaction, not broken) the bubble is added as an extra hurtbox (index range `>= 0x80`) that ignores the non-blockable and grab-like hit types. |
| `FGSTN_DefenceTickExtension` | Priority 110: the defence clocks (break recovery, `Empty` from decay, restore requests, the mirror every 16 frames while the value moves, the pushback start), the hold and parry stamps turned into raise, release and the move window, `BlocksActions` (shield stun, broken, punished), `AdmitsOnlyBindingsRequiring` (`Combat.State.Knockdown` while downed), the `OnParry` held cancel condition, the projection of the `Combat.State.*` defence tags, and the F8 hooks below. |
| `FGSTN_TumbleLanding` (F8) | The landing from a tumble and the optional tech, on the core's tumble contact hooks: a floor contact with no surface impact ends the reaction, gives the stick-steered mini bounce and the kit's ground bounce (Forced), then the downed state (`KnockdownEndFrame`: `MaxDownedFrames`, 0 = never; after a missed tech the spec's `KnockdownFrames` + bonus) with a movement lock (`UGSTN_GE_DownedLock`); holding the stick starts the get-up roll, a get-up command (neutral on the jump slot, get-up attacks) ends it. With a non-empty tech set: presses of `Input.Button.Shield` from the history (lockout, window), the kind from the stick and the contact (`PickTechType`), success consumes the contact (no impact) and starts the kit's tech ability Forced, failure lets the impact resolve first. |
| `FGSTN_DIWiring` (F8) | The DI rules overload at the end of a launch's hitstop (`OverrideDI`, `UGSTN_DIRulesMath::ApplyDI`), and the per-hit `HitFlags` (DI disabled, SDI blocked: throw, armor, stagger-in-place tier per the rules) the outcome applier records. |
| `FGSTN_SDIDrift`, `UGSTN_GE_SDIDrift` (F8) | Held-direction SDI: each hitstop frame of a staggered victim but the last, `UGSTN_SDIMath::NextDisplacement` against the core's `Hit.SDIDistanceUsed`, given to Mover as a one-frame drift freeze (`FGSTN_LayeredMove_Freeze::DriftVelocity`, priority above the hitstop freeze) so collision stops it. |
| `FGSTN_HoldWindowHook`, `FGSTN_ParryWindowHook` | Track hooks of `FGSTN_HoldWindowTrack` (stalls the timeline ON the hold frame while the button is held or the shield is stunned; never the combat clock) and `FGSTN_ParryWindowTrack` (stamps the window in simulation frames when the track starts). |
| `FGSTN_DefenceAttach`, `UGSTN_DefenceContentLibrary` | The content an entity plays with (rules, kit has a shield, bubble ignore types), attached from the loadout on every machine or by the harness (`AttachDefenceToActor`); an append-only rules table gives the snapshot's rules index. |
| `UGSTN_GA_Shield`, `UGSTN_GA_Tech`, `UGSTN_GA_GroundBounce`, `UGSTN_GA_GetupRoll`, `UGSTN_GA_GetupNeutral` | Thin `UGSTN_GA_BaseCombat` children. The shield builds its timeline from `FGSTN_ShieldAbilitySpec` and refuses through `CanExecuteShield`; the tech, ground bounce and roll are started Forced by the model, the neutral get-up by its command (F8). The four landing/tech classes have a plain `TotalFrames` timeline (20/24/30/20) a child replaces with its own tracks. None writes sync state. |

## Math

Stateless, deterministic functions (doubles and integers, no world, actor or component reads); the same inputs give the same result
on every machine and in a resimulation. Views (`GSTN_DefenceViews.h`) are plain structs holding frames and flags, never tags.

| Library | What it does |
|---|---|
| `UGSTN_DefenceMath` | `IsShieldAvailable` (mode flag x kit), the analytic shield value (`ShieldValueAt`, `RaiseShield`, `ReleaseShield`, `ApplyShieldDamage`, `RestampShield` for a rate change), `ShieldDamageOf`, the fixed bubble (`ShieldCentreAt` along the entity's local up, `MakeShieldBubble`, `MakeShieldBubbleHurtbox` as the extra hurtbox), `ShieldStunFrames`, `PushbackSpeedOf`, `EvaluateParryWindow` (anchor `Release` or `Press`), `ResolveParry` (one decision for the sources shield and move, shield first), `MoveWindowEndAfterParry`, `AttackerStunFramesOf`, the by-name tag reads (`HasParryImmunity`, `IsGrabLikeHitType`), the tech press rule and `EvaluateTech`, and `GroundBounceVelocity`. |
| `UGSTN_DIRulesMath` | The rules overload of M4's `ApplyDI`: strength curve, elevation curve, minimum knockback, victim scales, the hit's DI switch. |
| `UGSTN_SDIMath` | `NextDisplacement`: one frame of held-direction drift, limited to `MaxDistanceCmPerHit`. |

The bubble functions take no shield value and no stick: the radius is `RadiusCm` at any value (the fixed-bubble decision).
The tech kind (in place, roll, wall, wall jump) is picked in `GSTNDefenceNP` (`FGSTN_TumbleLanding::PickTechType`), where the contact record is.

## Tags

All native tags are declared in `GSTNDefenceCore` (`GSTN_DefenceTags.h`), one doc line each. This is an accepted layout: the GAS and
NP modules define no tag, and the "owner" column of the spec names the module that writes a tag, not the declaring module.
Designer vocabulary (the `GameplayCue.Combat.*` cues) is ini only: `Config/Tags/GSTNDefence.ini`.

| Tag | Written by |
|---|---|
| `Combat.State.Shielding`, `.ShieldStun`, `.ShieldBroken`, `.ParryWindow`, `.ParryRecovery`, `.Parried`, `.Teching`, `.Knockdown`, `.Tech.Lockout` | Projected by the defence tick extension (F7), never granted |
| `Combat.Mover.Tumble` | Reserved: F8 reads M4's knockback probe record and adds no contact modifier (deltas F8) |
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

## Pipeline steps (`GSTNDefenceNP`, spec 9 steps 5 and 6)

| Step | What it does |
|---|---|
| `UGSTN_HitStep_Parry` | One `ResolveParry` for both sources (perfect shield on the bubble, a move's `ParryWindow` track) at the hit's stamped frame: nullify, the profile's hitstop on both sides, a `Punish` inbox entry back to the attacker (none when the profile has no attacker stun), `Combat.Event.Parried` (magnitude = source). Runs in every mode. |
| `UGSTN_HitStep_Shield` | Only where the shield is available and blocks at the hit frame: a hit on the bubble is blocked (shield damage, stun, push back, hitstop x `ShieldHitstopMultiplier`, `Combat.Event.Blocked`) or breaks it (`Combat.Event.ShieldBroken`, an upward launch that never tumbles); a body hit while raised continues as an ordinary hit. |

`UGSTN_DefenceContentLibrary::AddDefenceStepsToRules` puts both after `HurtboxReaction` (Parry first). `FGSTN_DefenceOutcomeApplier` writes the
fragment at the hit frame plus one; the `Punish` kind (`GSTNDefenceInboxKinds::Punish` = 16) punishes the attacker and cancels its move.

## Harness

`Tools/GASNPPTest/defence_scenarios.py` (`SUITE_DEFENCE`): `shield_block`, `shield_bubble`, `shield_oos`, `shield_break`, `shield_mode_off`,
`grab_shield` (a stub until F12), `parry`, `parry_move`, and (F8) `landing_bounce`, `landing_getup`, `tech_ground`, `tech_wall`, `sdi`, each in a
fresh PIE through `defence_batch()`. The F8 kit content is set in memory by `UGSTN_DefenceContentLibrary::SetKitDefenceMoves` (the plugin's native
landing and tech abilities). Content: `Tools/GAS/setup_defence_link_content.py`
writes `/GSTNDefence/Test/DA_DefenceRules_Test` (PvP) and `DA_DefenceRules_TestPvE`; the kit, input config and combat rules are adjusted in memory
(never saved); the abilities are `UGSTN_GA_Shield` and `UGSTN_GA_CounterSample` (a counter stance sample with a parry window and an `OnParry`
cancel).

## Tests

`GSTNDefence.NP.*` (F7): `Fragment.*`, `Snapshot.*`, `Tick.*`, `Steps.*` (17 tests, list in docs/notes/m6-doc-deltas.md F7). F8 adds
`Landing.{BounceSteered, DownedThenGetupRoll, GetupAttackFromSlot, JumpGetsUp, MaxDownedFrames, TechSuccessCancelsImpact, TechFailureResolvesImpactFirst,
NoTechSetNoLockout}`, `DI.RulesOverloadApplied`, `SDI.{DriftSweptByMover, MaxDistancePerHit, ThrowBlocksSDI}` (12).

`GSTNDefence.GAS.*` (F6): `AttributeClamp`, `ShieldBindingOmittedWhenUnavailable`, `ShieldAbilityRefusesWhenUnavailable`, `TechSetPerKit`,
`GetupNeutralRequiresKnockdown`, `ShieldRestoreSeenAsRequest`, and (F7) `ShieldBindingMatchesPress`: on a stock ability system in a transient world, no NP, no asset from disk.

`GSTNDefence.Core.*`: `Tags.*`, `Data.*` (F3); F4 adds `Shield.*`, `Parry.*`, `Tech.*`, `DI.*`, `SDI.*`, `Bounce.*`. They run with only
this plugin and `GSTNCombatCore` loaded: no GAS, no NP, no world.
