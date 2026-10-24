# M4 H0 spike report (2026-10-24)

Source: plan `m4-hits-plan.md` H0 and §4, spec `combat-m4-hits.md` §2, §8, §9.
Status: **only the static (source) parts of H0b and H0e are done. H0a, H0c, H0d and the live parts of H0b/H0e are pending: editor.**
Why: Rider had no project open (Welcome screen), so the Rider MCP (`ue_health`, `ue_execute_python`) had no target. A command-line
launch with the .uproject did not open it, the click route was denied on the laptop, and after Rider was restarted once the
Rider MCP disconnected from this session. No editor ran, no PIE, no number below is measured. No code was written or changed.

## H0a cross-entity cost: pending: editor
Setup (not run): listen, 3 players, `m.net(0,0,0)` then `m.net(60,15,2)`; M0 hitstop debug button on client 1 writing a request
into client 2's combat state on the authority (throwaway). Measure: client 2 rollbacks per hit, visible delay on client 1,
host rollbacks (expected 0), bytes/s per client. Stop-and-ask: client 2 > 1 rollback per hit clean, or host reconciles.
Verdict: **not measured**. Gates H9 acceptance 2 and H10.

## H0b freeze + Mover tag
Static result (done): **a layered move CAN expose a Mover gameplay tag; the stop-and-ask row does not apply.**
- `UMoverComponent::HasGameplayTag` (`Plugins/EngineOverrides/Mover/Source/Mover/Private/MoverComponent.cpp:683`) forwards to
  `HasGameplayTagInState` (`:693`) in the sync backend. That function checks, in order: external/loose tags (`:696-707`), the
  active movement mode (`:710-716`, `UBaseMovementMode::HasGameplayTag`, `MovementMode.h:95`), movement modifiers in the sync
  state (`:719-729`, `FMovementModifierBase::HasGameplayTag`, `MovementModifier.h:195`), and **active layered moves in the sync
  state (`:732-738`, `FLayeredMoveBase::HasGameplayTag`, `LayeredMove.h:112`, virtual)**.
- Existing layered moves override it (`AnimRootMotionLayeredMove.cpp:126`, `RootMotionAttributeLayeredMove.cpp:193`), so
  `FGSTN_LayeredMove_Freeze` overriding it to return true for `Combat.Mover.Frozen` is the same pattern; no `FGSTN_FrozenModifier`
  is needed. `CancelMovesByTag` (`LayeredMove.cpp:147`, `:405`) uses the same virtual, so a freeze could also be cancelled by tag.
- Caveats: (1) the sync path reads `MoverSyncStateDoubleBuffer.GetReadable()` (`:690`), the last committed state, so a freeze
  queued this frame is visible to `HasGameplayTag` only once active (CLAUDE.md rule 5: one-frame lag from Mover to abilities).
  The wall lock inside the mover tick should use `HasGameplayTagInState(SimState.SyncState, ...)` (pattern at
  `RootMotionAttributeLayeredMove.cpp:138`). (2) With `mover.UseSnapshotGameplayTagsForSyncBackend != 0` or an async backend the
  result comes from the `AllGameplayTags` snapshot (`:685-688`, cvar at `:73-80`, default off): re-check if that is ever enabled.
Live part (pending: editor): Freeze (OverrideAll, zero velocity) in Walking, Falling and each wall mode with forced reconciles
inside the freeze; check the saved incoming velocity is restored after a reconcile.
Verdict: **pass on the tag question (source); live freeze behaviour not measured**. Implication for H7: put the tag on the
Freeze layered move; wall lock `IsMovementLocked` reads `HasGameplayTagInState` on the start sync state.

## H0c hitstop with a montage: pending: editor
Setup (not run): M0 pause path (`scenario_combat_spike`, `HitstopTestButton`) with a real C10 montage, forced reconcile inside
the window; compare montage position on the client before/after the reconcile and against the server. Stop-and-ask: divergence.
Verdict: **not measured**. Gates H7.

## H0d interpolation buffer: pending: editor
Setup (not run): `FixedTickInterpolationBufferedMS` 100 -> 80, 60, 50, 40 under 60 ms / 15 ms jitter / 2 % loss; sample the
interpolated proxy position per editor tick for ~10 s, report max and p99 frame-to-frame jump and the count of backward/zero
steps per value. The ini change is local only and must be reverted (owner OK needed, §6). Verdict: **not measured**; H10 keeps
100 until measured.

## H0e lag compensation API
Static result (done): the API **exists in the fork** and is built for Network Prediction actors.
- `UNetworkPredictionLagCompensation` component: `Plugins/GASNPP/NetworkPrediction/Source/NetworkPrediction/Public/NetworkPredictionLagCompensation.h`
  (`CaptureState` `:50`, `SetOwningActorState` `:57`, `CapturePreRewindState` `:62`; history struct picked by a `MetaStruct`
  property `:32`, data type in `NePredictionLagCompensationData.h`).
- `UNetworkPredictionWorldManager::RewindActors(AActor* Requesting, float RewindSimTimeMS)` / `UnwindActors()`:
  `NetworkPredictionWorldManager.h:257,260`, body `NetworkPredictionWorldManager.cpp:918`. Registered components are rewound
  except the requesting actor (`:958`); standalone returns false (`:925`); on the authority the time is clamped by
  `ClampRewindingTime` (`:1111`) to `Settings.MaxRewindTimeMS` + interpolation delay + (`FixedTickDesiredBufferedInputCount`+1)
  fixed steps (`GetMaxRewindDuration`, `:1127-1134`), so **a 250 ms window needs `MaxRewindTimeMS` set high enough**; the
  rewind logs a warning when clamped (`:945`).
- Already used by the fork's targeting: `AbilitySystemSimulation/.../TargetingLibrary.cpp:92-99` and
  `TargetingPredictionTask.cpp:214-225` (rewind with `GetSyncedInterpolationTimeMS()`, then unwind), the same call shape spec §9 plans.
- Not yet confirmed (pending: editor): that the default data struct captures the Mover transform of the test pawn (and not only
  a physics body), the `MaxRewindTimeMS` default, and the host cost in ms of one rewind of 3 pawns.
Verdict: **component present, not "missing"; "rewinds the wrong thing" not yet ruled out**. Implication for H10: plan to use
`RewindActors`/`UnwindActors`; add `UNetworkPredictionLagCompensation` to the test pawn and verify before relying on it.

## Summary

| Spike | Verdict | State |
|---|---|---|
| H0a | n/a | pending: editor |
| H0b | pass on the tag question (layered move carries the tag) | live freeze pending: editor |
| H0c | n/a | pending: editor |
| H0d | n/a (keep 100 ms) | pending: editor |
| H0e | API present | transform rewind and cost pending: editor |
