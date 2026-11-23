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

## 4. Gates (option C, revised 2026-11-08 after the owner's answers and G0/G0b)

Rule from decision 3 ("precautions now"): G1 is built so that G3-G5 extend it and never redo it. The predicted partner reaction runs on
the same per-link window, data and blend that G4 promotes to NP forward prediction. A visible snap to the current animation frame on a
misprediction is accepted (fighting-game rollback style), so no extra smoothing work is planned beyond the existing presentation blend.

| Gate | Scope | Acceptance (headless unless noted) | Machine | Model | Cost |
|---|---|---|---|---|---|
| **G0** done | Baseline + buffer sweep (G0 results); buffer set to 50 ms (decision 1) | done, 6fdbda3 | laptop | Sonnet | ~$7 |
| **G0b** done | Hop breakdown of the boost delay (section below) | done | laptop | Sonnet | ~$2 |
| **G1** | `GSTNLink`: per-link "partner window" (pair move or mount transition: start frame, authored length, partner timeline id) shared by G1 and G4; on the initiator's client the partner's authored reaction plays at once from that window (presentation), snap to the authoritative frame on refusal or misprediction; cvar `gstn.Link.PredictPartnerPresentation` | `scenario_link_pair_move_delay`: initiator sees the partner react <= 150 ms after its own action; `scenario_link_pair_move_refused` (snap allowed, no stuck state); SUITE_LINK, SUITE_DEFAULT 0/2 % unchanged | laptop | Sonnet (Opus for the window design) | $6-10 |
| **G2** | Review gate (rule 10): desktop video, cloud review PASS/FAIL; owner sees PASS only. Decides if G3-G5 still run for the partner's own screen | video PASS + owner yes | desktop + cloud | Sonnet | $2-4 |
| **G3** | Fork (approved in advance, decision 3): per-entity LOD switch (proxy <-> forward-predicted at runtime, Mover's 3 global reads made per entity), attribute base sent to proxies (unconditional, wire format), last-input repeat with `PrepareRepeatedInput`. Inert unless a caller switches | fork test `GSTNCombat.NP.ForwardPredictSwitch` (<= 2 extra reconciles per switch); resim cost per promoted pawn <= 0.5 ms on the low-end budget | laptop | Opus | $8-12 |
| **G3b** | Client hit inbox for link partners (decision 4: partners can hurt each other, e.g. PvP grabs, a rocket hit, hitgrabs): a hit from or on the forward-predicted partner resolves in the owning client's victim inbox (M4 inbox drained on the owning client, combat-m4-hits.md §13); server stays authoritative | `scenario_link_partner_hit` (hit during a link: one correction at most, damage equal on every world), SUITE_DEFENCE/LINK 0/2 % | laptop | Opus | $8-14 |
| **G4** | `GSTNLink` seam: link type flag `bForwardPredictPartner`, promotes the partner only inside the G1 window (hysteresis); harness reconcile reason `fp` so `unexplained_ours` stays the gate | `scenario_link_fp_window` (promotions = windows, 0 unexplained); SUITE_LINK + piggyback 0/2 %, listen and dedicated | laptop | Opus | $10-16 |
| **G5** | Piggyback integration (rider anchor from the predicted carrier inside windows), S1/S4 rerun, host cost with 4 pairs and 40 awake minions on the low-end budget, video review, docs | partner screen: gain over G0b recorded; video PASS | desktop | Opus/Sonnet | $6-10 |

**Total left**: G1-G2 $8-14; G3-G5 with G3b $32-52; all $40-66 (G0/G0b spent ~$9). Every gate runs before M10 (decision 5).

**Code touched**: `GSTNLink` (G1, G4); fork NP/Mover (G3, listed in PATCHES.md when it lands); **`GSTNCombatNP` in G3b only**: the
victim inbox drained on the owning client for link partners (the only reason, from decision 4); config: the 50 ms buffer already set.
Nothing in Blueprint.

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

## 6. Owner decisions (answered 2026-11-07)

1. **Buffer**: yes. `FixedTickInterpolationBufferedMS` = 50 ms, set in `Config/DefaultNetworkPrediction.ini` (where the NP settings live;
   not DefaultEngine.ini).
2. **Gate**: yes. 150 ms on the initiator's screen; the partner's own screen at about one round trip.
3. **Fork patches G3-G5**: approved in advance, with "precautions now": G1 must not be redone by G3-G5; a snap to the current animation
   frame on misprediction is fine.
4. **Hit inbox prerequisite**: NOT waived. Partners can hurt each other during a link (PvP grabs, a rocket launcher, hitgrabs): G3b.
5. **Order**: yes, all of FP before M10.

## 7. PERF-DEBT expected from this plan

- A: the initiator's client evaluates the partner's reaction animation twice for one move (presentation + the later authoritative
  timeline); cheap (one montage), accept unless profiling shows it.
- A: the 50 ms buffer: stalls at 2 % 3-9 per 5 s walk vs 3-6 at 100 ms on the laptop rig (noisy); recheck on the desktop host.
- G3b: the owning client drains the victim inbox for link partners: one more resolution pass per partner hit on that client.
- B: ~0.03-0.04 ms per promoted pawn per resimulated frame x ~14 frames x ~5 rollbacks/s during windows; per-entity LOD lookups in
  Mover's hot path (3 reads per tick per pawn) replace one global read.
- B: harness reconcile classification adds a log parse per reconcile line (test time only).

## G0 results (laptop, 2026-11-08): buffer sweep done with a working override

Dedicated + 2 clients, 60/15 ms, -nullrhi, one run per cell: `scenario_throw_rider` + `scenario_boost_jump` (client_rides_client) + a walk
probe (median distance between the displayed proxy and the server pawn while walking at ~540 cm/s; stalls = proxy still while the server
pawn moved; jumps = step > 3x median). Buffer set with the inert fork knob `np.FixedTickInterpolationBufferedMSOverride` (PATCHES.md §4),
applied on all 15 world inits of every run (log line). Raw files: `Saved/FPG0/sweep2.txt`, `Saved/FPG0/out/` (not versioned).
An earlier sweep (2026-11-08 night) never applied the value (`-ini` struct override and `set` both ineffective) and is discarded.

| Buffer ms | Loss | Boost: carrier screen after rider screen | Boost from tap (rider / carrier) | Throw: rider vs carrier screen | Throw from tap (carrier / rider) | Stalls | Jumps | Proxy offset | Scenarios |
|---|---|---|---|---|---|---|---|---|---|
| 100 | 0 % | 319 ms | 137 / 456 | -56 | 604 / 549 | 3 | 0 | 56.0 cm | PASS |
| 100 | 2 % | 314 ms | 154 / 468 | -55 | 614 / 560 | 6 | 0 | 46.9 cm | PASS |
| 66 | 0 % | 335 ms | 136 / 471 | -54 | 620 / 566 | 2 | 0 | 44.5 cm | PASS |
| 66 | 2 % | 366 ms | 137 / 503 | -1 | 593 / 592 | 3 | 0 | 44.6 cm | PASS |
| 50 | 0 % | 313 ms | 136 / 449 | -53 | 593 / 540 | 2 | 0 | 41.1 cm | PASS |
| 50 | 2 % | 309 ms | 135 / 444 | -58 | 615 / 557 | 9 | 0 | 42.3 cm | PASS |
| 33 | 0 % | 259 ms | 141 / 399 | -1 | 589 / 588 | 13 | 0 | 39.1 cm | PASS |
| 33 | 2 % | 332 ms | 136 / 468 | -1 | 587 / 586 | 12 | 1 | 38.0 cm | PASS |
| 400 (control) | 0 % | 364 ms | 135 / 499 | -56 | 619 / 563 | 1 | 0 | 48.5 cm | PASS |

- **The buffer is a small term.** From 100 to 33 ms the proxy offset falls by only ~10-15 cm (~20-30 ms at walking speed, against 67 ms
  removed), and the 400 ms control is no farther behind than 100 ms. The interpolation either catches up past the desired buffer or the
  Mover sim proxy shows something closer to the newest state. The boost delay does not follow the buffer: 259-366 ms in every cell, which
  is run noise around ~320 ms.
- **Smoothness**: 33 ms roughly triples the stalls (12-13 vs 3-6) and shows the only jump at 2 %. 50 ms stays near the baseline at 0 %
  (2), with one noisy 2 % cell (9).
- **Recommended buffer: 50 ms** if decision 1 is taken (a ~10 cm / ~20 ms gain on every proxy at no visible smoothness cost on this rig).
  It does not change the pair-move delay.
- **Verdict: the lightweight path (G1-G2) cannot reach 150 ms on the partner's screen. Expected remaining gap: ~150-170 ms** (about 310-330
  ms measured minus at most ~20 ms from the buffer). The predicted-partner presentation of G1 still helps the initiator's screen only.
- **Throw (for G1)**: both screens show the release together (server-given) but 540-620 ms after the tap at every buffer. That is likely the
  throw timeline's wind-up before its release event, design time that FP cannot remove. Check the timeline frames first in G1.
- **Next**: before building G3-G5, split the ~320 ms boost delay into its hops (rider input to server, server to link state, link state
  replicated to the carrier, shown). The buffer is not the big term, so forward prediction's gain must be re-estimated (~$2-3, laptop).

## G0b hop breakdown (laptop, 2026-11-08, buffer 50 ms, dedicated + 2, harness lag 60 ms / jitter 15 ms, 3 runs each)

Times are ms after the rider's tap, from the first frame each world shows the event (`Saved/FPG0/hops.txt`). Rider = client 1, carrier = client 2.

| Boost jump | 0 % (3 runs) | 2 % (3 runs) |
|---|---|---|
| rider's screen: rider leaves (predicted) | 132-137 | 133-134 |
| server: carrier braces (pair move starts) | 237-240 | 237-239 |
| server: rider leaves | 315-318 | 351-352 |
| rider's screen: carrier braces (proxy) | 350-355 | 306-353 |
| carrier's screen: rider leaves | 385-433 | 440-479 |
| **hop 1**: rider's screen to server, same event (client lead: input travel + server input buffer) | ~180 | ~218 |
| **hop 2**: server to carrier's screen (replication + 50 ms interpolation) | 70-115 | 90-128 |

Throw (carrier initiates, server-given): server 470-484, rider's screen 559-570, carrier's screen 562-622 ms after the tap. The ~470 ms
before the server releases is the carrier's input path plus the throw timeline's wind-up: design time first (G1 checks the frames).

**Re-estimated gains.**
- G1 (initiator presentation): the rider sees the carrier brace at ~350 ms today, ~215 ms after its own action. With the partner window
  it is shown with the action (~0 ms gap). **The 150 ms initiator target is reached by G1 alone.**
- G3-G5 (NP forward prediction of the partner): the partner's screen can only change once the initiator's input has reached it through the
  server. FP removes hop 2's interpolation and alignment (~30-60 ms), not hop 1 (~180-220 ms, the client lead). Expected partner gap:
  ~250-300 ms -> **~200-260 ms**, about "one round trip" (decision 2). It never reaches 150 ms on the partner's screen. The bigger lever is
  hop 1 (the server input buffer, `FixedTickInputSendCount` / desired buffered input), which is not FP and is not measured yet.

## G1 results (laptop, 2026-11-08): initiator target met

Built on `claude/fp-g1-laptop`: `FGSTN_LinkPartnerWindow` (`GSTNLinkNP/Sim`, pure builder from the initiator's fragment, shared with G4) and the
predicted partner presentation in `UGSTN_LinkPresentationComponent` (early TG_PrePhysics tick: the fork's montage finalize runs at
OnWorldPreActorTick, before the mesh animates; cvar `gstn.Link.PredictPartnerPresentation`, `GetPredictedPartnerMove`,
`OnPredictedPartnerMove`). No change to `GSTNCombatNP` or the fork. Dedicated + 2, 60/15, buffer 50 ms; ms after the initiator's tap:

| Move (initiator) | Loss | Partner reaction shown on the initiator's screen (G1) | Initiator's own event | Before G1: partner shown on the initiator's screen | Partner's own screen |
|---|---|---|---|---|---|
| Boost (rider) | 0 % | 68 | rider leaves 141 | carrier braces 333 | 413 |
| Boost (rider) | 2 % | 38 | rider leaves 143 | carrier braces 377 | 414 |
| Throw (carrier) | 0 % | 246 | (server release 463) | rider released 553 | 554 |
| Throw (carrier) | 2 % | 239 | (server release 499) | rider released 590 | 652 |

- **Initiator target met**: the partner's reaction now starts on the initiator's screen with its own action (boost: 73-105 ms before
  the rider leaves, as the move is authored; before G1 it came ~190-235 ms after). Throw: the rider's reaction shows ~310-350 ms earlier on
  the carrier's screen. `scenario_link_pair_move_delay` and `_throw` PASS at 0 % and 2 %. The partner's own screen is unchanged (G3-G5 scope).
- **Measured on the presentation API**, not on pixels: the test pair moves may carry no montage (only loaded ones are led). The G2 video
  review (rule 10) must confirm the visual, including that the late synced montage does not replay the move.
- Not covered: `scenario_link_pair_move_refused` (no cheap refusal path without editing content); a refusal decided on the partner's side
  (its requirements fail on the server) is invisible to the initiator's fragment, so the predicted reaction plays to its end there.

## G0c results (laptop, 2026-11-08): server input buffer vs hop 1

`scenario_boost_jump`, dedicated + 2, 60/15 (NetEmulation PktLag 60 on every driver = ~60 ms each way), buffer 50 ms, one run per cell.
Server input buffer depth set with the inert knob `np.FixedTickDesiredBufferedInputCountOverride` (PATCHES.md §4; project default 4 frames
= 64 ms), applied on every world (log). Starved = server ticks that held the last input (`np.Spike.LogInputSend`, whole run incl. setup).
Raw: `Saved/FPG0c/` (not versioned).

| Input buffer (frames) | Loss | Rider screen | Server: rider leaves | Carrier screen | Hop 1 (server - rider) | Partner gap (carrier - rider) | Starved ticks | Boost |
|---|---|---|---|---|---|---|---|---|
| 4 (default) | 0 % | 150 | 342 | 428 | 192 | 278 | 7 | PASS |
| 4 | 2 % | 133 | 313 | 385 | 180 | 252 | 6 | PASS |
| 2 | 0 % | 146 | 302 | 421 | 156 | 275 | 15 | PASS |
| 2 | 2 % | 130 | 318 | 445 | 188 | 315 | 14 | PASS |
| 1 | 0 % | 132 | 294 | 377 | 162 | 245 | 18 | PASS |
| 1 | 2 % | 138 | 327 | 454 | 189 | 316 | 18 | PASS |
| 0 | 0 % | 131 | 280 | 363 | 149 | 232 | 25 | PASS |
| 0 | 2 % | 133 | 287 | 398 | 154 | 265 | 24 | PASS |

- **Hop 1 split (estimate, default 4)**: ~60 ms one-way lag + ~7 ms mean jitter + 64 ms server input buffer + ~16-32 ms send and frame
  alignment = ~150-165 ms, against ~180-190 ms measured; the rest (~20-30 ms) is the client lead's slack (the offset adapts in steps).
- **What the buffer buys**: 4 -> 0 frames cuts hop 1 by ~25-40 ms (inside run noise at 2 %), and the starved ticks grow 3-4x (7 -> 25). Each starved
  tick repeats the last input on the server, a likely correction on the player's own client. One run per cell: the 1- and 2-frame cells are noise.
- **Cheapest setting that cuts hop 1 without more faults at 2 %**: none clearly. Depth 2 is the only candidate (starvation doubles, the gain is not
  visible above the noise). No input faults in any cell beyond the 2 start-up gap lines.
- **Partner-screen estimate** (boost, partner gap after the initiator's own action): today ~250-280 ms; with input depth 2 ~240-270 ms; with
  G3-G5 ~200-240 ms; with both ~190-230 ms. The floor is about two one-way trips plus the frames (~170-190 ms): never 150 ms on the partner's screen.
