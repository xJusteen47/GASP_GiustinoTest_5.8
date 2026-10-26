# Spike S-FP report: forward prediction of selected simulated proxies (2026-10-19)

Plan: docs/notes/spike-sfp-plan.md. Setup: PIE dedicated server + 2 clients (one process), 60/15/2 emulation,
observer = client 2 watching client 1's pawn (BP_GASNPP_TestCharacter: ASC + Mover + combat model), scenario_abilities +
scenario_combat_spike (4 dashes, a buff, hitstop window). Raw data: scratchpad out_R*.txt / raw_R*.json.

## Results
| Metric (scenario window ~16 s) | R0 Interpolated | FP (P1, attribute base fixed) | FP + predicted activation (P2) |
|---|---|---|---|
| State-mismatch reconciles | 2-4 | ~83, all on the observer | ~82 |
| Observer re-simulation | ~0 | ~75 rollbacks, ~1100 frames, ~95 ms | same |
| Re-sim cost per frame | ~0.05-0.06 ms (1 entity) | ~0.09 ms (2 entities) | same |
| Max observer mesh jump in one frame | 27-47 cm | 220-263 cm at each dash | same |
| Dash seen on observer after server | 108-132 ms | 73-82 ms | 73-79 ms |

- Before the attribute fix FP reconciled ~10/s even idle: the server never sends attribute **base** values to simulated
  proxies (EffectsDataTypes.cpp FAttributeSyncData::NetSerialize), so every compare failed. Fixing it halved the cost.
- What remains is inherent to predicting a remote player: the observer predicts it with its last input, so every new
  remote input (dash press, buff, stick change) arrives ~RTT late and costs one rollback of ~14 frames of every predicted
  entity. Remaining diffs: attributes/tags from the remote ability's effects (dash cost, cooldown, buff heal), combat
  input history, small Mover drift.
- Predicted activation on the proxy (P2) gives no measurable gain: the activating input arrives late either way and is
  injected at its true frame by the correction, so the ability appears at the same moment with or without P2.
- The gain (~40-50 ms) is essentially the removed interpolation delay (FixedTickInterpolationBufferedMS = 100).
- Every unpredicted burst action is shown as a 2-3 m single-frame jump: correction smoothing does not reach the
  forward-predicted proxy's mesh.

## Feasibility
Technically feasible with small fork patches, but not worth enabling as it stands:
- cost per predicted entity is cheap per frame (~0.03-0.04 ms per re-simulated frame), but rollbacks multiply: each
  predicted remote player adds ~5 rollbacks/s while active, and each rollback re-simulates every predicted entity
  (cost grows roughly with entities x input events);
- the visual result is worse than interpolation until correction smoothing works for proxies;
- the gain is small and most of it can be had by lowering the interpolation buffer.

## Fork patches a production version needs (estimates)
| Patch | Size | Notes |
|---|---|---|
| Send attribute base values to simulated proxies (always, a few bits) | S | Spike cvar np.Spike.SPSendAttributeBase changes the wire format: must be unconditional, not a client-side cvar |
| Prepare repeated input on proxies (P1) | S | done in the spike (np.Spike.SPPrepareRepeatedInput); without it one press acts every predicted frame |
| Per-entity LOD: persist a per-proxy override through InitForNetworkRole/UpdateConfigWithDefault; Mover reads the instance config instead of the global setting (3 places) | S-M | today the LOD is global |
| Correction smoothing for forward-predicted proxies (visual offset decays over frames) | M | needs investigation: FixedSmoothing is subscribed, yet the mesh jumps |
| Client-side hit inbox draining for predicted attacker -> local victim (combat 12.4) | M-L | the main reason to predict attackers at all; not built yet (M4) |
| Predicted activation on proxies (P2) | — | drop: no gain measured |

## Policy proposal
- Default: everything Interpolated. First lower `FixedTickInterpolationBufferedMS` (100 ms) to the smallest value that
  stays smooth under the target jitter: it buys most of the 40-50 ms without any rollback cost.
- Forward-predict only when the client-side hit inbox exists and correction smoothing works, and then only for the 1-2
  entities fighting the local player at close range (current aggressor or locked target, boss), switched per entity
  with hysteresis; never minions, never distant players.
- Re-evaluate with the M4 hit pipeline: the decisive number is how many hits on the local player stop being corrections.

## Spike code (uncommitted, inert by default)
NP: Services/NetworkPredictionService_Ticking.inl (P1), NetworkPredictionWorldManager.cpp/.h (P3 resim cost log).
ASC: NpAbilitySystemComponent.cpp/.h, NpGameplayAbility.cpp (P2), DataTypes/EffectsDataTypes.cpp (attribute base cvar).
Cvars: np.Spike.SPPrepareRepeatedInput, np.Spike.SPPredictActivation, np.Spike.LogResimCost, np.Spike.SPSendAttributeBase.
