# FP plan: forward prediction for pair partners, re-based after M8 (2026-11-07)

Plan only, no code. Supersedes the gate table of [fp-forward-prediction.md](fp-forward-prediction.md) (2026-11-06), which stays as history.
Sources: [sequenza-2026-11-06.md](sequenza-2026-11-06.md) rows 15-16 and D8; `Plugins/GASNPP/PATCHES.md` §3-§4; combat.md §12.7 (spike S-FP
and the policy), §12b (pair moves, S4); the M8 laptop status block in `docs/notes/handovers/m8-minion.md`; `docs/notes/perf-debt.md` and its
review `docs/notes/reviews/perf-debt-review-2026-11-07.md`. Base: `claude/m8-merge-feature` b4149a8 (feature + M8 G0-G12a).

## 1. What forward prediction means here

- **Today** every remote entity on a client is an NP *simulated proxy*, **interpolated**: shown about `FixedTickInterpolationBufferedMS`
  (100 ms) behind the newest server frame. The local player is *autonomous*: predicted, corrected by rollback + resimulation when the
  server state differs (one reconcile ~ RTT/16 ms frames resimulated for every predicted entity).
- **Forward prediction (FP)** turns one chosen proxy into a predicted entity on one client: it runs in the local frame, with a *guessed*
  input (its last one repeated), and is corrected like the local player. It removes the interpolation delay for that entity, at the
  price of a rollback every time its real input changes (~one RTT late) and of resimulating it in every rollback.
- **Scope of FP in this project** (owner order, sequenza D3/D8): only the **partner of a link** (rider/carrier, grabber/grabbed, pair
  moves), only on the two partners' clients. Never minions, never distant players (combat.md §12.7 policy).
- **Problem it must solve**: S4 measured the partner's reaction to a pair move at ~280 ms (throw) / ~300 ms (boost jump) on its own screen,
  over the 150 ms gate. Most of it is one client-server-client trip plus the 100 ms interpolation buffer.
- **What it does not touch**:
  - **Minion store** (M8): authority only; clients show minions through their own interpolator (G7), not NP proxies. FP changes nothing
    there, and client resimulation never runs the minion batch (`OnPostFixedTick` subscribers live on the authority only).
  - **Lag compensation / X4**: server-side rewind is unchanged. A forward-predicted partner is shown *at* the client frame, so mounted hits
    keep rewinding to the rider's view time (M7 G6). X4 (rewind iterates every minion) stays an independent perf item.
  - **Rollback counters**: FP adds *expected* reconciles. The harness (`report_reconciles`) counts `unexplained_ours`; FP rollbacks must be
    classified (new reason) or every link/default gate goes red.

## 2. Current state

- Fork: spike S-FP code is in the fork, inert (`np.Spike.*` = 0). LOD is **global**, not per entity (Mover reads it in 3 places). Known fork
  bug: attribute base values are not sent to simulated proxies (wire-format change needed before any FP).
- S-FP numbers (dedicated + 2, 60/15, 2 %): remote dash seen 40-50 ms earlier, almost all of it the interpolation buffer removed; ~5
  rollbacks/s per active remote; ~0.03-0.04 ms per predicted entity per resimulated frame; 2-3 m single-frame jumps (no proxy smoothing).
- Policy (combat.md §12.7, *Decision*): lower the interpolation buffer first; FP only after a client hit inbox and proxy smoothing exist.
- Links (M7): pair moves are **authored timelines** (`UGSTN_PairMoveDefinition`, forced partner timeline): the partner's reaction is known
  in advance once the move starts. Partners never hit each other.
- SUITE_DEFAULT 50/50 and the M8 gate are green on b4149a8; SUITE_LINK/PARTS/DEFENCE not rerun on the merge (desktop G12b sweep).

## 3. Design options

**A. No NP forward prediction: smaller buffer + predicted partner presentation.** Lower `FixedTickInterpolationBufferedMS` to the smallest
value that stays smooth under 15 ms jitter (measured sweep). On the client that starts a pair move, the link presentation plays the
partner's authored reaction at once on the partner's mesh (presentation only, like the hit presentation: cancelled and blended back if the
server refuses). On the partner's own client nothing is predicted; its reaction still arrives as a correction, earlier by the buffer cut.
Cost: an ini value + presentation code in `GSTNLink`. No fork change, no `GSTNCombatNP` change, zero resim cost. Limit: the partner's own
screen keeps one trip of delay (~RTT + buffer).

**B. Per-link NP forward prediction (the 2026-11-06 plan).** Per-entity LOD switch in the fork, attribute base on the wire, proxy
correction smoothing, a link seam that promotes the partner for the life of the link. Removes the buffer for the partner on both
clients. Cost: fork patches (wire format change), ~5 extra rollbacks/s per linked pair, each resimulating 2 predicted pawns; reconcile
storms at switch time are the main risk. $30-50.

**C. A first, then B only inside pair-move windows if A misses the gate (recommended).** A's numbers decide. If the video still fails,
B is built narrower: the partner is promoted only while a pair move or mount transition is active (a few hundred ms, hysteresis), so the
rollback cost exists only in those windows, not for the whole ride.

**Recommendation: C.** It follows the existing policy, costs ~$10-17 if A is enough, and never pays double: A's buffer cut and
presentation stay useful under B (B removes only what A could not).

## 4. Gates (option C)

| Gate | Scope | Acceptance (headless unless noted) | Machine | Model | Cost |
|---|---|---|---|---|---|
| **G0** | Baseline on b4149a8: S4 rerun (throw, boost; remote-remote, remote-host); interpolation buffer sweep 100/66/50/33 ms at 60/15, 0 and 2 %: proxy snap count, S4 delay, reconciles. No code. | numbers table in `docs/notes/fp-g0-report.md`; `scenario_link_pair_move_delay` (new harness scenario wrapping S4) | laptop (numbers), desktop (video) | Sonnet | $4-7 |
| **G1** | Option A: chosen buffer value (project ini, owner OK); `GSTNLink` presentation: initiator plays the partner's authored reaction early, cancel + blend on refusal; cvar `gstn.Link.PredictPartnerPresentation` (default on after the gate) | `scenario_link_pair_move_delay` <= 150 ms on the initiator, report on the partner; `scenario_link_pair_move_refused` (no pop > 15 cm on refusal); SUITE_LINK, SUITE_DEFAULT 0/2 % unchanged | laptop | Sonnet (Opus for the refusal blend if needed) | $6-10 |
| **G2** | Review gate (rule 10): desktop records the pair moves in-game, cloud review thread PASS/FAIL against the base video; PASS → owner. Owner PASS closes FP (docs: combat.md §12b, §12.7). | video PASS + owner yes | desktop + cloud | Sonnet | $2-4 |
| **G3** | Only if G2 fails. Fork: per-entity LOD switch (proxy ↔ forward-predicted at runtime, Mover's 3 global reads made per entity), attribute base sent to proxies (unconditional, wire format), last-input repeat with `PrepareRepeatedInput`. Inert unless a caller switches. | new fork test `GSTNCombat.NP.ForwardPredictSwitch` (<= 2 extra reconciles per switch); resim cost per promoted pawn <= 0.5 ms on the low-end budget (`t.MaxFPS`, laptop) | laptop | Opus | $8-12 |
| **G4** | Only if G2 fails. `GSTNLink` seam: link type flag `bForwardPredictPartner` + promote only inside pair-move / mount-transition windows with hysteresis; proxy correction smoothing (3-frame blend); harness: new reconcile reason `fp` so `unexplained_ours` stays the gate | `scenario_link_fp_window` (promotions = windows, 0 unexplained); SUITE_LINK + piggyback suites 0/2 %, listen and dedicated | laptop | Opus | $10-16 |
| **G5** | Only if G2 fails. Piggyback integration (rider anchor from the predicted carrier inside windows), S1/S4 rerun, host cost with 4 pairs on the low-end budget, video review as G2, docs | `scenario_link_pair_move_delay` <= 150 ms on both partners; video PASS | desktop | Opus/Sonnet | $6-10 |

**Total**: $12-21 if G2 passes (G0-G2); $36-59 with G3-G5. Sequenza's estimate ($30-50) holds only for the B path.

**`GSTNCombatNP` changes: none in any gate.** Fork changes only in G3 (listed above, each added to PATCHES.md §4 when it lands).
`GSTNLink` changes in G1 and G4. The project-wide ini value in G1 needs the owner's OK (plan §6 rule). Nothing in Blueprint.

## 5. Risks

- Smaller buffer under jitter: proxies extrapolate or stutter more (all NP proxies, not only partners). G0 picks the value from measured
  snaps; if 66 ms is the floor, A alone may land near 230 ms and G3-G5 become likely.
- Presentation-only partner reaction (A) can disagree with the server (refused move): one visible blend back; measured in G1.
- B: reconcile storm at the LOD switch (stop rule: > 2 extra reconciles per switch → stop and report); the wire-format change of the
  attribute base breaks mixed builds (every client must update together).
- Two links on one pawn (rider on a carrier who grabs): only the direct partner is promoted (M7 §12), never chains.
- `GSTNLink` is shared with M10 G1-G3: G1 and G4 never run in parallel with M10.
- Toaster target: B multiplies resim work by 2 pawns per window; with M8 hosts already spending the frame on minions, G5 must measure the
  host with minions on (40 awake) and 4 pairs.

## 6. Owner decisions

1. **Buffer change**: may G1 lower the project-wide `FixedTickInterpolationBufferedMS` (affects every NP proxy, not minions) to the G0 value?
2. **Gate**: keep 150 ms on the initiator's screen as the target for A, and accept the partner's own screen at "one trip" (~180-230 ms)?
3. **If A fails**: approve the B-path fork patches (per-entity LOD, attribute base wire format) and the ~$25-38 extra?
4. **Hit inbox prerequisite** (combat.md §12.7 policy): waive it for link partners, who never hit each other? (Recommended: yes.)
5. **Order**: FP G0-G2 before M10, and G3-G5 (if needed) also before M10 since both touch `GSTNLink`?

## 7. PERF-DEBT expected from this plan

- A: the initiator's client evaluates the partner's reaction animation twice for one move (presentation + the later authoritative
  timeline); cheap (one montage), accept unless profiling shows it.
- A: a smaller buffer means more proxy extrapolation work and more visible micro-corrections on every proxy under jitter.
- B: ~0.03-0.04 ms per promoted pawn per resimulated frame x ~14 frames x ~5 rollbacks/s during windows; per-entity LOD lookups in
  Mover's hot path (3 reads per tick per pawn) replace one global read.
- B: harness reconcile classification adds a log parse per reconcile line (test time only).

## G0 results (laptop, 2026-11-08): baseline valid, buffer sweep NOT valid (override never applied)

Dedicated + 2 clients, 60/15 ms, -nullrhi, `scenario_throw_rider` + `scenario_boost_jump` (client_rides_client) + a walk probe (displayed
proxy vs server pawn). Raw table and tooling: `Saved/FPG0/results.md`, `Saved/FPG0/out/` (not versioned).

| Run | Boost: carrier screen after rider screen | Boost from tap (rider / carrier) | Throw: rider screen vs carrier screen | Throw from tap | Proxy offset (walk) |
|---|---|---|---|---|---|
| baseline 100 ms, 0 % | 293 ms | 139 / 431 ms | 0 ms | 596 ms | 50.5 cm |
| baseline 100 ms, 2 % | 293 ms | 137 / 430 ms | -58 ms | 612 / 554 ms | - |
| "66 / 50 / 33 ms", 0 and 2 % | 290-356 ms | 133-141 / 426-496 ms | -58..0 ms | 549-631 ms | - |
| control "400 ms", two override methods | 304-351 ms | 135-137 / 441-486 ms | -56 ms | 563-619 ms | 45.1-56.5 cm |

- **The sweep did not change the buffer.** A 400 ms buffer must put a walking proxy ~2 m behind the server; it stayed at 45-56 cm, like
  100 ms. Both methods failed: `-ini:NetworkPrediction:[...]:Settings=(...)` (the commas inside the struct split the override) and the
  `set NetworkPredictionSettingsObject Settings (...)` exec command before PIE. Python cannot write the struct (fields not exposed).
  So every row is the 100 ms baseline measured again: the spread (290-356 ms) is run-to-run noise.
- **Baseline (valid)**: the partner sees a boost ~290-355 ms after the initiator (S4 confirmed). A throw shows on both screens at the same
  time (server-given) but ~550-630 ms after the tap, which is far above one trip plus 100 ms: the throw's own timeline (wind-up before the
  release event) is probably most of it. That is design time, not network time, so FP cannot remove it. Check it in G1 before tuning anything.
- **Verdict**: not decided. G0 cannot yet say whether a smaller buffer reaches 150 ms. The baseline says a buffer cut alone is unlikely
  to be enough: 150 ms would need ~150 ms less than the ~300 ms measured, and the whole buffer is only 100 ms.
- **Recommended value**: none yet. Close G0 with a working override first.
- **To close G0 (~$2-3, laptop)**: an inert fork cvar `np.FixedTickInterpolationBufferedMSOverride` (default -1, read in
  `UNetworkPredictionWorldManager::SyncNetworkPredictionSettings` and logged), added to PATCHES.md. That is one fork line and needs your
  OK (no `GSTNCombatNP`). Then rerun the 8-cell sweep with the 400 ms control and the proxy offset as the check that it applied.
