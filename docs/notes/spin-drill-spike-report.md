# Spin and drill — spike report

Spikes of [spin-drill-plan.md](../architecture/plans/spin-drill-plan.md). Spec: [spin-drill.md](../architecture/spin-drill.md)
§5 and §13b; keyboard shortcut proposal: [input.md](../architecture/input.md) §9.

## D0 — the Spin input condition as the spin command (2026-11-01, desktop)

Scope, as briefed: input only, no gameplay code, no change to the condition. Evidence is automation tests in
`Plugins/GSTNInput/Source/GSTNInputCore/Private/Tests/GSTN_InputTests_Conditions.cpp` (four new tests, below) run headless,
plus reading `FGSTN_InputCondition_Spin::Matches`, the direction classifier and the input history. Not done: PIE with a real
pad and keyboard, so the plan's gate "stop if the condition misses more than 5 % of deliberate rotations" is **not measured**
(it needs human input; see Next).

### What the condition does (from the code)

- `FGSTN_InputCondition_Spin` reads the history's direction events, merges consecutive events with the same Stick-space 8-way
  sector into nodes, and walks back from a node at or before the anchor: each step must move 1 to `1 + MaxSkippedSectors` sectors
  in one sense, no node may be neutral (or below `MinZone`), the whole span must fit `MaxFrames`, and the sum must reach
  `MinDegrees`. Defaults: 360°, 30 frames, Tilt, 1 skipped sector, `Either`. `MinDegrees` is clamped 45–720, so a 180° or 135°
  slot is already expressible.
- Angles: 0 = up, clockwise. A-S-D is left (270°), down (180°), right (90°): counter-clockwise, as input.md §9 says.
- Fixed tick 62.5 Hz (`Config/DefaultNetworkPrediction.ini`), so 30 frames = 480 ms.
- The history (`FGSTN_InputHistory`) pushes a direction event when the 8-way direction changes in **any** of the three spaces
  (Stick, World, Facing) or the zone changes, into a ring of **16** events (`MaxEvents`); the oldest is dropped on overflow.

### New tests

| Test | Covers |
|---|---|
| `GSTNInput.Core.Input.Conditions.SpinStickSpeeds` | (a) both senses: a sector every 1, 2, 3 frames; the slowest accepted turn (exactly 30 frames) and 31 frames rejected; a turn in 4 frames seen every other sector; 135° jumps rejected; a continuous 15°/frame sweep; a one-sector wobble breaks the chain |
| `GSTNInput.Core.Input.Conditions.SpinDirectionRingLimit` | (a) the same continuous sweep with camera 15° and facing 30°: pins the ring limit below |
| `GSTNInput.Core.Input.Conditions.SpinKeyboardSweep` | (b) A-S-D and D-S-A on 180° slots, with and without diagonal frames, fast and slow; the 360 slot ignores the half sweep; neutral gaps and A+S on one frame; the Motion alternative |
| `GSTNInput.Core.Input.Conditions.SpinKeyboardFalsePositives` | (c) strafes (direct, through neutral, diagonal to diagonal, zig-zag holding S), double-tap dash, flick back, sidestep; the U-turn risk |

Build: `Saved/Logs/build-spindrill-d0.log`, Development Editor, UBT command line, editor closed: `Result: Succeeded`.
Headless run: `Saved/Logs/headless-spindrill-d0.log` (`Automation RunTests GSTNInput`), measured with
`Select-String 'Test Completed. Result=\{Success\}' | Measure-Object`: **123 Success, 0 Fail** (123 completed; exit code 0).
The four new tests and the existing `Conditions.Spin` and `Resolver.StickOnlySpinFiresOnce` are among the successes.

### (a) Stick rotation, both senses, slow and fast, default leniency

**Yes, with one limit.** Every tested speed from 4 frames (64 ms, every other sector) to 30 frames (480 ms) matches in both
senses and only in the right sense; 31 frames does not (by design: `MaxFrames`). 135° between two samples is beyond the default
skip of one sector, so a turn faster than about 4 frames is lost; a person cannot spin that fast on a pad, so this does not
matter. A one-sector wobble back in the middle breaks the chain (the full turn must then be made after the wobble), which is the
existing "reversing" rule; slow, shaky turns are the likely victims.

**Limit (finding): the 16-event history ring.** World and Facing directions change on other frames than the Stick direction
whenever the camera or the character is not lined up with a 45° grid (the normal case in play). A continuous 360° sweep then
produces up to three events per 45° instead of one. `SpinDirectionRingLimit` measures it: the same 15°/frame sweep (24 frames,
384 ms) matches with camera and facing at 0 and **does not match** with camera 15° and facing 30°, in both senses. My model of
the classifier (scratch Python, not a measurement) counts 9 events per turn aligned and 25 with that offset, so the ring keeps
only about the last 240°. The same model with the character turning toward the stick (camera 15°, turn rates 5–15°/frame)
misses some 12–15°/frame sweeps and keeps 24°/frame ones: fast turns survive, deliberate slower turns are at risk. This is a
history property, not a Spin bug, and it also shortens what the other conditions that read direction events (`Motion`, for one) can see during heavy direction churn.
Possible fixes (not made; GSTNInput, its own small checkpoint):
1. A second small ring in the history that logs only Stick-space sector changes (16 entries of frame + sector, about 80 B of
   sync state), read by Spin. Smallest behaviour change; the other conditions keep their ring.
2. `MaxEvents` 32 for direction events only. Doubles that ring's copy and serialisation cost every frame.
3. Push an event only when the Stick direction or zone changes and recompute Facing/World on read. Not possible: facing comes
   from corrected state and the history exists because it cannot be rebuilt (header comment).

Recommended: 1, before D4 relies on the stick spin.

**Fix chosen (D3b, 2026-11-01, owner OK): fix 2, compacted.** The owner preferred one bigger ring to a second log, on condition
that it cost no more memory or copying than before (the game targets very weak hardware). The stick ring is 32 events, the button
ring stays 16, and both rings store packed events (16-bit frames rebuilt from the newest frame, the stick in 15 bits plus the aim
byte): `sizeof(FGSTN_InputHistory)` went from 348 to 232 bytes, the wire is unchanged per event. `SpinDirectionRingLimit` now
matches the camera 15 / facing 30 sweep in both senses, and `SpinThreeSpaceChurn` checks a 30-frame spin with all three spaces
changing on different frames. Details in input.md ("History rings and storage") and docs/notes/input-memory-audit.md.

### (b) A 180° keyboard sweep with 8-way digital input

**Yes for a normal roll, today, with `MinDegrees = 180`** and the default 30 frames / one skipped sector: A-S-D rolled through
both diagonals (left, down-left, down, down-right, right), with one diagonal, with none (each key change is a 90° step, allowed by
the skip), one frame per key state, and over exactly 30 frames all match the counter-clockwise slot and not the clockwise one;
D-S-A is the mirror. All four keys rolled around (W, W+D, D, ..., W) is a 360 on the default slot. The default 360 slot ignores
the half sweep, so the shortcut needs its own slots (two, one per sense) or `DigitalMinDegrees` (input.md §9, not built).

Two keyboard habits fail and would need a change to the condition (described, not made):
- **A neutral frame between two keys** (A released one frame before S is pressed): the neutral node breaks the chain. Change:
  `MaxNeutralFrames` on the Spin condition (default 0 = today's behaviour), a neutral stretch up to that length between two
  sectors is skipped instead of ending the walk; the step across it is measured between the sectors on both sides.
- **A and S pressed on the same frame**: the sweep starts on the diagonal and is 135° only. A 135° slot catches it (tested), at
  the cost of more false positives; or count the sweep from the first sector touched with `DigitalMinDegrees` = 135.

**Alternative with no change:** a `Motion` condition in Stick space, Left then Down then Right (other stick events allowed in
between, 12 frames between elements, 30 in total). It matches the rolls and the neutral gaps, and rejects D-S-A (its mirror
is a second slot). It misses A+S on one frame (no pure left), unless its first element also takes down-left.

### (c) False positives

With a 180° `Either` slot, none of these match: left-right strafes (direct, through neutral), down-left to down-right, a
zig-zag while holding S, a double-tap dash, a forward-then-flick-back, a forward-then-sidestep. A left-right switch is a
four-sector jump in one event, so the skip rule rejects it.

**Risk (pinned in the test as "known risk"): a quick U-turn through the diagonals is a real 180° rotation.** W, W+D, D, D+S, S
within 30 frames matches a 180° slot, and so does a half circle of a gamepad stick. A running player turning around would spin.
The `Motion` form rejects that U-turn (no left), but a stick half circle left-down-right still matches it on a pad. So any
180° shortcut should be **limited to digital input**: the `bDigitalDirection` bit of input.md §9 (not built), or a binding set
that only the keyboard uses (not checked whether bindings can be per device).

### Recommendation for the default kit (SD.1)

- **Gamepad: stick rotation**, two 360° slots (one per sense) with the default leniency. Works at every human speed tested.
  Fix the ring limit (option 1 above) before D4; until then slower deliberate spins can be missed when the camera and the
  character are not lined up.
- **Keyboard: the A-S-D / D-S-A shortcut as two `Motion` slots** (left-down-right counter-clockwise, right-down-left
  clockwise) rather than 180° Spin slots: no condition change, tolerant of neutral gaps, fewer false positives. Gate it to
  digital input when `bDigitalDirection` exists; until then a pad half circle also fires it.
- **Button: keep it optional** (an extra binding, not in the default kit). It has no false positives and is the fallback if the
  owner finds the keyboard shortcut awkward in PIE.

The plan's stop condition for D0 ("stick spin unreliable on keyboard, ask whether the button is the default") is not hit: the
keyboard shortcut is reliable in the tests. It is still the owner's call after trying it.

### Next / blocks

- Nothing here blocks **D1** (plugin skeleton).
- Before **D4**: decide the ring fix (owner OK for a GSTNInput change) and the keyboard form (Spin 180 vs Motion), and run the
  PIE check of the 5 % gate with a real pad and keyboard.
