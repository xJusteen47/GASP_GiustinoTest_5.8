# Independence audit (MT section 3.7) - GSTNInput + GSTNCombat, 2026-10-22 (read-only audit)

Clean: no `/Game/`, no `GASP_Giustino2` include/symbol, no `GConfig`/`GGameIni` reads, no `#if WITH_<OTHER_PLUGIN>` anywhere in
Plugins/GSTN*/Source (only `WITH_DEV_AUTOMATION_TESTS` and `ENABLE_DRAW_DEBUG`, both fine). Test prefixes are plugin-scoped:
`GSTNInput.Core`, `GSTNInput.NP`, `GSTNCombat.Core`. Intermediate/ ignored.

## Findings (most important first)
1. Plugins/GSTNCombat/Source/GSTNCombatNP/GSTNCombatNP.Build.cs:18 - rule 2: depends on `GSTNInputNP` (non-Core module of another
   plugin). Includes: Public/Simulation/GSTN_CombatSimComponent.h:12 (`Simulation/GSTN_InputSimulator.h`),
   Public/Simulation/GSTN_CombatSimTypes.h:6 (`Simulation/GSTN_InputSimTypes.h`), Private/Simulation/GSTN_CombatSimComponent.cpp:8
   (`Context/GSTN_InputContextProvider.h`) and :20 (`Sources/GSTN_InputSource.h`).
   Fix: move the interfaces/plain types the combat model embeds (context provider, input source interface, sim types) into
   `GSTNInputCore`, keep implementations in InputNP, wire via a Core registry/factory; or, if embedding the NP simulator is the
   design, record it as a documented exception in the layers file (6.1, check_modules rule 11). Owner decision.
   Resolved by owner decision 2026-10-22: documented exception (NP may depend on NP of same layer).
2. Config/DefaultGame.ini:33-38 - rule 1: combat's tiers/exclusive buttons (`CommandTiers`, `ExclusiveButtons`, section
   `[/Script/GSTNInputCore.GSTN_InputSettings]`) live in the PROJECT ini, so a stranger project gets empty lists. Fix: a
   `UGSTN_CombatSettings` (UDeveloperSettings, GSTNCombatCore) with FGameplayTag fields and defaults in
   Plugins/GSTNCombat/Config/DefaultGSTNCombat.ini, pushed into the input plugin through a Core registry at startup; remove the block.
3. Plugins/GSTNInput/Source/GSTNInputCore/Public/Input/GSTN_InputSettings.h:21 - `config = Game, defaultconfig` stores in the
   project's Config/DefaultGame.ini; neither Plugins/GSTNInput/Config/ nor Plugins/GSTNCombat/Config/ exists. Fix: add
   Plugins/GSTNInput/Config/DefaultGSTNInput.ini with the defaults and use `config = GSTNInput` (or ship defaults in the plugin file).
4. Plugins/GSTNCombat/Source/GSTNCombatCore/Private/Input/GSTN_CombatButtonTags.cpp:7-12 - tag ownership (MT 4.1): combat declares
   `Input.Button.*`, under the root owned by GSTNInput (GSTN_InputTags.cpp:7). Not a project leak, but one plugin declaring under
   another's root. Fix: rename to a `Combat.Button.*` root or move the leaf tags to a tag ini shipped in the combat plugin. Owner decision.
5. Config/Tags/GSTNCommands.ini does NOT exist (neither Config/ nor Config/Tags/), though referenced by Plugins/GSTNInput/README.md:25,
   Public/Input/GSTN_InputTags.h:9, Public/Input/GSTN_CombatCommandTags.h (combat), and docs input.md/combat.md/combat-m2-sim-input.md.
   Fix: create it inside the plugin (Plugins/GSTNCombat/Config/Tags/) or correct the references.
6. Plugins/GSTNInput/GSTNInput.uplugin:~28-50 and Plugins/GSTNCombat/GSTNCombat.uplugin:29-55 - rule 5: the plugin lists
   GameplayAbilities, AbilitySystemSimulation, NetworkPrediction, Mover, EnhancedInput unconditionally, so `*Core` cannot be used
   without the fork; the descriptions (line 6) do not state the fork requirement for the NP modules. Fix: mark non-Core needs
   `Optional`/split into a sub-plugin and say so in Description and README.
7. Plugins/GSTNInput/README.md:157 and Plugins/GSTNCombat/README.md:78 - test command hard-codes `GASP_Giustino2.uproject`.
   Fix: "your project's .uproject".
8. Config/DefaultEngine.ini:213-215 - ClassRedirects for `GSTN_MoverSimStateTracker` sit in the project ini; they describe plugin
   history. Fix: move to Plugins/GSTNInput/Config/DefaultGSTNInput.ini ([CoreRedirects]).
9. Rule 6 gaps: no changelog (VersionName "0.1" only), no plugin Content/ minimal map, no Plugins/*/Config/; rule 7 canary
   (Tools/Canary/make_canary.py) absent.
10. Edge to list in layers file 6.1: GSTNCombatCore -> GSTNInputCore (Build.cs:18; used by tests GSTN_CombatTests_Defaults.cpp:50,57,58).
    Allowed by rule 2.
11. Config/DefaultGame.ini:30-31 `AbilitySystemMappingContexts` /Game/... entries are project-owned, not plugin-owned: no action.

## Rules 3-5 (3.8, 3.9, designer tags) - audit of M2 code, 2026-10-22 (read-only; Plugins/GSTNInput + Plugins/GSTNCombat, Intermediate/ ignored)
Rule 3 = MT 3.8 Blueprint extension, rule 4 = MT 3.9 replaceable content, rule 5 = MT 4.2b designer tags. Paths below are relative to Plugins/.
**Clean:** no `ConstructorHelpers`, `LoadObject`/`StaticLoadObject` or literal `/Game/` path; no leaf of `Kit.`/`Weapon.`/`Ability.Combat.Special.`/`Super.` named in code (M2 has none of these roots yet).
**Literal paths (benign, no fix):** GSTNInput/.../Input/GSTN_InputConditions.h:110, :254, :358 (`BitmaskEnum = "/Script/GSTNInputCore.EGSTN_StickDirection"`) and GSTNCombat/.../Simulation/GSTN_CombatSimComponent.h:111 (`MustImplement = "/Script/GSTNInputNP.GSTN_InputSource"`) are class paths in meta strings, not content.
**Hard refs (rule 4):** GSTN_InputConfig.h:28, :64 `TObjectPtr<UInputAction>`; GSTN_CommandBindingSet.h:36, :46 `TSubclassOf<UGameplayAbility> Ability`, :126 `TObjectPtr<UGSTN_InputSlotSet>`; GSTN_CombatSimComponent.h:100, :104 `InputConfig`/`SlotBindings`. Fix: `TSoftObjectPtr<UInputAction>`, `TSoftClassPtr<UGameplayAbility>` (resolved when the binding table compiles, outside the simulation, before the first NP tick), `TSoftObjectPtr` for sets/config, or Primary Asset IDs. No hard cross-plugin content refs found.
**Tags with leaves in code that should be ini (rule 5):** none for designer identities. Native leaves are system vocabulary (GSTNCombat GSTN_CombatCommandTags.cpp:7-27, GSTN_CombatButtonTags.cpp:7-12; GSTNInput GSTN_InputTags.cpp:7-16): keep native. Test-only leaves (GSTN_TestTags.cpp:7-28, GSTN_InputNPTests_Simulator.cpp:23-25) fine. M3 action: `Kit.`/`Weapon.`/`Super.` roots only, leaves in ini.
**Extension points that should be BlueprintNativeEvent/Blueprintable (rule 3):** GSTNInputNP GSTN_InputContextProvider.h:22 `UCLASS(Abstract, EditInlineNew, DefaultToInstanced)` lacks `Blueprintable`; virtuals :29-45 (`GetFacingYaw`, `AppendContextTags`, `GetContextFingerprint`, `AppendFrameEvents`) are plain C++. Fix: add `Blueprintable`, make `AppendContextTags`/`GetFacingYaw` `BlueprintNativeEvent` (const view in, result out), mark BP override opt-in because it runs every NP tick. GSTN_InputSource.h:30 `UINTERFACE(MinimalAPI)`, `ProduceInput` :47: per-frame, keep C++ and document the exception. Instanced-struct families cannot be Blueprint-subclassed: FGSTN_InputCondition virtuals (GSTN_InputConditions.h:79-100), FGSTN_PriorityCriterion::Score (GSTN_PriorityCriteria.h:45), FGSTN_BufferResetRule::ShouldFlush (GSTN_BufferResetRules.h:46). Fix: keep as the deterministic data path and, if designers need custom rules, add one wrapper variant holding a Blueprintable UObject class with a BlueprintNativeEvent (owner decision). FGSTN_InputSimulator virtuals (GSTN_InputSimulator.h:28-50) are the NP host interface: C++ only.
**UCLASS/USTRUCT without Blueprint exposure:** GSTN_CombatSimComponent.h:45 (add `Blueprintable` so projects subclass it); GSTN_MoverSimStateTracker.h:25 (add `BlueprintType` for read access); GSTN_StandardSlots.h:27 (GSTNCombat), GSTN_InputContextProvider.h:57, GSTN_CommandBindings.h:72, GSTN_InputSlotValidation.h:63 plain `UCLASS()` (add `BlueprintType` / `UBlueprintFunctionLibrary` + `BlueprintPure` when exposed). All public-header `USTRUCT`s already `BlueprintType`; GSTN_TestCriteria.h:12 is private test code.
**Tag fields without `Categories`:** GSTN_BufferResetRules.h:20 `EventTags`, :23 `TagsAdded` (add `Input.BufferEvent` for EventTags; TagsAdded are state tags, note why none), :76 `Tag` (state tag); GSTN_CommandBindings.h:24 `RequireTags`, :28 `IgnoreTags` (state tags, arbitrary roots: document); GSTN_CommandBindingSet.h:90 `WatchedTags`; GSTN_InputContextProvider.h:85 `MoverTags` (Mover mode tags, no GSTN root). Correct already: InputConditions.h:139-219, InputSlot.h:63, CommandBindingSet.h:27, BufferResetRules.h:42/:61, InputConfig.h:20, InputSettings.h:91/:99.
**BlueprintPure utils:** neither plugin has a `UBlueprintFunctionLibrary`; stateless helpers (button map, script build, priority scoring, slot validation) are C++ only. Add when designer-facing functionality appears (M3).


## Decision (coordinator, 2026-10-22): Blueprint wrappers and soft refs
- Opt-in wrapper per instanced-struct family (conditions, priority criteria, reset rules), e.g. `FGSTN_InputCondition_Blueprint` holding a Blueprintable UObject class with a const BlueprintNativeEvent. Built-in C++ structs remain the default. README documents it as the slower path that must stay deterministic (no world queries, no state outside its inputs). Needs an Automation test.
- Soft references: loaded-at-use is fine for assets; anything the simulation reads every frame is resolved once at setup or on loadout change, never in the tick.

## Known deviation: GSTNCombatNP depends on GSTNCombatGAS (2026-10-24)

`GSTNCombatNP.Build.cs` lists `GSTNCombatGAS` (the timeline ability tags, the combat attribute set the stats modifier reads, `UGSTN_GE_WeaponStats`/`FGSTN_StatValue` in the weapon stats entries, the loadout component). The loadout table swap (C17) does not widen it: the converter, the table set and the swap logic use Core and InputNP types only (plus the weapon stats entry type, already there); GAS is touched in the component's `BindLoadoutComponent` / `BuildWeaponStats` glue.

## New edge: GSTNCombatCore -> GSTNSurfaceCore (H11, 2026-10-25)

`GSTNCombatCore.Build.cs` (private use in `Hits/GSTN_ImpactMath.cpp`, the tests) and `GSTNCombat.uplugin` depend on `GSTNSurface`. Core -> Core, planned in the GSTNSurface README (traversal-surfaces.md §3, TS.1): the impact table keys its rows by the GSTNSurface type index, and `Surface.Type` (root and leaves Wall, Glass, Crate, Trampoline) lives in GSTNSurface, not in combat. GSTNSurfaceCore has no GAS, Network Prediction or Mover, so the Core purity of combat is unchanged. The duplicate `Surface.Type` root and leaves H3 had put in `GSTNCombatCore` were removed.
