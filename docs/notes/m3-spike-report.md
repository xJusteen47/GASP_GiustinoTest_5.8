# M3 spike report (C0a, C0c) - STATIC ANALYSIS ONLY

Scratch notes, not versioned. Source read only; nothing built or run. Date 2026-10-22.
Paths are relative to the project root. `NpASC` = `Plugins/GASNPP/AbilitySystemSimulation/Source/AbilitySystemSimulation/Private/Abilities/NpAbilitySystemComponent.cpp`.
`AbDT` = `.../Private/DataTypes/AbilitiesDataTypes.cpp`, `SimDT` = `.../Private/DataTypes/AbilitySimulationDataTypes.cpp`, `EffDT` = `.../Private/DataTypes/EffectsDataTypes.cpp`.

## Summary

| # | Spike | Verdict |
|---|---|---|
| 1 | Runtime grants | PASS by static reading (path exists, designed for it); NEEDS-LIVE-TEST for "one reconcile, no storm", the client lookup latency and the byte numbers |
| 2 | Stats modifier | PASS: settings object is per Mover component. Gravity override is NOT simulation state (usable only if rewritten every tick from simulated data). NEEDS-LIVE-TEST for the forced-reconcile equality run |
| 3 | GAS triggers | PARTIAL. `GameplayEvent` triggers: supported (NEEDS-LIVE-TEST). `OwnedTagAdded/Removed` triggers: NOT rollback-safe as the fork stands (fires inside `RestoreFrame`). Decision needed (one-line fork patch vs trigger table) |
| 4 | C0c fragment wire size | PASS: 0 fragments = 1 bit, 1 fragment = 25 + 8L bits, 2 fragments = 41 + 8(L1+L2) bits; already asserted by existing automation tests |

---

## 1. Runtime grants (about 40 specs + infinite GEs, outside the simulation)

### Verdict: PASS (static) / NEEDS-LIVE-TEST (storm, latency, bytes)

### Evidence

How a server grant outside the sim reaches the owner:

- The fork does NOT replicate `ActivatableAbilities` / `ActiveGameplayEffects` through the stock GAS replication. `GetLifetimeReplicatedProps` disables every replicated property of `UGameplayTasksComponent` and `UAbilitySystemComponent` and keeps only `OwnerActor`, `AvatarActor`, the NP proxies (NpASC:1034-1065). `ReplicateSubobjects` skips the parent (NpASC:1067-1072). So the only channel is the NP sync state.
- `GiveAbility` on the authority appends to `ActivatableAbilities.Items`, creates the primary instance for `InstancedPerActor`, and bumps `SyncedAbilitiesHandlesCount` (NpASC:1660-1685). Nothing in it requires being inside the sim tick. If the ability list is scope-locked, the spec goes to `AbilityPendingAdds` and lands on unlock (NpASC:1665-1670).
- `FillSyncState` runs at the end of every sim tick and snapshots the live ASC: `Abilities.FillFromActivatableAbilities(ActivatableAbilities)`, `ActiveGameplayEffects`, `AttributeSets`, handle count (NpASC:1001-1014). The ASC state is live (not rebuilt from the previous sync state at tick start: `InternalSimulationTick` only reads `StartData.SyncState` for montage and projectiles, NpASC:968-999). So a grant made between two ticks is simply in the next frame's server sync state.
- The abilities part of the sync state is sorted by handle (`AbilitySimulationLibrary.cpp:116-124`), so array order differences do not cause false reconciles.
- Client side: the owning (autonomous) client compares its predicted frame with the server frame. `FAbilitySimSyncState::ShouldReconcile` (SimDT:481-503) returns true on `ActivatableAbilitiesHandleCount` mismatch, on different spec count (`FActivatableAbilitiesCollection::ShouldReconcile`, AbDT:856-867) and on different active GEs (`FActiveEffectSyncDataContainer::ShouldReconcile`, EffDT:675-694). Reconcile calls `RestoreFrame` (NpASC:724-752) -> `RestoreAbilities` (NpASC:2818-2879) which adds any missing handle with `ForceGiveAbility` (NpASC:2475-2523) and `RestoreGameplayEffects` (NpASC:2980). `ForceGiveAbility` appends to `ActivatableAbilities.Items`, creates the instance(s) the server had, registers triggers (`OnForceGiveAbility`, NpASC:2524-2578) and calls `OnGiveAbility` on the primary instance (NpASC:2569-2577), so `GSTN` abilities caching state in `OnGiveAbility` get it on the client too (it must be idempotent and must not mutate sim state: it runs while `bIsRestoringFrame`).
- `FindAbilitySpecFromClass` is the stock function (`AbilitySystemComponent_Abilities.cpp:988`, not overridden in the fork) and walks `ActivatableAbilities.Items`, so it is valid on the owning client AFTER that reconcile has been processed, not before. Between server grant and the client reconcile (about half RTT + the input/ack buffering) the lookup returns null. The loadout code must treat "ability present" as an asynchronous condition (poll or wait on the handle/sync state), never assume it right after the loadout RPC.
- Simulated proxies (other players on a remote client): the sim-proxy serialization drops abilities and active GEs on purpose (`SimDT:336-354`, loading branch empties `Abilities` and `ActiveGameplayEffects`). So `FindAbilitySpecFromClass` is NEVER valid for another player's pawn on a client. Anything cosmetic about a remote fighter's kit must come from the loadout component/PlayerState replication, not from the remote ASC. Tags, attributes and cues are synced to proxies.
- Listen host: authority, no reconcile for itself, only the remote client goes through the path above.

The existing error log:

- Message `GiveAbility called on ability X on simulated proxy, not allowed!` is NpASC:1653-1658. Condition is `!IsOwnerActorAuthoritative()`, which is `CachedNetRole` of the NP proxy being neither `ROLE_Authority` nor `ROLE_AutonomousProxy` (NpASC:1025-1032). `CachedNetRole` defaults to `ROLE_None` (`NetworkPredictionProxy.h:296`) and is set by `CheckOwnerRoleChange` (InitializeComponent, PreNetReceive, PreReplication; NpASC:117, 1077, 1098).
- Source in the project log (`Saved/Logs/GASP_Giustino2.log:3406-3424`, same names in the backup log): `GSTN_GA_TestDash/TestBuff/Jump/WallSlide/TestAttack/TestStance`, in bursts of 6, repeated per client pawn. Those are the `DefaultAbilities` of the ASC: `InitializeSimulationState` calls `K2_GiveAbility` for each (NpASC:797-806) on EVERY machine, and on a client whose pawn is still `SimulatedProxy` (the autonomous role arrives later, or the pawn is a remote one) the call is refused. It is harmless: the owner receives the specs through `RestoreAbilities`; remote pawns never need them. It does mean the log is not proof of a bug, and that a loadout component calling `GiveAbility` on clients (OnRep, BeginPlay) would produce the same refusal. Grant on the server only.
- Related: an AUTONOMOUS client passes `IsOwnerActorAuthoritative`, so a client-side `GiveAbility` is NOT refused for the local pawn. It would be a predicted local grant that the server never made; it is erased by the next reconcile (handle count differs). Do not do it.

Active GEs outside the sim: `ApplyGameplayEffectSpecToSelf` has no in-tick requirement (NpASC:1307-1463); only `NetMode == ROLE_SimulatedProxy && !CanPredictAsSimulatedProxy()` and `GetIsRestoringFrame()` refuse. Infinite GEs go into `ActiveGameplayEffects` and from there into the next sync state exactly like abilities (EffDT:564-576). Attribute changes caused by them are in `AttributeSets`.

### Numbers (estimates from the NetSerialize code; to be replaced by the live measurement)

Per ability spec, full serialization (`FActivatableAbilitySyncState::NetSerialize`, AbDT:472-551, plus the collection writing the handle as int32, AbDT:916):

- handle int32 32 bits + class object ref (packed NetGUID, about 16-24 bits steady, but the first export of the class path on a connection costs a path string, order of 40-80 bytes per class, once) + handle packed 8 bits (16 above handle 127) + Level int32 32 + RemoveAfterActivation 1 + source-object flag 1 (+ ref if set) + dynamic tags 6 bits (empty container) + granting-GE flag 1 + set-by-caller flag 1 + ActiveCount uint8 8 + instance data.
- Instance data for `InstancedPerActor` (always exactly one entry, `NetSerializeDataArray` AbDT:367-381): `FActiveAbilityInstanceData::NetSerialize` (AbDT:21-43) = bIsActive 1 + HasAbilityData 1 + HasCues 1 + ActivatedByInput int8 8 + cancelable 1 + blocking 1 + task data 2 bits per `PredictionTasksInstances` of the CDO (BaseTaskData.cpp:197-217) + HasEventData 1 = 14 + 2T bits. `InstancedPerExecution` idle: 8 bits (count) only.
- `LocalOnly` / `ServerOnly` net execution policy: instance and ActiveCount are skipped (AbDT:539-544), the spec is about 100 bits.
- Total full spec: about 124 + 2T bits, roughly 16 bytes. 40 specs: about 620 bytes full, plus the one-time class exports (up to 40 x 40-80 bytes = 1.6-3.2 KB across the first packets after the grant, spread by NP/network bunch limits).
- Steady state (delta path, `np.ToggleDeltaSerialize` default 1, acked base state): per spec int16 DeltaIndex 16 + SameBase 1 + SameActiveCount 1 + the instance data written in full again (AbDT:622, "ToDo: Serialize Instances As Delta") = about 32 + 2T bits, roughly 4-5 bytes. 40 specs: about 160-200 bytes on top of the rest of the ASC sync state, EVERY server send to the owning client, even when nothing changes. At 30 sends/s that is about 5-6 KB/s per owner just for idle ability bookkeeping. `ServerOnly`/`LocalOnly` specs cost 17 bits in steady state, so marking passive/cosmetic abilities that way is the cheap lever if it matters.
- Active GEs: the whole container is 1 bit when unchanged (`SameAsBase`, EffDT:597-601); when something changes each element is delta coded; a new GE is written in full (EffDT:89-152: def ref, level, durations, three tag containers, modifier values 32 bits each, set-by-caller, effect context incl. instigator refs): order of 30-60 bytes per GE with a couple of modifiers. A handful of weapon stat GEs is negligible.
- Reconcile cost on the client: one `RestoreFrame` plus a re-simulation of the frames since the server frame (about RTT / fixed step, e.g. 100 ms at 60 Hz = 6 frames), 40 `NewObject` ability instances created in that single frame (`ForceCreateNewInstanceOfAbility`, NpASC:2579+), and O(N^2) handle matching in `RestoreAbilities` (40x40, trivial). Possible one-frame hitch from the 40 instance allocations: measure.

### Reconcile storm analysis

- Expected: exactly one reconcile per batch of grants/GEs that lands in the same server tick (spec count and handle count differ), then states match and it stops. If the loadout applies grants over several ticks, one reconcile per differing tick at most (fewer in practice because resimulation overwrites the history).
- Things that would keep it reconciling forever, checked statically: handle ordering (sorted, OK), instance count mismatch (client creates the instances listed in the authority state, OK), `SourceObject` (`ShouldReconcile` compares it, AbDT:770; `NetSerialize` sends null if the object is not `IsSupportedForNetworking`, AbDT:485. Give grants a null or a stably-named asset SourceObject, never a transient UObject, or every frame differs), `DynamicAbilityTags`, `Level`. The `SetByCaller` compare is commented out (AbDT:774-775). `GrantingGameplayEffectHandle` must be INDEX_NONE or a handle that exists on both sides.
- GE: `FEffectSpecSyncData::ShouldReconcile` compares Def, level, duration, period, stacks, captured attributes/tags, dynamic tags, modified values, set-by-caller (EffDT:415-473). Effect context compare is disabled. `StartTimeMS` comes from `StartWorldTime` and is compared (EffDT:536); on the client it is restored FROM the authority (NpASC:3053), so a GE applied outside the sim is consistent after the first restore. Risk to watch live: the start time stamped for a GE applied between ticks on the server (it uses the cached sim time, not a frame boundary).

### Live tests still needed (spike 1)

See the final list, items L1-L3.

---

## 2. Stats modifier (MaxSpeed and gravity)

### Verdict: PASS for the shared-settings question; gravity override is NOT sim state; NEEDS-LIVE-TEST for the forced-reconcile equality

### Evidence

- `UCommonLegacyMovementSettings` is a plain `UObject` (`Plugins/EngineOverrides/Mover/Source/Mover/Public/DefaultMovementSet/Settings/CommonLegacyMovementSettings.h:13`, `MaxSpeed` at :63). It lives in `UMoverComponent::SharedSettings`, declared `UPROPERTY(EditDefaultsOnly, EditFixedSize, Instanced, ...)` (`MoverComponent.h:914-915`). `Instanced` means the object is duplicated per component instance from the archetype; `RefreshSharedSettings` creates missing ones with `NewObject<UObject>(this, SharedSettingsType, ...)` outered to the component (`MoverComponent.cpp:1436-1470`). Modes get it through `FindSharedSettings<UCommonLegacyMovementSettings>()` (`MoverComponent.cpp:2584-2597`; the project does the same at `Source/GASP_Giustino2/Private/GAS/GSTN_GA_Jump.cpp:18`). So it is shared between the MODES of one component (walking, falling, flying all read the same `MaxSpeed`), NOT between components/actors. Two fighters never share a settings object, unless someone makes the component's SharedSettings point to a common asset (not the case for `Instanced`).
- Engine precedent for mutating it at runtime: `FStanceModifier::ApplyMovementSettings` writes `Acceleration` and `MaxSpeed` of the same object via `FindSharedSettings_Mutable` (`StanceModifier.cpp:235-249`) and `RevertMovementSettings` restores them from the CDO's settings (`:260-274`). That one runs in `OnStart`/`OnEnd`, which is rollback-fragile; recomputing every tick from simulated inputs is more robust.
- `OnPreMovement` call site: `MovementModeStateMachine.cpp:288-295`: queued modifiers are flushed into the sync state, then for every active modifier `OnPreMovement(MoverComp, SubTimeStep)` runs BEFORE layered move generation (:297+) and BEFORE `CurrentMode->GenerateMove` (:368). Walking/falling/flying read `CommonLegacySettings->MaxSpeed` inside `GenerateMove` (e.g. `WalkingMode.cpp:97,105`, `FallingMode.cpp:88`, `SimpleWalkingMode.cpp:68`). So a `OnPreMovement` rewrite takes effect in the same substep.
- Rollback safety: `MaxSpeed` is a member of the settings UObject, NOT in the Mover sync/aux state. It is rollback-safe only as a pure function of simulated state recomputed on every `OnPreMovement` (attribute value from the ASC, which is restored by `RestoreAttributeSets` and ticked before Mover, CLAUDE.md rule 5). Never "apply a delta" or cache the previous value: assign `MaxSpeed = Base * f(attribute)` from an immutable base (the CDO/archetype value, as the stance revert does) every substep. Same for `Acceleration`.
- The modifier itself is simulation state: modifiers live in `FMoverSyncState::MovementModifiers` (`MovementModeStateMachine.cpp:271-272, 288-290`). A permanent modifier queued at loadout time from outside the sim goes through the same "outside the sim" injection problem as spike 1 (it reaches the client via the Mover sync state, not the ASC one). Queue it from inside the sim (e.g. on the first tick after the loadout, driven by a sim-visible flag) or test it live (see L5). If a modifier is the problem, a project-side mode subclass reading the speed in `GenerateMove` avoids having any persistent object.
- `SimpleWalkingMode` HAS `MaxSpeedOverride` (`SimpleWalkingMode.h:28-30`, float, `>= 0` wins over the shared settings, used at `SimpleWalkingMode.cpp:67-68`); `SimpleFlyingMode` too (`SimpleFlyingMode.h:30`, `.cpp:47`). `USmoothWalkingMode` derives from `USimpleWalkingMode` (`SmoothWalkingMode.h:16`) so it inherits the property; the file adds no speed logic (grep). `MaxSpeedOverride` is a plain UPROPERTY on the mode instance (modes are per component instance too), so it has exactly the same rollback properties as writing the settings: not sim state, valid only if rewritten every tick. The classic `WalkingMode`/`FallingMode`/`AsyncWalkingMode` have no override and always read the shared settings. `Falling` air control also reads `CommonLegacySettings->MaxSpeed` (`FallingMode.cpp:88`), so the shared-settings write is the single place that covers walk + air; a per-mode override would need to be set on each mode.
- Gravity: `UMoverComponent::SetGravityOverride(bool, FVector)` (`MoverComponent.cpp:2247-2254`) stores `bHasGravityOverride` and `GravityAccelOverride` (both plain `UPROPERTY(EditDefaultsOnly)` members, `MoverComponent.h:917-919, 972-974`, not replicated, not in sync/aux state) and rebuilds `WorldToGravityTransform` / `GravityToWorldTransform`. Same rule as MaxSpeed: not rollback state; call it every `OnPreMovement` with `bOverride=true` and `Gravity = DefaultGravity * Scale`. Where it is read: `GetGravityAcceleration()` (`:2257-2274`) used by Falling (`FallingMode.cpp:119`), AsyncFalling, Swimming (`:58`) and by `GetUpDirection()` (`:2299`). Gotchas: (a) a scale of exactly 0 makes `GetUpDirection` fall back to `DefaultUpDir` (`MovementUtils.cpp:171-181`), harmless for a world-up game but the up vector is no longer derived from gravity; clamp the scale to a small positive minimum if a "zero gravity" state is wanted, or handle it in the jump timeline. (b) `SetGravityOverride` is also used internally around some code paths (`MoverComponent.cpp:2398-2472` temporarily forces zero then restores the previous override): do not call it from outside the modifier while that code runs. (c) The wall modes in the project do not use the Mover gravity override (they use their own `Slide.*GravityScale`, `GSTN_WallSlideMovementMode.cpp:44-47`), so a global gravity scale does not affect wall slide; decide if it should.
- Reading the ASC from a Mover modifier is the allowed direction (rule 1 forbids abilities reading Mover queries, not the reverse), but use the attribute value as of the ASC state of the current frame, not any cached copy.

### Live tests still needed (spike 2)

See final list L4-L5.

---

## 3. GAS triggers in the fork

### Verdict: PARTIAL

- `GameplayEvent` triggers: SUPPORTED statically, needs a live once-and-resim check.
- `OwnedTagAdded` / `OwnedTagPresent` triggers: NOT rollback-safe as the fork is today (fires during `RestoreFrame`). Decision with the user required (CLAUDE.md/m3-plan C0a says stop and ask).

### Evidence

Supported machinery in the fork:

- Trigger registration is ported: `OnGiveAbility` fills `GameplayEventTriggeredAbilities` / `OwnedTagTriggeredAbilities` from `Ability->AbilityTriggers` and binds `MonitoredTagChanged` to the tag event for the tag sources (NpASC:1785-1812); `OnForceGiveAbility` does the same for abilities that appear through a client reconcile (NpASC:2540-2567); removal paths clean up (NpASC:1846+, 2645+, `CheckForClearedAbilities` 1929-1976, 2714+).
- `TriggerAbilityFromGameplayEvent` is overridden and ends in `InternalTryActivateAbility(handle, key, null, null, &EventData)` (NpASC:2331-2359). `HandleGameplayEvent` is NOT overridden: the stock one (`AbilitySystemComponent_Abilities.cpp:2564-2605`) walks `GameplayEventTriggeredAbilities` for the tag and its parents and calls the virtual above, so `SendGameplayEventToActor` / `HandleGameplayEvent` from an ability in the sim tick reaches the trigger path. Event data is part of the synced ability instance (`bHasEventData`, `CurrentEventData`, AbDT:36-41, restored at `NpGameplayAbility.cpp:182-190`, which re-activates a force-activated instance with the stored payload), i.e. the fork was written with triggered activation in mind.
- `InternalTryActivateAbility` (NpASC:2177-2330): refuses on a simulated-proxy role unless `CanPredictAsSimulatedProxy()` (NpASC:2222; true only for a forward-predicted proxy inside the sim tick). Server and autonomous owner activate. Note it contains no `GetIsRestoringFrame()` guard (unlike `GiveAbility`, `ApplyGameplayEffect*`, `CancelAbility*`, `SetTagMapCount`, `UpdateTagMap`).
- In-sim ordering: `TickAbilities` iterates a COPY of the spec array in array order (NpASC:2454-2473). An ability activated by an event raised from another ability's `SimulationTick` is ticked in the same frame if its spec comes later in the array, next frame otherwise. Array order is not synchronized (only the sync-state copy is sorted); `RemoveAtSwap` in `ForceClearAbility` (NpASC:2627) and the different insertion order on a client can change it. This is a latent determinism risk for same-frame triggered abilities; verify live (L6) or make `TickAbilities` iterate in handle order.

The rollback problem for tag triggers:

- `MonitoredTagChanged` (NpASC:2360-2406) activates via `InternalTryActivateAbility` when the owned tag count becomes > 0 (and cancels on 0 for `OwnedTagPresent`). Guard: only `IsOwnerActorAuthoritative()` (server and autonomous owner; not simulated proxies). No restore guard.
- `RestoreTags` (NpASC:3162-3202) writes tag counts with `GameplayTagCountContainer.SetTagCount(...)` directly on the container, bypassing the ASC-level guards (`NpAbilitySystemComponent.h:403-426`). `SetTagCount` -> `UpdateTagMap_Internal` -> `GatherTagChangeDelegates` fires the per-tag `OnNewOrRemove`/`OnAnyChange` delegates on every 0<->N transition (`GameplayEffectTypes.cpp:1076-1101, 1019-1074`, `GameplayEffectTypes.h:1261-1282`). Therefore, inside `RestoreFrame` (and `FinalizeSimulatedTags`), a tag that goes 0 -> 1 because the restored frame differs from the current one fires `MonitoredTagChanged` and activates the triggered ability while `bIsRestoringFrame` is true. `RestoreAbilities` runs BEFORE `RestoreTags` (NpASC:746-747), so the activation would act on an already-restored spec, then the resimulation can activate it again. Result: double or phantom activation after a rollback, only on the owning client (and only when a rollback crosses a tag transition). The server never rolls back.
- Normal (non-restore) operation: tag changes happen inside the sim tick (GE application/expiry, loose tags), so the trigger fires once per simulated frame in which the transition occurs, on server and client, and again when that frame is re-simulated. That is the desired behaviour; the problem is only the extra firing in the restore step.
- Not checked statically (live): `ShouldAbilityRespondToEvent`, tag requirements, and whether a triggered activation in `InternalTryActivateAbility` from a tag delegate raised during `ApplyGameplayEffectSpecToSelf` is safe inside the ASC's own scope locks (the stock code runs the same path, `ABILITYLIST_SCOPE_LOCK` is taken at :2362/:2197).

### Conclusion and minimal alternatives

- Use `GameplayEvent` triggers (or even call a project helper) only; avoid `OwnedTag*` triggers until the fork is patched.
- Minimal fork patch (needs the user's ok, `Plugins/GASNPP` is the fork): add `if (GetIsRestoringFrame()) { return; }` at the top of `UNpAbilitySystemComponent::MonitoredTagChanged` (NpASC:2360). One line; also consider the same guard at the top of `InternalTryActivateAbility`. After the patch, the tag-trigger behavior would be: fires in the sim tick only, and the resim re-fires it exactly like any other in-sim effect.
- No-fork alternative that needs no ASC changes: a trigger table in `UGSTN_GameplayAbility` (event tag or watched owned tag -> ability class/handle) evaluated by a sim-tick step of the combat tick logic (section 7 of the M3 doc), comparing the current tag state against the previous frame's value kept in a sync-state fragment (so the edge detection itself is rolled back). It is fully deterministic, costs one fragment, and does not depend on delegates at all. This is what I would pick if the fork must stay untouched.

### Live tests still needed (spike 3)

See final list L6-L8.

---

## 4. C0c: wire size of the combat sync state fragment array

### Verdict: PASS (formula read from the code; the existing automation tests already assert the exact bit counts)

### Evidence

- `FGSTN_CombatSyncState::NetSerialize` = `Core.NetSerialize(Ar)` then `FGSTN_SyncFragmentList::NetSerialize(Ar, Fragments)` (`Plugins/GSTNCombatNP/Source/GSTNCombatNP/Public/Simulation/GSTN_CombatSimTypes.h:64-68`). The combat model has no delta serialization: it ignores `BaseDeltaStatePtr`, so the full state is written on every send.
- Fragment list format (`GSTN_SyncFragment.h:99-114`, implementation `Private/Sync/GSTN_SyncFragment.cpp:109-166`): 1 bit "any fragments"; if 1: `SerializeIntPacked(count)`; per fragment (sorted by id): `uint8` id, `SerializeIntPacked(length)`, then `length` raw bytes produced by the fragment's own `NetSerialize` into a byte-oriented `FMemoryWriter` (no bit packing inside the payload: a bool costs a byte).
- `SerializeIntPacked` is byte-granular (7 data bits per byte): values 0-127 cost 8 bits, 128-16383 cost 16 bits. Count is at most 16, so 8 bits. Payload length is at most 512 (`MaxFragmentBytes`): 8 bits if L < 128, otherwise 16 bits.

### Formula and numbers (bits)

```
Bits(list) = 1                                              for F = 0
           = 1 + 8 + SUM_i ( 8 + P(L_i) + 8*L_i )           for F in 1..16
P(L) = 8 for L < 128, 16 for 128 <= L <= 512
```

| Fragments | Formula | Example (matches the tests) |
|---|---|---|
| 0 | 1 bit | 1 bit (test "zero fragments cost one bit", `GSTN_CombatNPTests_Fragments.cpp:109-111`) |
| 1, payload L bytes (L<128) | 25 + 8L bits | L=5: 65 bits = 9 bytes (test line 155: `+1 + 8 + 8 + 8 + 8*5`) |
| 2, payloads L1, L2 | 41 + 8(L1+L2) bits | L1=5, L2=4: 113 bits = 15 bytes (test line 207) |
| F, all L<128 | 9 + 16F + 8*SUM(L) | e.g. 4 fragments of 8 bytes: 9 + 64 + 256 = 329 bits |

Overhead per fragment beyond the payload: 16 bits (id + length), plus 8 bits for the count byte once, plus the presence bit. Total state = core bits + list bits. The core is variable (`FGSTN_InputSimState`, history/classifier) plus two int32 hitstop frames (64 bits); test log lines `Fragments.RoundTrip0/1/2` print the real totals when the tests run. The 0-fragment case adds exactly one bit to the pre-fragment wire format.

Notes for C3: the cost of byte-granular payloads (an 8-bit flag per bool) is acceptable for the planned fragments, but a fragment that wants bit packing should write whole bytes itself. The two-fragment bytes are identical whatever the array order (sorted by id on write; test line 220-222). Unknown ids are skipped by the reader, so an old client does not desync.

No live test needed for the formula. The running automation test log is a sanity check (L9).

---

## Live tests still required (minimal C++/PIE steps)

All in the existing PIE harness (`Tools/GASNPPTest/gasnpp_test.py`, listen primary + dedicated secondary, 2 clients, 60/15 ms + 2 % loss at the end). Test component C++ only; no plugin changes needed for L1-L5, L6-L8 only add a test ability class.

L1. Grant and lookup (spike 1). Test-only server RPC/console command that, outside the sim tick (a plain game-thread call), calls `GiveAbility` for 40 `UGSTN_GameplayAbility` test classes (mix: 30 `InstancedPerActor` predicted, 10 `ServerOnly`) and applies 3 infinite GEs with modifiers. On the owning client log, every frame: `FindAbilitySpecFromClass` for the 40 classes (count found), `np.PrintReconciles`, and the frame/time the count reaches 40. Pass: reached within about RTT; exactly one reconcile (reason "Different number Of Activatable Abilities" or handle count), none afterwards for 10 s idle; same on the listen-host remote client; the host itself has no reconcile.
L2. Bytes (spike 1). `stat net` or `net.PacketLoss`-free run with `Net.ShowNetTrafficStats`/NP insights (or log `Writer.GetNumBits()` around `FAbilitySimSyncState::NetSerialize` for the owner target) for 0 and 40 specs, idle and during a grant. Report bytes per send and bytes/s for 1 and 2 clients (also in the listen host upload). Compare with the estimate (about 160-200 B/send idle at 40 specs).
L3. Robustness (spike 1). Repeat L1 with 60/15 ms and 2 % loss, with a grant while the owner is mid-jump and mid-ability (an active ability whose instance state is in the sync state). Also grant, then revoke (`ClearAbility`) and re-grant a different batch (loadout swap): check no reconcile storm, no `ForceGiveAbility`/`GiveAbility on simulated proxy` errors on the owner, and that a remote (simulated proxy) client sees NO specs (confirms the design assumption). Time the single-frame cost of the 40-instance restore (insights or `SCOPE_CYCLE_COUNTER` print).
L4. Stats modifier (spike 2). Test modifier (`FMovementModifierBase` subclass, `DurationMs = -1`) queued once; each `OnPreMovement` assigns `MaxSpeed = CDOBase * K` and `SetGravityOverride(true, Default*Scale)` from an attribute. An infinite GE changes the attribute; force reconciles with `np.ForceReconcile`-style console command (or the harness' forced reconcile) every 30 frames, across the GE change and across a jump apex. Pass: server and client position/velocity identical (harness compare), no reconcile caused by the change itself except the one carrying the attribute, and no divergence after forced reconciles. Also run two pawns with different K simultaneously and confirm each keeps its own speed (confirms per-component settings).
L5. Queueing path (spike 2). Queue the permanent modifier from the loadout step (outside the sim) and check it reaches the owning client and remote client without a storm; if it does not, queue it from a sim-side step. Verify `MaxSpeed` and gravity are re-applied after a rollback that happens before the modifier existed on the client.
L6. Triggers, GameplayEvent (spike 3). Ability A with `AbilityTriggers = {GameplayEvent T}` on B's side; ability B calls `SendGameplayEventToActor(T)` in its `ActivateAbility` and from a tick of its `SimulationTick`. Count `ActivateAbility` calls per sim frame on server and client; force a re-simulation over that frame and count again. Pass: once on each machine, exactly once more per forced resim of that frame, no reconcile. Include the same-frame vs next-frame tick order (spec array order) check by swapping the grant order of A and B.
L7. Triggers, OwnedTag (spike 3). Ability C with `AbilityTriggers = {OwnedTagAdded X}` and a GE that grants X inside the sim. Same counts as L6. Expected FAIL on the client before the patch: force a rollback across the tag transition (restore to a frame without X, then resim) and count `ActivateAbility`; extra activation inside `RestoreFrame` confirms the finding. Re-run after adding the `GetIsRestoringFrame()` guard in `MonitoredTagChanged` (only if the user approves the fork patch) or test the trigger-table alternative.
L8. Same as L6/L7 on a listen host: the host activates once, the remote client once, no double fire from "server" and "local" paths.
L9. C0c sanity: run the existing NP automation tests (`GSTN_CombatNPTests_Fragments.cpp`, console `Automation RunTests` for the GSTN combat NP group) and copy the `Fragments.RoundTrip0/1/2` log lines into combat.md §17.

## Open decisions for the user

1. Spike 3: accept GameplayEvent triggers only (no fork change), patch `MonitoredTagChanged` (one line in `Plugins/GASNPP`), or use the trigger-table approach in `UGSTN_GameplayAbility`.
2. Spike 1: if the idle ability bookkeeping cost (about 160-200 bytes per send at 40 specs) matters, mark passive/cosmetic abilities `ServerOnly`/`LocalOnly` or grant only the kit's current move set (the loadout compiler decides), instead of all 40 at once.
3. Spike 2: the stance modifier precedent shows it is acceptable to mutate the per-component settings, but the update must be an every-tick assignment from simulated data. No Mover override change is needed for MaxSpeed (plan item 12 / rule 8 not triggered).

## Live results (2026-10-22, listen host + 2 clients, 60/15 ms, Development editor, patched fork)
See takeover-state.md last entry for the full list. Summary: grants (L1-L3) functionally pass, 1 reconcile per grant/revoke, owner sees 40 specs within ~0.1 s, remote clients none; stats (L4) per-pawn MaxSpeed/gravity written from simulated scales, separate Mover settings per pawn (B's gravity 4x A's), apex differs 238 vs 60 cm, speed-peak check invalid (pawn blocked by level geometry); triggers (L6-L8): single press = single activation everywhere, none during restore in the forward case, server-only tag arrives through a restore; the forced-resim re-fire checks (E2, E3, T2) did not pass and one run showed extra owner forward activations: unresolved. Byte counts (stat net) were not measured.

## Live findings, 2026-10-22 evening (listen, 60/15/0, harness map DefaultLevel_Harness)
- abilities_startup(3) with PR #7 merged: 3/3 runs clean, 0 mismatches at startup and in the window, 0 offset changes, CMP 6/6. The "2 in-window singles" of the earlier run did not reproduce (no GASNPP_MODE lines were emitted: the cvar `gstn.test.LogMoverModeChanges` is per PIE session, set it again after a restart).
- spike_triggers (client 1), with GASNPP_ACT logging: the re-fire IS happening. Owner (role=2) logs the same activation twice at the same frame number (E2: frame 18150 at t=05.503 and t=05.587; T2: frame 18600 twice), server (role=3) once, other client once. Both owner lines say `resim=0 restoring=0`: the probe classifies with NpASC->IsReSimulating(), which is false in the path that re-runs the activation. So the checks 'owner re-fired 0 times / forward 3 want 2' are a PROBE CLASSIFICATION problem, not a patch problem: a re-fire shows up as a second "forward" activation at an already-seen frame. Fix the probe/expectation: count a re-fire when an activation is logged at a frame <= the highest frame already activated on that machine (or use the NP time step's resimulation flag), then want forward==1 (+ re-fires 1..2 per forced reconcile). Not yet changed (needs a PIE re-run).
- The tag-restore case works as designed: T3 owner logged `TagTarget role=3 ... ` only on the server at 18756 and one `restoring=1` activation on the owner at 18771 (the activation guard did not suppress the restore-phase observation, counters show attempts 0 / activationsRestoring 1 as the check expects). The failing check 'T3: the owner reconciled' reads 0 mismatches although a restore happened: the reconcile was probably counted as forced/other, re-check the counter it reads.
