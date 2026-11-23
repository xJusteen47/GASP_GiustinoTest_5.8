# Spike S-FP: forward prediction of selected simulated proxies (plan, 2026-10-19)

Brief: docs/architecture/combat.md §12.7 "Open — spike S-FP". Goal: measure whether other players / bosses can be
forward-predicted on a client (ASC + Mover + combat model), at what cost, and which fork patches that needs.

## What the code already does (mapped 2026-10-19)
- A simulated proxy (SP) whose NP config is `ForwardPredict` is subscribed to FixedRollback + FixedTick + FixedFinalize
  (+ FixedSmoothing) (NetworkPredictionWorldManager.h ~558-586). So the ASC, Mover and the combat sim already
  SimulationTick on it. With `Interpolated` it only gets FixedInterpolate (no simulation).
- LOD is effectively global: FNetworkPredictionDriver::GetConfig reads Settings.SimulatedProxyNetworkLOD; a per-proxy
  Configure() is overwritten by UpdateConfigWithDefault on role init. Mover also reads the global setting
  (MoverNetworkPredictionLiaison.cpp FinalizeFrame, SimpleSpringWalkingMode, SmoothWalkingMode). The project config is
  `Interpolated` (Config/DefaultNetworkPrediction.ini); the NP default is ForwardPredict.
- SP input: no input service, Passive driver. The last received InputCmd is copied to the head frame
  (Rollback.inl "Copy received InputCmd to head") and then repeated verbatim by Ticking.inl Tick_Internal
  (`OutputFrameData.InputCmd = InputFrameData.InputCmd` for SP). Edge-triggered data ("just pressed") repeats too:
  wrong. PrepareRepeatedInput (driver hook) is only called by the server's remote input service.
- ASC on SP: ticking works, server-decided abilities/effects arrive through RestoreFrame (ForceGiveAbility,
  ForceActivateAbility, ForceApplyEffect) and keep ticking (TickAbilities). Local initiation is blocked by role guards:
  NpAbilitySystemComponent TryActivateAbility (~2136), InternalTryActivateAbility (~2200), ApplyGameplayEffectSpecToSelf
  (~1313), NpGameplayAbility::ShouldActivateAbility (NpGameplayAbility.cpp ~270). GiveAbility also rejects SP (keep it).
- Combat sim (GSTN_CombatSimComponent) has no role checks; on SP it classifies inputs and requests actions through
  ASC TryActivateAbility from HandleAbilitySimulationPreTick (bound to ASC OnSimulationPreTick), which the SP guard
  rejects. No hit inbox exists yet (M4).

## Spike patches (all behind cvars, default OFF, so the tree stays inert when not testing)
P1 NP, Services/NetworkPredictionService_Ticking.inl Tick_Internal: for ROLE_SimulatedProxy, after copying the input
   into the output frame, call `FNetworkPredictionDriver<ModelDef>::PrepareRepeatedInput(OutputFrameData.InputCmd.Get())`
   when cvar `np.Spike.SPPrepareRepeatedInput` != 0. (The frame that carries a real received input is overwritten by the
   head copy / ApplyCorrection, so only repeated frames are prepared.) Declare the cvar with the NETSIM_DEVCVAR_SHIPCONST_INT
   pattern used in Services/NetworkPredictionService_Input.inl.
P2 ASC: `bool bInSimulationTick` member, set true for the duration of UNpAbilitySystemComponent::InternalSimulationTick
   (scope guard, also reset on early exits). Helper `bool CanPredictAsSimulatedProxy() const` = cvar
   `np.Spike.SPPredictActivation` != 0 && cached net role == ROLE_SimulatedProxy && bInSimulationTick. Relax the four
   guards above when it returns true (TryActivateAbility, InternalTryActivateAbility, ApplyGameplayEffectSpecToSelf,
   NpGameplayAbility::ShouldActivateAbility via the ASC from ActorInfo). Do NOT relax GiveAbility. Check nothing after
   those guards sends a server RPC for a non-authority LocalPredicted activation (report if it does).
P3 Instrumentation, NetworkPredictionWorldManager.cpp rollback block (ReconcileSimulationsPostNetworkUpdate_Internal,
   the `for (Frame = RollbackFrame; ...)` loop): when cvar `np.Spike.LogResimCost` != 0, accumulate wall time
   (FPlatformTime::Seconds) and frame count of re-simulations; every 5 s of real time log one line
   `LogNetworkPrediction: Display: SpikeResim: rollbacks=%d frames=%d totalMs=%.2f msPerFrame=%.3f` and reset.
No change to Mover, combat, project Source or Content. The global LOD is switched for a PIE session by setting the
NetworkPredictionSettingsObject CDO `Settings.SimulatedProxyNetworkLOD` from editor Python before Play (same technique as
bEnableFixedTickSmoothing; never SaveConfig), and restored after.

## Measurements (dedicated server + 2 clients, 60/15/2; observer = client 2 watching client 1's pawn as an SP)
Runs: R0 LOD Interpolated (baseline). R1 LOD ForwardPredict, P1 on, P2 off. R2 ForwardPredict, P1 on, P2 on.
Each run: np.Spike.LogResimCost 1 on client worlds; scenario_abilities((1,)) then scenario_combat_spike(1).
1. Reconciles: mark_log/report_reconciles; split per client world if the log allows (state the method).
2. Resim CPU: SpikeResim lines on the observer during the scenario window; compare R0 vs R1/R2 (extra cost of one
   predicted entity per re-simulated frame).
3. Visual pops: on client 2 sample the SP pawn's mesh world location every rendered frame (slate post-tick callback
   inside gasnpp_test-like helper written in a separate scratch module, not in gasnpp_test.py); report the max and 99th
   percentile per-frame displacement vs the median, and count frames with displacement > 3x median.
4. Startup frames lost: on server world and client 2, poll every frame whether the SP pawn's ASC has tag
   Test.State.Dashing; report (first time seen on client 2) - (first time on server) in ms per dash, for each run.
Pass/fail is not the point: report numbers per run.

## Decision inputs for the report
Feasibility, the patch list above with what else would be needed for production (per-entity LOD API in
FNetworkPredictionProxy + Mover reading the instance config; hit inbox draining on the client for predicted
attacker -> local victim), CPU budget per predicted entity, and a per-entity policy proposal.
