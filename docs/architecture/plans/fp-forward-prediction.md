# FP implementation plan: forward prediction of the pair partner (2026-11-06)

Plan only, no code. Source: [combat-m7-piggyback.md](../combat-m7-piggyback.md) §2 spike 4 and §6.4, combat.md §12b (*Open*) and §12.7
(per-entity LOD), `docs/notes/m7-spike-report.md` (S1: a client rider's anchor vs the authority is 95-135 cm apart because the carrier is
interpolated 17-21 frames behind; verdict "server-given" for client riders). Ground rules: [resto-m3-m4b-m5-t.md](resto-m3-m4b-m5-t.md) §0.
Owner order: after M8, before M10 (sequenza D3).

## 1. Goal and the rule that can close it early

When two players are linked (rider + carrier, grabber + grabbed, pair moves), each client shows its partner from the past (interpolated).
Forward prediction makes the **partner only** a predicted entity on that client for the life of the link, so the pair moves together on
both screens. It costs CPU on every resimulation (the partner is resimulated too) and needs input guesses for the partner.

**Stop rule (G0)**: if spike 4 shows a remote-to-remote partner delay median ≤ 150 ms with presentation hiding on, and the owner, after
the video, says the rider looks fine, FP ends at G0 (no code) and the rider stays server-given (sequenza, decision D8).

## 2. Gates

| Gate | Content | Machine / model | Cost | Needs |
|---|---|---|---|---|
| **G0** | Spike 4 measured on today's feature (carrier throws rider, rider boost jump, remote-remote and remote-host, 60/15 ms and 2 %), side-by-side video for the owner; spike report row | desktop, Sonnet | $4-7 | M7 merged |
| **G1** | Fork spike: one simulated proxy switched to the NP `ForwardPredict` LOD at runtime and back, with the partner's last input repeated as the guess; measure resim cost per frame on the low-end budget (`t.MaxFPS`, laptop) | laptop, Opus | $6-10 | G0 = go |
| **G2** | Generic link seam in `GSTNLink`: a link type flag `bForwardPredictPartner` (data, default off) promotes the partner on each linked client at link start and demotes it at link end or on break; promotion is presentation-safe (no pop: blend over 3 frames) | laptop, Opus | $10-18 | G1 |
| **G3** | Piggyback integration: rider anchor built from the forward-predicted carrier instead of the server-given position; re-run S1 (anchor agreement) and S4; mounted hits keep server rewind to the rider's view time | desktop, Opus | $6-10 | G2 |
| **G4** | Sweep: SUITE_LINK + piggyback suites at 0/2 %, listen and dedicated; host frame cost with 4 pairs on the low-end budget; docs (combat.md §12b closed) | laptop, Sonnet | $4-6 | G3 |

**Total**: $4-7 if G0 stops, $30-51 otherwise.

## 3. Blueprint and editor

Nothing in Blueprint: it is a sim/network seam. The only designer-facing part is the flag on the link type data asset and two CVars
(`gstn.Link.ForwardPredict.Enable`, `gstn.Link.ForwardPredict.BlendFrames`).

## 4. Stop and ask

- G1: the fork cannot switch LOD at runtime without a reconcile storm (> 2 extra reconciles per switch) → stop, propose the fallback
  (partner extrapolated from the last authoritative state in presentation only).
- G1: resim cost of one forward-predicted partner > 0.5 ms on the low-end budget → stop and report.
- G2 touches `GSTNLink` and the NP fork: never in parallel with M10 G1-G3.

## 5. Risks

Mispredicted partner input (the partner presses something new): one correction on the linked client, the same cost as a hit.
Two links on one pawn (rider on a carrier who grabs): only the direct partner is promoted (no chains, M7 §12).
