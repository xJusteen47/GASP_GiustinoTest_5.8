# GSTNCombatGAS

The ability-system layer of [GSTNCombat](../GSTNCombat/README.md). It is its own plugin so that `GSTNCombat`
(`GSTNCombatCore`) stays free of GameplayAbilities: a project that only wants the input-facing combat content never loads
the ability system. Design: [docs/architecture/combat.md](../../docs/architecture/combat.md) and
[combat-m3-moves-kits.md](../../docs/architecture/combat-m3-moves-kits.md).

Status: **experimental (0.1)**. It holds the native tags, the two attribute sets, the meter-cost and weapon-stats
effects, the stat/meter data structs, the plugin settings, the kit, weapon and super data assets, and the assembly that feeds them to
the pure binding-table compiler and loadout rules of `GSTNCombat`, and the loadout component, the local-player loadout subsystem
with its save game, and the skin component.

## Dependency rule

```
GSTNCombatCore  <  GSTNCombatGAS  <  GSTNCombatNP
 (no GAS/NP)       (GAS, no NP/Mover)   (fork, NP, Mover)
```

- Module `GSTNCombatGAS` depends on Core, CoreUObject, Engine, NetCore (the replicated loadout component), GameplayTags, GameplayAbilities, GameplayTasks,
  DeveloperSettings, `GSTNCombatCore` and `GSTNInputCore` (the slot and requirement types the bindings use). The descriptor lists
  the plugins `GSTNCombat`, `GSTNInput` and `GameplayAbilities`.
- It never depends on NetworkPrediction, AbilitySystemSimulation, Mover or `GSTNCombatNP`, and nothing here refers to the
  project module, `/Game/` content or a project tag. It compiles against stock GAS and against the GAS-NPP fork in
  `Plugins/GASNPP` (same plugin and module name).
- Nothing in `GSTNCombat` depends on this plugin. `GSTNCombatNP` will list it when it first needs a type from here.

## Tags

Native, declared in `Public/GSTN_CombatTags.h` (namespace `GSTNCombatTags`), one doc line each in
`Private/GSTN_CombatTags.cpp`.

| Subtree | Tags | Notes |
|---|---|---|
| `Combat.State.*` | `Busy InTimeline SuperInput CancelWindow OverrideWindow Falling` | Projected by the combat tick, never granted by an ability or effect. |
| `Combat.Ability.*` | `Timeline AwaitingAdoption` | Ability identity. |
| `Combat.Source.*` | `Command Forced Adopted` | How a timeline started (`EGSTN_ActivationSource`), on the `TimelineStarted` event. |
| `Combat.Lock.Movement` | | Read by the stats movement modifier. |
| `Combat.Weapon.Role.*` | `Primary Support` | Granted by the stats effect of the active weapon. |
| `Combat.Event.*` | `TimelineStarted TimelineFinished WeaponSwitched SuperUsed` | Authoritative outbox events. |
| `Combat.SetByCaller.MeterCost` | | Set-by-caller key. |
| Designer roots | `Weapon`, `Kit`, `Super`, `Loadout`, `Skin`, `Combat.Stat`, `Combat.Meter`, `Combat.SetByCaller.Stat` | Roots only, so tag pickers can filter; the leaves are designer content in a tag ini and code never names one. |

Not here: the input vocabulary (`Input.Button/Context/Lock.Command/BufferEvent.*`, `Command.*` families) belongs to
`GSTNInputCore` and `GSTNCombatCore`, and `Combat.Mover.*` to the NP module that sees Mover.

## Attributes, effects and data

| Type | File | Role |
|---|---|---|
| `UGSTN_CombatAttributeSet` | `Attributes/` | Movement and jump stats (`WalkSpeed`, `Acceleration`, `Braking`, `GroundFriction`, `AirControl`, `Weight`, `MaxCarryWeight`, and the jump set `JumpVelocity` ... `JumpSquatFrames`). No health. Every stat is clamped to at least 0, gravity scales to at least `MinGravityScale` (0.05), in both `PreAttributeChange` and `PreAttributeBaseChange` (the fork idiom: the aggregated and the base value agree). Frame counts are floats, read with `Get*FrameCount()` (rounded down). |
| `UGSTN_MeterAttributeSet` | `Attributes/` | Meters as value/max pairs (`ExMeter`/`ExMeterMax`, `SuperMeter`/`SuperMeterMax`). `GetMeterPairs()` is the only place that pairs them: a new meter is two attributes and one line there. Value clamped to `[0, Max]`, max to `>= 0`; an instant effect that lowers a max pulls the value down (`PostGameplayEffectExecute`). |
| `FGSTN_StatValue`, `FGSTN_KitStats` | `Attributes/GSTN_KitStats.h` | A designer number keyed by a `Combat.SetByCaller.Stat.*` leaf. `FGSTN_KitStats::ApplyTo(ASC, InitEffect)` fills the spec and applies the kit's instant init effect; `Validate` reports empty, duplicate and out-of-root tags. |
| `FGSTN_MeterSetup`, `UGSTN_MeterUtils` | `Attributes/GSTN_MeterSetup.h` | `{Meter tag (Combat.Meter), Value, Max, PointsPerBar, Bars}` and BlueprintPure readers (`FindMeterSetup`, `GetFullBars`, `GetMeterAmount`, `GetMeterMax`). |
| `UGSTN_GE_MeterCost` | `Effects/` | Instant cost: pick the meter attribute and set `Cost` (an `FScalableFloat`, positive; a curve table scales it by ability level) on a Blueprint child and the additive modifier is built from both, negated. The cost is data because the stock `CheckCost`/`ApplyCost` build the spec themselves; the ability's cost effect class is how a kit or weapon sets a move's cost. `CheckCost` blocks an unaffordable activation; applying without it stops at 0. |
| `UGSTN_GE_WeaponStats` | `Effects/` | Infinite base of a weapon stats effect. `Apply`/`Remove`/`IsApplied` find the effect by class (no stored handle); `Apply` returns the live effect instead of stacking. One stats class per weapon. |
| `UGSTN_CombatGASSettings` | `Settings/` | Starting values of the attributes by property name (`AttributeDefaults`), defaults in `Config/DefaultGSTNCombatGAS.ini`. A kit init effect overwrites them. |

Modifiers on a stats effect and on the kit init effect read their magnitudes from set-by-caller stat tags, and the number for
each tag lives in `FGSTN_KitStats` / the weapon modifiers, so no C++ names a stat leaf. A kit init effect must override a
meter maximum before the meter.

Blueprint notes: `UGSTN_GE_MeterCost` and `UGSTN_GE_WeaponStats` are `Blueprintable` (children are the designer content);
the structs are `BlueprintType` with `EditAnywhere, BlueprintReadOnly`, tag fields carry `Categories`; the attribute sets
are native only. Applying and removing stats effects is for the loadout, outside the simulation.

## Kit, weapon and super data (`Kits/`)

Primary data assets and the binding type. They live here, not in `GSTNCombatCore`, because they hold ability or effect classes; the
pure companions (command set, loadout data, super input scheme, skin, identity rules) are in
[GSTNCombat](../GSTNCombat/README.md). Every content reference is soft, so naming a weapon, effect or ability loads nothing.

| Type | What it is |
|---|---|
| `FGSTN_AbilityBinding` | `{Command, Requirements, SlotOverride, Ability, TimelineOverride, PriorityOffset, BufferFramesOverride}`: a command bound to an ability for a context. `HasSameKey` = same command and same requirement sets (the order of tags does not matter). The slot override is the input plugin's `FGSTN_InputSlotEntry`, which holds its slot asset by hard pointer: the one hard reference, the input plugin's choice. |
| `UGSTN_AbilityBindingUtils` | `InheritBindings(Parent, Child)` (parent entries in order, each replaced in place by the child entry of the same command and requirements, then the new child entries) and `Validate` (invalid command, duplicate key, override slot of another command, bad buffer; with a command set: binding to a removed command, or to a command neither in the set nor given a slot override). |
| `UGSTN_PrimaryKitDefinition` | `UPrimaryDataAsset` (`GSTN_PrimaryKit`): identity tags, primary weapon, `Stats` + `InitStatsEffect`, `Meters`, `CommandSet`, `KitBindings`, triggered abilities, passives, super slots and scheme, the allowed weapons/supers/skins queries, ricochet fallback and `Extensions`. |
| `UGSTN_WeaponDefinition` | `UPrimaryDataAsset` (`GSTN_Weapon`): identity tags, `ParentWeapon`, `Bindings` (inherited along the parent chain by `GetEffectiveBindings`), `AllowedAlternatives`, one `StatsEffect` class plus `StatModifiers` (`FGSTN_WeaponStatModifiers` is `TArray<FGSTN_StatValue>`), passives, `Parameters`, anim layer, visual actor, ricochet fallback, `Extensions`. |
| `UGSTN_SuperDefinition` | `UPrimaryDataAsset` (`GSTN_Super`): ability (+ timeline override), `SuperTags`, meter tag and cost, input code, owner requirements. |

`IsDataValid` of each asset runs its `Validate` (also callable from tests and tools, no editor needed) and, for kits and weapons,
compares against the other registered assets of the type. Errors: a binding to a command the command set removes (or neither in the
set nor given a slot override); a duplicate command+requirements pair; an override slot of another command; **the identity rule**
(`KitTags` / `WeaponTags` hold exactly one leaf under the kit / weapon root, zero or several is an error, the other tags only classify,
unique among the assets of the type); a weapon whose stat modifiers have no stats effect; one stats effect class used by two weapons
(it is removed by class); super slots without a scheme; a super without ability or a cost without a meter; a parent chain that loops.
The roots come from `GSTNCombatTags` (`Root_Kit`, `Root_Weapon`); the leaves are designer content and no C++ names one.

**Registering the types.** The uniqueness checks and the asset manager lookups need the types in the project's
`Config/DefaultGame.ini` (`PrimaryAssetTypesToScan` for `GSTN_PrimaryKit`, `GSTN_Weapon`, `GSTN_Super`, `GSTN_SuperInputScheme`,
`GSTN_Skin` and `GSTN_CombatTimeline`); the plugin does not edit the project's config, and until a type is registered nothing is
compared across assets.

**Extending in Blueprint.** Kits, weapons and supers are data assets created from the Content Browser (Miscellaneous > Data Asset);
a designer fills the details panel and runs Validate. Abilities and effects are the Blueprint classes the soft fields point at
(`UGSTN_GE_WeaponStats` children for stats effects). `UGSTN_AbilityBindingUtils::InheritBindings` is `BlueprintPure`. A kit gets
behaviour from an optional plugin through `Extensions` (a `FGSTN_KitExtension`-derived struct picked in the details panel), never a
new property. Nothing in this layer is subclassed; it is data.

## Loadout assembly (`Loadout/`)

The compiled binding table and the loadout rules are pure and live in [GSTNCombat](../GSTNCombat/README.md) (`GSTNCombatCore`): they
read plain values and opaque soft paths for abilities, so they need no ability-system type. This plugin holds the one piece that
knows both sides.

| Type | What it is |
|---|---|
| `FGSTN_ResolvedLoadout` | The assets a loadout names, already resolved by the caller (the loadout component, outside the simulation): kit, weapons in loadout order (0 = the kit's primary weapon), supers per slot, scheme, skin. A null pointer is an id that did not resolve. |
| `FGSTN_LoadoutAssembly` | `BuildCompileInput` (kit command set and bindings, each weapon's bindings after inheritance, supers, customisations; the timeline predicate is `IsTimelineAbilityClass`, which loads the class and looks for the `Combat.Ability.Timeline` asset tag on its default object), `BuildRulesView` (tags, queries, allowed alternatives, compile findings), `CompileAndValidate` (compile, conflict check, rules in one call). |

Why the compiler is not here: the spec put it in this module because it holds `TSoftClassPtr<UGameplayAbility>`, but the compile only
sorts, dedupes and indexes ability keys, so an opaque `FSoftObjectPath` is enough and the code stays testable without GAS. Everything
that needs the class (the timeline question, binding inheritance along the weapon chain) happens in the assembly.

**Extending in Blueprint.** Loadout validation is extended with `UGSTN_LoadoutRule` subclasses in a `UGSTN_LoadoutRules` asset (see the
GSTNCombat README); the assembly is stateless and not subclassed.

## Loadout component, subsystem and skins (`Loadout/`, `Cosmetics/`)

Nothing here depends on Network Prediction or Mover: a loadout is granted on the pawn's ability system **outside the simulation**,
and the combat sync state only ever sees the result (the revision write and the table hand-over are the NP adapter's job, `GSTNCombatNP`).

| Type | What it is |
|---|---|
| `UGSTN_LoadoutComponent` | `UActorComponent`, meant for the **PlayerState**. `ServerRequestLoadout(FGSTN_LoadoutData)` (Server, Reliable) calls `AcceptLoadout`, which resolves the ids, compiles the table and runs the rules asset (`Rules`, empty = the built-in set); a refusal comes back as `OnLoadoutRejected` (rule findings as strings) and changes nothing. An accepted loadout is stored in the replicated `Applied` (with the replicated `Revision`, which counts accepted loadouts from 1), then `ApplyAppliedLoadout()` runs: **called directly on the server** and by `OnRep_Applied` on a client. It resolves every asset id once (Asset Manager, `ResolveAsset`), keeps the assets, compiles the table on this machine (`GetCompiledTable`), and on the server grants, then broadcasts `OnLoadoutApplied(Component, Revision)`. |
| What the server grants | On the pawn's ability system (`GetTargetAbilitySystem`: the PlayerState's pawn, or the owner's own ASC): the kit's init stats effect (`FGSTN_KitStats::ApplyTo`), the primary weapon's stats effect (`UGSTN_GE_WeaponStats::Apply`), then the table's `GrantedAbilities` plus the primary weapon's `PassivesWhileActive` (`GiveAbility`, no source object, an ability the pawn already has is left alone and never removed). A later apply first removes exactly what the previous one granted (handles kept server-side, the stats effect removed by class). A pawn that is not there yet leaves `IsGrantPending()` true; the component listens to `APlayerState::OnPawnSet` (server) and applies again one tick after a pawn is set, and `ApplyAppliedLoadout()` can be called by hand. |
| `UGSTN_LoadoutSubsystem` | `ULocalPlayerSubsystem`: named presets in the save slot `GSTNCombatLoadouts` of the local user. `SaveLoadout`, `LoadLoadout` (drops ids whose asset no longer exists, with a Warning, on a copy: the file keeps what was saved), `DeleteLoadout`, `HasLoadout`, `ListLoadouts`, `RequestLoadout(Preset, Component)`. Presets load lazily on first use. Storage is `ReadStorage`/`WriteStorage` (virtual: replace for a cloud save), the existence check is `IsAssetIdKnown`. |
| `UGSTN_LoadoutSaveGame` | `USaveGame` with `TMap<FString, FGSTN_LoadoutData> Loadouts` and its own `SaveVersion`; `ToBytes`/`FromBytes`; the static `DropUnknownAssets` (kit/skin cleared, a super slot emptied, an additional weapon removed with the customisations of the later weapons re-indexed and those of the dropped weapon removed). |
| `UGSTN_SkinComponent` | `UActorComponent` for the **pawn**, cosmetic only, never on a dedicated server. Follows the PlayerState's loadout component (auto-bind at BeginPlay with a short retry while the PlayerState replicates; or `BindToLoadout`) and applies `GetResolvedSkin()` on spawn and on every `OnLoadoutApplied`: mesh, material overrides per slot, anim class, all loaded once at apply. The pawn's original look is remembered and restored by `RestoreOriginal`/an empty skin, so skins never stack. |

**Listen server.** The host is both server and a player: `AcceptLoadout` calls `ApplyAppliedLoadout()` itself and the host never relies
on `OnRep_Applied` (it does not run for a value the host set). Grants and effect applications check that the ability system's owner has
authority, so a client that calls `ApplyAppliedLoadout()` (its OnRep does) only resolves, compiles and broadcasts. With the GAS-NPP fork,
abilities and effects reach the owning client through the simulation sync state: one reconcile after a grant, and
`FindAbilitySpecFromClass` on the client is valid only after it (never right after the loadout changed; never for another player's pawn).
Nobody but the server grants, because a client-side grant on an autonomous pawn is erased by the next reconcile.

**Wiring.** (1) Add `UGSTN_LoadoutComponent` to the project's PlayerState class (component in the Blueprint, or `CreateDefaultSubobject` in C++); optionally point `Rules` at a
`UGSTN_LoadoutRules` asset. (2) Add `UGSTN_SkinComponent` to the pawn class (`bAutoBind` on). (3) Register the primary asset types of the
kit, weapon, super and skin (above, and the GSTNCombat README): ids resolve through the Asset Manager. (4) From the loadout UI, get the
`UGSTN_LoadoutSubsystem` from the local player and call `RequestLoadout`, or call `ServerRequestLoadout` on the component. The pawn needs an
ability system component that is reachable with `UAbilitySystemGlobals::GetAbilitySystemComponentFromActor`. To change where the pawn's ASC is found or where assets
come from, derive from the component and override `GetTargetAbilitySystem` / `ResolveAsset` (both `protected virtual`).

**Extending in Blueprint.** Both components are `Blueprintable` and `BlueprintSpawnableComponent`; every function above except the two virtual hooks is
`BlueprintCallable`/`BlueprintPure`, the delegates `BlueprintAssignable`, the subsystem and save game `BlueprintType`. Rules are extended with
`UGSTN_LoadoutRule` subclasses (see the GSTNCombat README).

## Primary asset types a host project must register

Kits, weapons and supers are referenced by `FPrimaryAssetId` from the loadout (ids, resolved by the Asset Manager), so they must be registered as primary asset types. The type names are in `GSTNCombatAssetTypes` (GSTNCombatCore); the skin, super input scheme and timeline types are listed in the GSTNCombat README.

| Primary asset type | Base class (`AssetBaseClass`) |
|---|---|
| `GSTN_PrimaryKit` | `/Script/GSTNCombatGAS.GSTN_PrimaryKitDefinition` |
| `GSTN_Weapon` | `/Script/GSTNCombatGAS.GSTN_WeaponDefinition` |
| `GSTN_Super` | `/Script/GSTNCombatGAS.GSTN_SuperDefinition` |

Add one `+PrimaryAssetTypesToScan=(PrimaryAssetType="...",AssetBaseClass="/Script/<Module>.<Class>",bHasBlueprintClasses=False,bIsEditorOnly=False,Directories=((Path="/Game/<your folder>")),SpecificAssets=,Rules=(Priority=-1,ChunkId=-1,bApplyRecursively=True,CookRule=AlwaysCook))` line per type under `[/Script/Engine.AssetManagerSettings]` in the project's `DefaultGame.ini`. The plugin never edits the project's config. `AlwaysCook` matters because nothing hard-references these assets: they are found by id.

## How to wire it

1. Enable the plugin `GSTNCombatGAS` (the project's `.uproject` lists it; it pulls in `GSTNCombat` and GameplayAbilities).
2. A module that uses the tags lists `GSTNCombatGAS` in its `Build.cs` and includes `GSTN_CombatTags.h`.
3. Designers add leaves under the roots in a tag ini (`Config/Tags/` of a plugin or of the project).

## Testing

Automation tests `GSTNCombat.GAS.*`: `Tags.AllRegisteredAndDocumented`, `Attributes.Clamp`, `Attributes.KitStatsInitEffect`,
`Attributes.MeterCostData`, `Effects.MeterCostCheckApply` (cost deducted and activation blocked through the real ability CheckCost/ApplyCost), `Attributes.WeaponStatsApplyRemoveByClass`, `Data.BindingInheritance`,
`Data.BindingToRemovedCommandIsError`, `Data.UnknownExtensionIgnored`, `Data.KitIdentityExactlyOne`, `Data.WeaponIdentityExactlyOne`, `Table.AssembleFromDefinitions`
(kit and weapon assets through the assembly into the compiler and rules), `Loadout.SaveRoundTrip` (presets through the subsystem and the save game
bytes, a new session over the same bytes), `Loadout.UnknownIdDropped` (unknown kit/weapon/super/skin ids dropped, super slots and weapon indices kept consistent, the file untouched),
`Loadout.RulesRejectInvalid` (a rejected request changes nothing) and `Loadout.ApplyOnHostWithoutOnRep` (the server apply path on a transient ability system:
grants, stats effects, replacement by a second loadout, broadcast count, a grant that waits for a pawn; no OnRep, no PIE, no Asset Manager). The compiler and rule tests are `GSTNCombat.Core.Table.*` and
`GSTNCombat.Core.LoadoutRules.*`, where the code lives.
The data tests build assets in memory and use test-only identity leaves (`GSTN_DataTestTypes`). The loadout tests use test doubles of the component and
subsystem (`GSTN_LoadoutTestTypes`) that answer asset ids from a table, grant on a chosen ASC and keep the save slot in memory; they need a `ULocalPlayer` object only as the subsystem's outer. The attribute tests use a stock
`UAbilitySystemComponent` on an actor in a transient game world (effects need a world for their start time), with no Network
Prediction or Mover, so they also run against stock GAS. Run with the same command
as the other combat tests: `-ExecCmds="Automation RunTests GSTNInput+GSTNCombat; Quit"`.
