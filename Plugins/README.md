# Plugins

Every plugin in this folder has a `README.md` next to its `.uplugin`. Each README covers the same points: purpose and status,
modules, dependencies, the gameplay tags the plugin owns, how to extend it in C++ and Blueprint, how to use it in another
project, and where to read more in [docs/architecture](../docs/architecture/). Convention: a new plugin gets its README in the
same commit, and a change to a plugin's modules, dependencies or tags updates it (and this index).

The project module `GASP_Giustino2` (`Source/`) is not a plugin; it uses all of the GSTN plugins and `GASNPP`. Layout:

| Folder | Contents |
|---|---|
| `GSTN*` | The game's own systems (below). |
| [GASNPP](GASNPP/README.md) | The GAS + Network Prediction fork. |
| [EngineOverrides](EngineOverrides/README.md) | Copies of engine plugins loaded instead of the engine's. |
| [UE_MCP_Bridge](UE_MCP_Bridge/README.md) | Third-party editor-only MCP bridge. |

## Project plugins (GSTN)

Each feature is split into a pure **Core** plugin (no GAS, Network Prediction or Mover) and adapter plugins that add GAS or
Network Prediction, so the Core can be reused in a project without the GAS-NPP fork.

| Plugin | Layer | Module | What it is | Needs the GAS-NPP fork |
|---|---|---|---|---|
| [GSTNInput](GSTNInput/README.md) | Core | `GSTNInputCore` | Input commands, buffer, priority, locks, contexts and native input tags. | No |
| [GSTNInputNP](GSTNInputNP/README.md) | NP adapter | `GSTNInputNP` | The input simulation embeddable in an NP model, ability bindings, the Mover sim-state tracker. | Yes |
| [GSTNCombat](GSTNCombat/README.md) | Core | `GSTNCombatCore` | Default combat command set, combat tags and settings, timelines, loadout data and rules, sync fragment format. | No |
| [GSTNCombatGAS](GSTNCombatGAS/README.md) | GAS | `GSTNCombatGAS` | Combat ability tags, attribute sets, kit stats, meter and weapon effects, kit/weapon/super assets, the loadout component, loadout presets (save game) and the skin component. | No (stock GAS works) |
| [GSTNCombatNP](GSTNCombatNP/README.md) | NP adapter | `GSTNCombatNP` | The combat NP model (`UGSTN_CombatSimComponent`), sync fragments and tick extensions. | Yes |
| [GSTNAnimation](GSTNAnimation/README.md) | Core | `GSTNAnimationCore`, `GSTNAnimationRig` | Feature animation decoupled from the main graph: override layers, anim sets per feature tag (`Anim.Set.*`), skin variants by cosmetic tags (`Anim.Trait.*`), limb contact IK (Control Rig node: hands and feet onto a nearby surface); presentation only, hitboxes unchanged. | No |
| [GSTNLevelObjects](GSTNLevelObjects/README.md) | Level objects | `GSTNLevelObjects` | Placeable objects that offer traversal surfaces: the adjustable trampoline (`AGSTN_Trampoline`), a GSTNSurface provider. Depends on GSTNSurface and GSTNCombat Core only. | No |
| [GSTNParts](GSTNParts/README.md) | Core (combat extension) | `GSTNPartsCore`, `GSTNPartsGAS` | Destructible body parts: native part tags, settings, tag ini, plus the part set data, penetration table, validators and the pure integer part math (P1-P4), and the GAS half: one-shot break effects, part events, tag projection (P5). Optional, `EnabledByDefault` false. | No |
| [GSTNDefence](GSTNDefence/README.md) | Core, GAS, NP adapter (combat extension) | `GSTNDefenceCore`, `GSTNDefenceGAS`, `GSTNDefenceNP` | Shield (fixed bubble, Smash style, PvP only), parry from the shield and from move windows, tech, DI/SDI rules: data, tags and pure math, the shield attribute mirror and effects, and the NP half (sync fragment, bubble hurtbox, clocks, steps, ability classes). Optional, `EnabledByDefault` false. | Core and GAS no; NP yes |
| [GSTNLink](GSTNLink/README.md) | Core (combat extension) | `GSTNLinkCore` | Links between two entities (grabs, carries, mounts): definitions, rules, state types, exit placers, mash and break evaluators, a fake host for tests. Never depends on GSTNDefence. Optional, `EnabledByDefault` false. | No (the NP half is not built yet) |

## GAS-NPP fork

[GASNPP](GASNPP/README.md): Epic's NetworkPrediction (+ Extras, Insights), the GameplayAbilities fork and
AbilitySystemSimulation, ported to UE 5.8. Every project change is listed in [GASNPP/PATCHES.md](GASNPP/PATCHES.md).

| Plugin | README |
|---|---|
| `NetworkPrediction` | [GASNPP/NetworkPrediction](GASNPP/NetworkPrediction/README.md) |
| `NetworkPredictionExtras` | [GASNPP/NetworkPredictionExtras](GASNPP/NetworkPredictionExtras/README.md) |
| `NetworkPredictionInsights` | [GASNPP/NetworkPredictionInsights](GASNPP/NetworkPredictionInsights/README.md) |
| `GameplayAbilities` | [GASNPP/GameplayAbilities](GASNPP/GameplayAbilities/README.md) |
| `AbilitySystemSimulation` | [GASNPP/AbilitySystemSimulation](GASNPP/AbilitySystemSimulation/README.md) |

## Engine overrides

[EngineOverrides](EngineOverrides/README.md): copies of engine plugins loaded instead of the engine's. Mover carries project
edits marked "GAS-NPP fork"; the others are unmodified as far as git shows.

| Folder | Plugin | README |
|---|---|---|
| `Mover` | Mover | [Mover](EngineOverrides/Mover/README.md) |
| `ChaosMover` | ChaosMover | [ChaosMover](EngineOverrides/ChaosMover/README.md) |
| `MoverExamples` | MoverExamples | [MoverExamples](EngineOverrides/MoverExamples/README.md) |
| `SmartObjects` | SmartObjects | [SmartObjects](EngineOverrides/SmartObjects/README.md) |
| `GameplayInteractions` | GameplayInteractions | [GameplayInteractions](EngineOverrides/GameplayInteractions/README.md) |
| `GameplayTargetingSystem` | TargetingSystem | [GameplayTargetingSystem](EngineOverrides/GameplayTargetingSystem/README.md) |
| `GASToolsets` | GASToolsets | [GASToolsets](EngineOverrides/GASToolsets/README.md) |
| `AllToolsets` | AllToolsets | [AllToolsets](EngineOverrides/AllToolsets/README.md) |

## Editor tooling

[UE_MCP_Bridge](UE_MCP_Bridge/README.md): third-party editor-only WebSocket bridge for the UE-MCP server. Not game code.

## Dependency overview

Arrows read "depends on". Source of truth: the `Plugins` list of each `.uplugin` and the `Build.cs` of each module; check
them before relying on this picture.

```
GSTNInput (GSTNInputCore) ---------------------------> EnhancedInput
   ^            ^
   |            +------ GSTNCombat (GSTNCombatCore)
   |                         ^
   |                         +------ GSTNCombatGAS ----------------------> GameplayAbilities (fork)
   |                         |             ^
GSTNInputNP -----------------+--- GSTNCombatNP
   |                                       |
   +---------------+-----------------------+
                   v
   AbilitySystemSimulation -> GameplayAbilities (fork), NetworkPrediction (fork), NetworkPredictionExtras, Mover, MotionWarping
   Mover (override) -> NetworkPrediction (fork), MotionWarping, PoseSearch, Water, ChaosVD
```

| Plugin | Depends on (plugins in `.uplugin`) | Module dependencies on other project modules |
|---|---|---|
| `GSTNInput` | EnhancedInput | none |
| `GSTNCombat` | GSTNInput | `GSTNInputCore` |
| `GSTNCombatGAS` | GSTNCombat, GSTNInput, GameplayAbilities | `GSTNCombatCore`, `GSTNInputCore` |
| `GSTNInputNP` | GSTNInput, GameplayAbilities, AbilitySystemSimulation, NetworkPrediction, EnhancedInput, Mover | `GSTNInputCore` |
| `GSTNCombatNP` | GSTNCombat, GSTNCombatGAS, GSTNInputNP, GSTNInput, GameplayAbilities, AbilitySystemSimulation, NetworkPrediction, EnhancedInput, Mover | `GSTNCombatCore`, `GSTNCombatGAS`, `GSTNInputCore`, `GSTNInputNP` |
| `GSTNParts` | GSTNCombat | `GSTNCombatCore`, `GSTNCombatGAS`, `GSTNInputCore`, `GSTNCombatNP` (NP module) |
| `AbilitySystemSimulation` | GameplayAbilities, NetworkPrediction, NetworkPredictionExtras, EnhancedInput, Mover, MotionWarping | none |
| `GameplayAbilities` | EngineAssetDefinitions, GameplayTagsEditor, Niagara, DataRegistry | none (does not depend on NetworkPrediction) |
| `NetworkPrediction` | none | none |
| `NetworkPredictionExtras` | NetworkPrediction | none |
| `NetworkPredictionInsights` | none | none |
| `Mover` | NetworkPrediction, MotionWarping, PoseSearch, Water, ChaosVD | none |
| `ChaosMover` | Mover | none |
| `MoverExamples` | Mover, ChaosMover, EnhancedInput, CableComponent | none |
| `SmartObjects` | GameplayAbilities, TargetingSystem, WorldConditions, PropertyAccessEditor, PropertyBindingUtils | none |
| `GameplayInteractions` | StateTree, SmartObjects, ContextualAnimation, NavCorridor, GameplayStateTree | none |
| `TargetingSystem` | GameplayAbilities | none |
| `GASToolsets` | GameplayAbilities, ToolsetRegistry | none |
| `AllToolsets` | 21 toolset plugins, among them GASToolsets | none |
| `UE_MCP_Bridge` | PythonScriptPlugin, GameplayAbilities, ... (see its README) | none |

Rules that hold today: no Core module depends on GAS, Network Prediction or Mover; nothing in `GASNPP` or `EngineOverrides`
depends on a GSTN plugin; no project module depends on `UE_MCP_Bridge`. The GSTN plugins that need the fork are the two
NP adapters (they link `AbilitySystemSimulation` and `NetworkPrediction`). `GameplayAbilities`, `NetworkPrediction` and `Mover`
resolve to the copies in this folder, not to the engine's.

## Gameplay tag roots owned by the GSTN plugins

Native tags are declared with `UE_DEFINE_GAMEPLAY_TAG*` in the module named below. The project's own designer leaves live in
`Config/Tags/GSTNCommands.ini` (project config, not a plugin) and in the tag lists of `Config/DefaultGameplayTags.ini`. Plugin
`Config/Tags` folders are not read by the engine for ordinary plugins; a plugin that ships one has to register it itself.
`GSTNInputNP` and `GSTNCombatNP` declare no tags (apart from test-only tags in `GSTNInputNP`'s automation tests).

| Plugin / module | Roots and subtrees it declares | Where |
|---|---|---|
| `GSTNInputCore` | `Input.Button` (root), `Command` (root), `Input.Lock.Command` (root), `Input.Context.{Grounded,Airborne,OnWall}`, `Input.BufferEvent.{AbilityActivated,Landed}` (+ the `Input.BufferEvent` root), `Input.Event.CommandExecuted`, and the `*.Test.*` tags used by its tests (`Input.Button.Test.*`, `Command.Test.*`, `Input.Lock.Command.Test.*`) | `Private/GSTN_InputTags.cpp`; test tags in `Private/Tests/` |
| `GSTNCombatCore` | `Combat.State.{Busy,InTimeline,SuperInput,CancelWindow,OverrideWindow,Falling}`; standard commands `Command.{Attack,Strong,Special,Combo}.*` and `Command.Super.Slot1..8`; their locks `Input.Lock.Command.{Attack,Strong,Special,Combo,Super}`; standard buttons `Input.Button.{Attack,Strong,Special,Crouch,Shield,Jump}` (under the root the input plugin owns); identity roots `Weapon`, `Kit`, `Super`, `Loadout`, `Skin` | `Private/GSTN_Combat*Tags.cpp` |
| `GSTNCombatGAS` | `Combat.Ability.*`, `Combat.Source.{Command,Forced,Adopted}`, `Combat.Lock.Movement`, `Combat.Weapon.Role.*`, `Combat.Event.{TimelineStarted,TimelineFinished,WeaponSwitched,SuperUsed}`, `Combat.SetByCaller.*`, and the designer roots `Combat.Stat`, `Combat.Meter` | `Private/GSTN_CombatTags.cpp` |
| `GSTNPartsCore` | `Combat.Part.{Broken,GrabLocked,AllBroken,Effect}`, `Combat.Entity.Destructible`, `Combat.Event.Part.{Damaged,WeakPointHit,Ricochet,Broken,AllBroken}`, `Message.Combat.PartBroken`; ini: `Part.Type.*`, `GameplayCue.Combat.Part.*` | `Private/GSTN_PartsTags.cpp` |

`GSTNCombatGAS` also declares the test tags of its own automation tests. The engine plugins in `EngineOverrides` define their own
tags (Mover: `Mover.*` state flags in `MoverTypes.cpp`); those are not project tags.

## On the parallel branch `claude/project-thread-v1plhd` (not merged yet)

Six more GSTN plugins exist on that branch only. They are not in this tree yet, so their folders are absent here; their READMEs
can be read with `git show origin/claude/project-thread-v1plhd:Plugins/<name>/README.md`. All six are experimental and
`EnabledByDefault: false`, and the branch enables them in `GASP_Giustino2.uproject`. When it merges, move them into the tables
above and extend the dependency overview and the tag table.

| Plugin | Version | Module (type) | What it is | Plugin dependencies | Tags it owns |
|---|---|---|---|---|---|
| `GSTNSurface` | 0.1 | `GSTNSurfaceCore` (Runtime) | Traversal surface data contract: which mechanics a collider offers (wall slide, climb, launch, stand), impact class, launch parameters, one stateless resolver. | none | `Surface.Type.*` (ini, `Config/Tags/GSTNSurface.ini`), `Surface.Behaviour.{WallSlide,WallClimb,Launch,Stand,Slippery,OneWay}`, `Surface.Event.Launched` (native) |
| `GSTNInteract` | 0.1 | `GSTNInteractCore` (Runtime); `GSTNInteractRuntime` (Runtime) added by checkpoint I1; the simulation bridge `GSTNInteractCombat` is a separate plugin, not built | One focus, one prompt: what the Interact button acts on. Pointing, reach and arc, scoring, sticky focus, stamped candidate registry, meaning table, game-thread interactor with a minimal presentation. | none | `Interact`, `Interact.Target`, `Message.Interact.FocusChanged` (native); the `Interact.Target.*` leaves belong to the consumers |
| `GSTNParts` | 0.1 | `GSTNPartsCore`, `GSTNPartsGAS`, `GSTNPartsNP` (Runtime);  presentation and editor modules are planned, not built | Destructible body parts: part sets, penetration table, break effects as data, integer-quantised hit math. | GSTNCombat | `Part.Type.*` and `GameplayCue.Combat.Part.*` (ini), `Combat.Part.*`, `Combat.Entity.Destructible`, `Combat.Event.Part.*`, `Message.Combat.PartBroken` (native) |
| `GSTNCombatEditor` | 0.7 | `GSTNCombatEditor` (Editor) | Timeline editor (edit context, model, panel, track editors), validation, socket baking, hurtbox set editor. | GSTNCombat, GSTNInput, GSTNCombatGAS, GSTNCombatNP, DataValidation | none of its own |
| `GSTNCombatDebug` | 0.2 | `GSTNCombatDebug` (Runtime, denied in Shipping) | PIE and development tooling: the input display widget; Gameplay Debugger category, debug draw and state trace are planned. | GSTNCombat, GSTNInput, GSTNInputNP, GSTNCombatNP | none |
| [`GSTNPlatform`](GSTNPlatform/README.md) | 0.1 | `GSTNPlatformCore` (Runtime), `GSTNPlatformRuntime` (Runtime) | Enemies as platforms and traversal surfaces for fighters (traversal plan T3): kinematic unattached colliders posed once per simulation frame (Mover's pre-simulation tick), shapes with GSTNSurface behaviours (surface providers), per-shape states (mask, action state, drop grace) as frame timelines, collision derived from behaviour, pose history velocity, scripted test fighter. Channel `GSTN_EnemyPlatform` in the plugin's `Config/Engine.ini`. No minion proxy, no part bridge. | GSTNSurface, Mover | `Platform`, `Platform.State.*`, `Platform.Event.*` (native) |

The same branch adds the `Combat.Hurtbox.{Head,Torso,Arm,Leg,Hand,Foot}` tags to `GSTNCombatGAS` and a few const accessors to
`UGSTN_CombatSimComponent`. Nothing in the runtime plugins depends on `GSTNCombatEditor` or `GSTNCombatDebug`.
