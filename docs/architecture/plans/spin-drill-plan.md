# Implementation plan — Spin and drill (`GSTNSpinDrill`)

Spec: [`../spin-drill.md`](../spin-drill.md). Plan only, no code. Same method as M4/M4b: one checkpoint = one commit, a short
agent session per checkpoint (cheapest model that fits), HANDOVER entry in `docs/notes/takeover-state.md`, deviations in
`docs/notes/spin-drill-doc-deltas.md`.

## 1. Ground rules

- Epic C++ coding standard; plugin README kept current at every checkpoint; native tags only.
- Simulation code follows gas-npp.md: deterministic, rollback-safe, state only in `FGSTN_DrillFragment`, no net-mode branches.
- Listen server first in every harness run (host + 2 clients, 60/15 ms, with and without 2 % loss).
- No edits to `GSTNCombat*`, `GSTNParts*`, `GSTNLevelObjects` or the project's wall modes outside the checkpoints that name them,
  and those only after the owner OK of SD.11 and agreement with the thread that is working there at the time.
- Placeholder animations only (owner rule: logic first). Animation set and skin override follow the pattern of the thread
  "Animare la wall slide nell'ABP".

## 2. Checkpoints

| # | Checkpoint | Touches core? | Needs |
|---|---|---|---|
| D0 | Spike: spin input in both senses on keyboard/gamepad in PIE, cancel-window route by binding requirement (no new code beyond test bindings); measure. Stop if the `Spin` condition misses > 5 % of deliberate rotations | No | SD.1 |
| D1 | Plugin skeleton `GSTNSpinDrill` (Core, NP, GAS, Presentation, Tests), README, native tags §10, `check_modules.py` clean, disabled in the .uproject until owner OK | No | — |
| D2 | Core data and math: tuning asset, target response, `UGSTN_DrillMath` (anchor/axis quantisation, wobble, launch vector), unit tests | No | — |
| D3 | `FGSTN_DrillFragment` + tick extension skeleton (phase machine on a fake snapshot), fragment serialisation tests | Feature id only (S5, one line in the central id file) | S5 OK |
| D4 | Ground spin + air spin (timelines, hop, low gravity inside the timeline, once per airtime, refresh rules), harness `spin_ground_hits_ring`, `spin_air_hop_low_gravity`, `air_spin_once_per_airtime` | No | SD.3 |
| D5 | Opposite-spin cancel (`UGSTN_GA_SpinCancel`, projected sense tags, bindings), harness `spin_opposite_cancel` | No | SD.2 |
| D6 | Core seams S1 (re-hit interval) and S2 (loop section) in `GSTNCombatCore` / timeline player, with their own unit tests and the existing 396+ tests green | **Yes** | SD.11 + after M4b P14 |
| D7 | Core seams S3 (attacker root override) and S4 (attacker hit notify), S6 (gravity contributor) if SD.3 wants it | **Yes** | SD.11 |
| D8 | Dive: plunge along gravity, ground lag via the movement track, non-drillable bounce; harness `drill_ground_lag`, `drill_nondrillable_bounce`, `drill_tilted_gravity` | No (uses D6–D7) | SD.4, SD.6 |
| D9 | Latch + bore on **static** targets: snapshot extension, latched movement mode, seat, multi-hit loop, end conditions, launch, detach pop; test prop in the test module; harness `drill_latch_static_prop_multi_hit`, `drill_release_early_pop`, `drill_launch_after_bore`, `drill_target_destroyed` | No | SD.5, SD.7, SD.10 |
| D10 | Moving targets (`bLatchMoves`): derived transform for hitbox (S3), exit placement, camera target; harness `drill_latch_moving_enemy`, `drill_hit_while_latched_detaches`; count corrections per latch | No | — |
| D11 | Presentation: procedural spin + wobble component, placeholder montages per phase, cues, skin override hook per the anim pattern | No | anim pattern thread |
| D12 | Bridges: `GSTNSpinDrillParts` (part under the tip, break ends bore) after M4b is done; `GSTNSpinDrillLevelObjects` (`AGSTN_DrillableProp`) | Bridge plugins only | SD.9 |
| D13 | Validation (data validators: drill timelines have a loop section, skin montages match length), docs pass, final listen-server run of all scenarios | No | — |

D1–D5 can start at once (they touch nothing owned by other threads). D6–D7 wait for M4b P14 on the laptop and for SD.11.
D8 onward depends on D6–D7.

## 3. Coordination

- **M4b thread (laptop, `GSTNCombat*`/`GSTNParts`)**: D6–D7 edit the combat core; run them only after P14, or in a window that thread
  agrees to. D12's parts bridge waits for M4b complete.
- **Desktop thread ("Implementazioni parallele sul fisso")**: D1–D5 and D11 can run on the desktop branch if the owner prefers
  parallel work there; nothing in them touches `GSTNSurface` or the M5 editor shell.
- **Anim pattern thread ("Animare la wall slide nell'ABP")**: D11 uses its names; until then placeholders behind one interface.
- **Camera spec owner**: camera follows the derived latch transform (spec §9), a camera-side change.

## 4. Stop conditions (stop and ask)

- D0: the stick-rotation spin is unreliable on keyboard → ask whether the button is the default (SD.1).
- D6: a seam needs to change existing hit-ring or timeline-player behaviour for timelines that do not use it.
- D9: latch disagreement between client and server above 1 in 10 latches on a **static** prop at 60/15 ms.
- D10: corrections per latch on a moving enemy above 3 at 60/15 ms → propose keeping moving targets for after M6 links.
- Any need to edit the project's wall modes or `GSTNSurface`.

## 5. Assumptions if the owner does not answer

| Q | Assumption used by the code |
|---|---|
| SD.1 | Stick rotation default, optional `Input.Button.Spin` slot |
| SD.2 | Opposite rotation stops the spin, from frame 4 |
| SD.3 | Low gravity only during the air spin (`LowGravityExtraFrames = 0`, S6 not built) |
| SD.4 | One hit + bounce, no air-spin refresh |
| SD.5 | No damage during the seat, no shake-off rule |
| SD.6 | Gravity direction only; ground drill only onto a drillable under the feet, else lag |
| SD.7 | Final jump (`bContinueThroughDestroyed = false`) |
| SD.8 | Players not drillable by default |
| SD.9 | Static test prop in the test module first; bridge plugins later |
| SD.10 | Early release = pop without jump; launch refreshes the air spin |
| SD.11 | **No assumption**: D6–D7 do not start without the owner OK |
| SD.12 | After M4b |

## 6. Edits needing the owner's OK

Enabling `GSTNSpinDrill` in the .uproject; core seams S1–S6 (SD.11); test bindings on the shared test kit; any change to the shared
test character or AnimBP (the anim pattern thread owns the AnimBP side).

## 7. Risks

See spec §12. Plan-specific: D6–D7 is the critical path and depends on another thread's schedule; D1–D5 give a playable spin
before that.
