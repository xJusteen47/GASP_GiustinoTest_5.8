# M4 H0 spike report (2026-10-24)

Source: plan `m4-hits-plan.md` H0 and §4, spec `combat-m4-hits.md` §2, §8, §9. Run live on the laptop (HEAD 12a9601), PIE listen with
3 players in one process unless noted. Editor frame rate during the runs was about 19 fps (3 viewports), so every wall-clock number
below has a resolution of one editor frame (about 50 ms). Throwaway code (patch: `spikes/m4-h0-spike.patch`, applies on 12a9601) was
reverted before this commit; scripts were scratch files.

| Spike | Verdict against §4 | Gates |
|---|---|---|
| H0a cross-entity cost | pass: 1 rollback per hit on the victim, 0 on the host | H9 acc. 2, H10 |
| H0b freeze + tag | pass: layered move carries the tag; freeze holds in Walking, Falling, WallSliding; reconcile inside is clean | H7 |
| H0c hitstop with montage | pass: the montage holds through the window and matches the authority after a forced reconcile inside it; a bystander client has no montage state | H7 |
| H0d interpolation buffer | 80 ms matches 100 ms; 60 ms is worse | H10 |
| H0e lag compensation | pass: rewinds the Mover pawn at 250 ms, 0.22 ms per rewind+unwind | H10 |

## H0a cross-entity cost
Setup: throwaway `Spike_ServerOpenHitstop(8)` on the host's copy of client 2's `UGSTN_CombatSimComponent`
(`NetworkPredictionProxy.WriteSyncState`, the same call `ServerWriteLoadoutRevision` uses), standing in for "client 1 hit client 2,
the host resolved it". 10 hits clean, 12 hits at `m.net(60, 15, 2)`, 1.6 s apart. Times are from the host call to the first editor
frame where that world's copy of client 2 shows the new hitstop window.

| Profile | Victim (c2) rollbacks / hit | Host reconciles | Window seen: host | Window seen: victim own copy | Window seen: attacker (c1) view of c2 |
|---|---|---|---|---|---|
| clean | 1.0 (10/10 hits) | 0 | 33-41 ms | same frame | same frame |
| 60 ms, 15 ms jitter, 2 % loss | 1.0 (12/12 hits) | 0 | 33-42 ms | 101-146 ms (median 108) | 141-184 ms (median 172) |

- Clean: in one process with no emulation the packet arrives in the same editor frame, so the clean "visible delay" is below the
  resolution. At 60 ms the attacker sees the victim's reaction about 170 ms after the host decided: one way (about 60 ms) plus the
  interpolation buffer (100 ms). Presentation was off.
- c2 total rollbacks in the lossy run: 15 for 12 hits; the 3 extra are the known loss-induced mismatches (testing.md), none clean.
- Bytes per second per client: **not measured** (no Python accessor for the net driver). From H6 (HANDOVER 20): a victim in hitstop
  serializes 67 B against 37 B idle, so +30 B per state packet while the window is open.
- Verdict: pass. Implication: H9 acceptance 2 can assert "victim 1 rollback per hit, host 0". The attacker-side delay is dominated by
  the interpolation buffer, so H0d's lower value shortens it directly.

## H0b freeze + Mover tag
Static result from the earlier session stands: `FLayeredMoveBase::HasGameplayTag` is read by `UMoverComponent::HasGameplayTagInState`,
so `FGSTN_LayeredMove_Freeze` can carry `Combat.Mover.Frozen`; no modifier needed.
Live: throwaway `FGSTN_LayeredMove_Freeze` (OverrideAll, zero velocity, 1000 ms; 800 ms in Falling) queued on the host copy and on
client 1's copy in the same editor tick, `np.ForceReconcile` 0.5 s later, sampled on host, owner and the other client. Clean network.

| Case | Auth drift in window (x / y / z cm) | Tag seen (auth / owner / other client) | Rollbacks on c1 | After the window |
|---|---|---|---|---|
| Walking, holding forward | 0 / 0 / 0 | 100 % / 79 % / 100 % | 2 | speed 0 -> 400 again (accelerates from zero) |
| Falling, mid-air (up velocity 621) | 0 / 0 / 0 at z 140.5 | 100 % / 82 % / 94 % | 3 | falls from zero (-157 cm/s first sample) |
| WallSliding (test pawn) | 0 / 5.7 / 0 (first sample presses into the wall) | 100 % / 87 % / 100 % | 3 | Falling from zero |

- Owner drift (up to 18.6 cm in Walking, 11 cm in Falling, 45 cm in WallSliding) and the owner/other-client tag fractions below 100 %
  come from the two queue calls landing on different simulation frames; the reconcile pulls every copy to the authority's frozen
  position (no copy ended apart). The other client's tag lingers 1-2 samples after the window (interpolation).
- **Incoming velocity is not restored**: OverrideAll with zero velocity drops it, so after the window the pawn restarts from rest.
  If hitstop must keep momentum, H7 has to save the velocity itself (sync state).
- **The wall mode leaves during the freeze**: in the WallSliding case the authority went WallSliding -> Falling at about 0.95 s while
  the tag was still set (position stayed fixed because of OverrideAll). Mode transitions keep running under a freeze, so the wall
  lock must be an explicit gate (`HasGameplayTagInState` on the start sync state), not a side effect of the freeze.
- Not exercised: Simple / Skidding / Controlled wall modes (registered on the shared `SandboxCharacter_Mover` Blueprint only; the
  test pawn has WallSliding). The freeze is a layered move, independent of the mode, so no mode-specific risk is expected.
- Verdict: pass (the tag question and the freeze). Implication for H7: put the tag on the Freeze layered move; save the velocity
  if it must come back; keep the wall lock gate.

## H0c hitstop with a montage
Retried in the harness session of 2026-10-24 (HEAD ac98c3b, patch `spikes/m4-h0c-spike.patch`, reverted). Throwaway: `SpikeMontage` +
`Spike_SetMontage` + `Spike_GetMontage` (ASC `GetPlayingSyncedMontage` time, latest frame, hitstop window) on `UGSTN_CombatSimComponent`, and
`PlaySyncedMontage(SpikeMontage)` in the M0 hitstop trigger, so the montage starts inside the simulation on the same frame as the window.
Montage: `M_interaction_bench_into_BR_LFoot_Montage` (12.3 s, a content montage: the project has no C10 attack montage yet). Listen, 3 players,
`m.net(60, 15, 0)`; client 1 taps the test hitstop button (8 frames, the Blueprint value), `np.ForceReconcile` with extra frames fired while the
window is open; montage time sampled on every editor tick on host, owner and the other client.

| Run | Reconcile | Owner rollbacks / resim ticks | Frames compared host vs owner | Max difference | Montage time ever went back |
|---|---|---|---|---|---|
| 1 | 0.25 s after the tap (window frames 3099-3107), 10 extra frames | 1 / 13 | 25 | 0.000 s | no |
| 2 | 0.12 s after the tap (window 7756-7764), 20 extra frames | 3 / 53 | 29 | 0.000 s | no |

- On both machines the montage position stays at 0.000 s for every frame of the window and then advances 0.016 s per frame from the first
  frame after `HitstopEndFrame`; the owner re-simulated the window and ended on exactly the authority's values. The pause path (pause on every
  hitstop frame, resume at the end frame, both derived from the sync state) is rollback-safe for a montage started in the same frame.
- The other client (simulated proxy) never shows a playing synced montage (0 of 100 samples): the montage state is simulation state of the ASC
  and a proxy does not run it. If H7 needs the observer to see the montage frozen, that comes from presentation, not from this state.
- Not measured: a montage started before the window and paused mid-way (the code takes the same branch on every frame), a reconcile that
  restores a frame before the montage started, and loss (clean network).
- Verdict: pass. No fork fix needed; H7's montage part can proceed.

### The crashes of the first attempt
Crash dirs `Saved/Crashes/UECC-Windows-5C644DB3...`, `...6753168E...` (and two more in this session, `...7348F507...`, `...8BBBE5B0...`). The
first two: `Ensure this->IsBound()` ("Unable to bind delegate to 'OnOwnerActorDestroyed'") in `UAbilitySystemComponent::SetOwnerActor` <-
`UNpAbilitySystemComponent::InitAbilityActorInfo` <- `UGSTN_AbilityInputComponent::TryBind` (tick), i.e. `RefreshAvatarActor` on a pawn whose
owner is already pending kill, then an access violation at 0x38. The two in this session: the same address, in `UFallingMode::GenerateMove`
(`FallingMode.cpp:80`, `CommonLegacySettings` null) called from `UMoverTrajectoryPredictor::Predict` (PoseSearch of the pawn's animation) on a
pawn whose components were already invalid (`ObjectInstance is null` when Python touched them). Common step in all four: Python
`set_editor_property` on a **live component instance** in a PIE world (`spike_montage`, `hitstop_test_frames`), together with a slate-tick
callback that raised every frame (`NameError`, the exec globals are gone, see HANDOVER 21 lesson 1). The runs that did not crash set the montage
through a `UFUNCTION` setter and sampled from a module-level callback that unregisters itself. Which of the two is the cause was not isolated
(one factor changed at a time was not affordable); the safe rules are both. Not fixed in engine or fork code. Also seen: a locked
`UnrealEditor-GASP_Giustino2.dll` after a crash (LNK1104) until the stale editor/crash reporter processes were gone.

## H0d interpolation buffer
Setup: `FixedTickInterpolationBufferedMS` set on the settings CDO from Python before each PIE start (in memory, not saved), listen,
`m.net(60, 15, 2)`, client 1 walks +Y/-Y legs of 2.5 s (reversal transients skipped), client 2's world samples client 1's simulated
proxy position each editor tick for 10 s, two runs per value. Jump = frame-to-frame displacement in cm (about 18-20 cm is the
mean step at 19 fps); speed cv = stdev/mean of displacement / frame time.

| Buffer ms | Max jump cm (run1/run2) | p99 jump cm | Speed cv | Backward steps | Zero steps |
|---|---|---|---|---|---|
| 100 | 31.6 / 27.2 | 28.3 / 27.1 | 0.26 / 0.27 | 0 / 0 | 3 / 5 |
| 80 | 32.6 / 35.1 | 27.9 / 29.8 | 0.23 / 0.22 | 0 / 0 | 0 / 0 |
| 70 | 33.5 / 36.5 | 24.7 / 24.0 | 0.13 / 0.16 | 0 / 0 | 0 / 0 |
| 60 | 39.6 / 58.4 | 37.1 / 41.3 | 0.26 / 0.34 | 0 / 0 | 0 / 0 |

50 and 40 ms were not run: 60 ms is already clearly worse (max jump +25 to +85 %), and the trend is monotone.
70 ms also matches 100 ms (its runs had 21 fps against 19, which shrinks frame jumps), but it sits 10 ms above the visible
degradation at 60 and has only two samples. Verdict: **80 ms** (saves 20 ms of every remote action's delay, which is the attacker-side
170 ms above). Implication for H10: use 80; 70 is a candidate if a longer measurement on a lower-load machine confirms it.
The ini is committed at 80 in its own commit.

## H0e lag compensation
Setup: throwaway `AddLagComp` (a `UNetworkPredictionLagCompensation` cannot be added to a pawn from Python) on the 3 pawns, host world;
client 1 walks 3 s while its authoritative copy is sampled with `GetCurrentLagCompensationTimeMS`; then
`RewindActors(hostPawn, now - 250)` / `UnwindActors()`. Clean network.

| Measure | Value |
|---|---|
| Registered components | 3 (requester skipped by the manager) |
| Window requested / effective | 250 ms; the pawn moved 106.4 cm back (about 425 cm/s x 250 ms), no clamp seen |
| Rewound y vs recorded y at that sim time | 467.5 vs 473.9 cm (6.4 cm = about 15 ms of sampling error) |
| Unwind | back to the exact present position (573.9) |
| Cost of one rewind+unwind of the 3 pawns | 0.216 ms (mean of 30, from Python; call overhead 0.006 ms) |

- The component captures and restores the actor transform, which is what Mover moves, so the "only physics bodies" stop-and-ask does
  not apply. `MaxRewindTimeMS` default is 200, but the effective limit adds the interpolation buffer and the input buffer steps
  (`GetMaxRewindDuration`), so 250 passed.
- Not measured: rewind under 60 ms / 15 ms / 2 % (the clamp uses the interpolation time of the requester), rewind of a pawn mid-hitstop.
- Verdict: pass. Implication for H10: use `RewindActors`/`UnwindActors`; the component must be added to the pawn Blueprint
  (shared Blueprint: owner OK needed, §6); no own ring from `FGSTN_PoseHistory` needed for the transform.
