# Project patches to the GAS-NPP fork

`Plugins/GASNPP/` is a fork of Epic's GameplayAbilities, NetworkPrediction (+ Extras, Insights) and the
AbilitySystemSimulation plugin, ported to UE 5.8. This file lists every change this project made on top of the port, why
it exists and where to look, so a future re-sync with upstream (or with a newer fork) can carry them over.

Rules for new patches: keep them small, comment the *why* in the code, add a line here, and never restyle upstream code.
Related architecture notes: [gas-npp.md](../../docs/architecture/gas-npp.md), [combat.md](../../docs/architecture/combat.md) (section 12.7 for the spike).

Paths below are relative to `Plugins/GASNPP/`. `NP` = `NetworkPrediction/Source/NetworkPrediction`,
`ASC` = `AbilitySystemSimulation/Source/AbilitySystemSimulation`.

## 1. Rollback and reconcile correctness

| Patch | Why | Where |
|---|---|---|
| Simulation views point at the restored frame on rollback (`d53dff2`) | The state view (pending frame, sim time) was only updated by `DoTick`, so a simulation sorted before an instance's resim tick read the pre-rollback frame: queued layered moves, gameplay effects and cues got wrong start times. The first resim step now sets every view to what `DoTick` would have left. | `NP/Public/Services/NetworkPredictionService_Rollback.inl` |
| Skip server states ahead of the local pending frame | Until the server consumes a client's first input the offset is 0 and the server frame maps past the local pending frame. The never-simulated slot was compared and reported 6-15 false mismatches per PIE start (`RollbackFrame N AHEAD of PendingFrame 0`, a no-op rollback). Now skipped; a simulated proxy still copies the received input to the head. | `NP/Public/Services/NetworkPredictionService_Rollback.inl` |
| Ignore the montage pause flag when no montage plays (`3520b35`) | `FMontageSimSyncState::NetSerialize` only writes `bIsPaused` with a montage, but `ShouldReconcile` compared it always: a reconcile on every compared frame after a hitstop pause without montage. | `ASC/Private/MontageSimulator/NetMontageSimulatorData.cpp` |
| No owned-tag triggered activation while a frame is restored | `RestoreTags` writes the tag counts straight into the count container, which still broadcasts the tag delegates. A tag that differs between the restored frame and the current one (typically an effect applied by the server outside the simulation) made `MonitoredTagChanged` call `InternalTryActivateAbility` (or cancel) inside `RestoreFrame`, where the restored ability state is already authoritative: a double or phantom activation on the owning client. Now `MonitoredTagChanged` returns at once while `GetIsRestoringFrame()`; the resimulation fires the trigger again from the tag changes it simulates. GameplayEvent triggers are untouched (events only come from the simulation). | `ASC/Private/Abilities/NpAbilitySystemComponent.cpp` (`MonitoredTagChanged`) |
| Harden rollbacks older than the frame history (`951c495`) | A server state older than the 64-frame ring was compared with, and rolled into, a slot already holding a newer frame. It is now ignored (rate-limited warning) and forced rollbacks are clamped to the history. Delta serialization no longer keeps every received state forever. `NpInstanceFrameBufferSize` replaces a hard-coded buffer size. | `NP/Public/Services/NetworkPredictionService_Rollback.inl`, `NetworkPredictionInstanceData.h`, `NetworkPredictionSerialization.h`; `NetworkPredictionExtras/.../MockRootMotionComponent.cpp` |

Re-applying the owned-tag patch on a fork update: in `UNpAbilitySystemComponent::MonitoredTagChanged`, put `if (GetIsRestoringFrame()) { return; }` (with the comment) before `ABILITYLIST_SCOPE_LOCK()`. `bIsRestoringFrame` is true only inside `RestoreFrame` (it covers `RestoreTags`, which runs after `RestoreAbilities`) and false again before the re-simulation ticks that follow (`NetworkPredictionService_Rollback.inl`: `RestoreFrame`, then `DoTick` with `Resimulate`), so tag changes inside re-simulated ticks still trigger abilities. Check with `scenario_spike_triggers` (it fails on `CanActivateAbility` calls made while restoring); `scenario_spike_triggers(fork_patched=False)` shows what the unpatched fork does.

## 2. Input buffer and hitch recovery

| Patch | Why | Where |
|---|---|---|
| Hitch-safe input buffer (`5a5fbcd`) | A 300 ms hitch caused three input-offset reconciles and ~80 "Gap in input stream" faults. The server now holds the last frame when starved (instead of rewinding and replaying consumed inputs) and skips only when the client is further ahead than the 64-frame ring. The client sends one RPC per engine frame, new frames only when the connection is ready, redundant copies only without backlog. | `NP/Private/NetworkPredictionWorldManager.cpp`, `NetworkPredictionPlayerControllerComponent.cpp`, `Services/NetworkPredictionService_Input.inl` |
| `InputType::PrepareRepeatedInput()` hook (`5a5fbcd`) | When the fork repeats an input (held tick, gap fill) edge-triggered data must not act twice. Optional on the input type; `FAbilitySimInputCmd` implements it. | `NP/Public/NetworkPredictionDriver.h`; `ASC/Public/DataTypes/AbilitySimulationDataTypes.h` |
| Mover and combat input implement the hook (`3ff9286`, `282fd92`) | A repeated `FCharacterDefaultInputs` kept `bIsJumpJustPressed` and the suggested mode and could jump twice. | `Plugins/EngineOverrides/Mover` (`FMoverInputCmdContext::PrepareRepeatedInput`, marked "GAS-NPP fork"); input: `FGSTN_InputCmd` in `Plugins/GSTNInputNP/Source/GSTNInputNP/Public/Simulation/GSTN_InputSimTypes.h` |
| Late input frames cannot repair gaps | Built and measured (0 repairs in 30 hitches), reverted. Recorded so it is not retried. | commit `4d9ceef` |

## 3. Hook for other simulations

| Patch | Why | Where |
|---|---|---|
| `UNpAbilitySystemComponent::OnSimulationPreTick` (`d6039e2`) | The ASC sync state is closed and its tick is not virtual. The delegate fires at the start of every (re)simulated tick, after the time step is cached and before input events and abilities, so an earlier-sorted simulation (the combat model, priority 47) can issue its frame's activations, cancels and montage changes inside the ASC tick. | `ASC/Public/Abilities/NpAbilitySystemComponent.h`, `ASC/Private/Abilities/NpAbilitySystemComponent.cpp` |

## 4. Spike S-FP: forward prediction of simulated proxies (inert)

Kept in the fork as recorded in combat.md 12.7 (`bdfa7e8`). **Every cvar defaults to 0, so behaviour is unchanged.** The
spike measured that forward prediction is not worth enabling yet; the report is the reference if it is revisited.

| Cvar (default 0) | Effect when 1 | Where |
|---|---|---|
| `np.Spike.SPPrepareRepeatedInput` | Forward-predicted proxies call `PrepareRepeatedInput` on the input they repeat. | `NP/Public/Services/NetworkPredictionService_Ticking.inl` |
| `np.Spike.SPPredictActivation` | A forward-predicted proxy may activate abilities / apply effects to itself, only inside its own simulation tick. | `ASC/Private/Abilities/NpAbilitySystemComponent.cpp`, `NpGameplayAbility.cpp` |
| `np.Spike.SPSendAttributeBase` | Also send attribute base values to simulated proxies. **Changes the wire format** (same value needed on both ends); a production version would make it unconditional. | `ASC/Private/DataTypes/EffectsDataTypes.cpp` |
| `np.Spike.LogResimCost` | On clients, log re-simulation wall time and frame count every 5 s. | `NP/Private/NetworkPredictionWorldManager.cpp` |

## 5. Engine overrides that go with the fork

`Plugins/EngineOverrides/` holds engine plugins copied into the project so they build against the forked plugins (Mover,
ChaosMover, SmartObjects). Mover's NPP liaison forwards the re-simulation flag to Mover, and Mover implements the
`PrepareRepeatedInput` hook above.

| Patch | Why | Where |
|---|---|---|
| `DidGenerateMove` blackboard entry kept for 64 frames (`84e4893`) | With 2 slots any rollback deeper than one frame invalidated it and smooth walking reset its spring on the first re-simulated frame (reconcile chains after dashes). | `Mover/Private/DefaultMovementSet/Modes/SimpleWalkingMode.cpp` |
| Smooth walking spring tolerances and `ToString` (`84e4893`) | Tolerances in line with the default sync state; `ToString` passed a `TCHAR*` to an ANSI builder. | `Mover/Private/DefaultMovementSet/Modes/SmoothWalkingState.cpp` |
| `FLayeredMoveFinishVelocitySettings::NetSerialize` writes its settings (`84e4893`) | The flag was never written when saving, so replicated moves ended with ClampVelocity/SetVelocity on the server only. | `Mover/Private/LayeredMove.cpp` |
| Re-simulation flag forwarded to Mover (`53c8716`) | Mover's own queries need to know they run inside a re-simulation. | `Mover/Private/Backends/MoverNetworkPredictionLiaison.cpp` |

## Verifying a change to the fork

Run the PIE harness (`Tools/GASNPPTest/gasnpp_test.py`, dedicated server + 2 clients, `m.net(60, 15, 2)`) with scenarios
`abilities`, `forced_rollback`, `server_dash` and `combat_spike`: every `CMP` line must be `OK`. See
[testing.md](../../docs/architecture/testing.md).
