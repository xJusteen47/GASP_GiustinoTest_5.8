# L1 · Holds and life (`P-HOLD`, `P-LIFE`)

Level 1. **Read this first** if you generate or edit animation with a script, a solver, a model or an interpolator.
A clip can pass every technical check (no foot slide, no pop, clean loop, curves present) and still be dead. "Dead" has
recognisable, measurable signatures; they are listed here so that a reviewer can name them and an author can avoid them.

## Holds

### P-HOLD-01 — No dead frames inside an action
- **Rule:** in an action, the body is never completely still unless the hold is intended and readable (an impact hold,
  a pose held for gameplay).
- **Measure:** `dead_frames` = frames where the mean speed of the big joints is ~0. Free mocap: 0 in every clip measured;
  lifeless rebuilds: up to 15 in a 2.5 s clip. Indicative: any unintended run of ≥ 3 dead frames inside an action = FAIL.
- **Broken looks like:** the body freezes between two poses; the "computer" hold.
- **Severity:** FAIL.

### P-HOLD-02 — Holds are moving holds
- **Rule:** a held pose keeps a small drift: the body sinks a little, a part settles, breath or balance continues.
- **Why:** a living body is never perfectly still; a 100% still hold looks like the animation stopped.
- **Measure:** during a hold, the leading parts keep moving at a low speed (a few % of the action's peak), toward the
  same direction (drift), not jitter.
- **Broken looks like:** a perfect freeze (dead), or noise that wiggles in all directions (fake life).
- **Severity:** WARN for holds ≥ 10 f; impact holds (hit-pause) are frozen on purpose (`P-GAME-07`).

### P-HOLD-03 — The hold is chosen, not left by the interpolator
- **Rule:** a hold exists because a beat needs reading (impact, end pose, a pause for gameplay), not because two keys are
  near-equal.
- **Severity:** WARN.

## Signatures of lifeless (interpolated / generated) animation

These signatures were measured by rebuilding free motion-capture clips from key poses every 0.2 s with smoothstep easing
(`tools/animkb/kb_motion.py lifeless`), the typical output of "a script interpolates a few poses". Figures in
`figures/A-*`. Each signature is a FAIL criterion; any one is enough.

### P-LIFE-01 — Lockstep: every joint stops at every key
- **Signature:** all joints reach a speed minimum on the same frames (the keys), then accelerate together.
- **Measure:** `stop_sync_share` > 0.5 (mocap 0.00-0.25, lifeless 0.88-0.96). On the angular-speed graph: all curves
  touch zero together (`figures/A-overlap-punch.png`).
- **Fix direction:** offset the keys of the children (`P-OVERLAP-02`), break tangents, add follow-through; do not add
  noise.

### P-LIFE-02 — Ease on every key ("floaty", "morphing")
- **Signature:** spacing crowds at both ends of every segment, so the clip pulses at the key rhythm; every pose is
  reached gently, nothing lands or snaps.
- **Measure:** spacing ruler (`figures/A-spacing-punch.png`, lower): ticks bunch at the key frames for every part at once.
- **Fix direction:** spacing that follows force (`P-SPACE-01`), strikes that accelerate into contact (`P-SPACE-03`).

### P-LIFE-03 — Morphed in-betweens
- **Signature:** in-betweens are averages of their keys; limbs pass through the body or shorten mid-way; arcs are
  replaced by chords (`P-ARC-01`) or by arcs that the joints' rotations produce by accident.
- **Measure:** breakdown frames at ~50% (`P-SPACE-04`); arcs.

### P-LIFE-04 — Feet that never plant
- **Signature:** feet never satisfy height ~0 and speed ~0 together, or they slide through the whole plant.
- **Measure:** `foot_contacts` (lifeless run rebuild: 0 contacts per foot vs 2 in mocap) (`P-CONTACT-01/02`).

### P-LIFE-05 — Weight without loading
- **Signature:** the pelvis travels in straight eased segments; no dip before pushes, no absorption after contacts; the
  lowest point is on the key frame.
- **Measure:** pelvis-height curve vs contacts (`figures/A-contact-jump.png`), `P-WEIGHT-03/04`.

### P-LIFE-06 — Symmetry and twinning
- **Signature:** left and right do the same thing at the same time; poses are symmetric.
- **Measure:** compare left/right joint curves: identical shape and phase = twinning (`P-POSE-04`).

### P-LIFE-07 — Clean but empty
- **Signature:** the clip passes isolation checks (no slide, no pop, clean loop, curves present) and still reads as a
  puppet. Technical checks are **necessary, not sufficient**.
- **Rule for reviews:** a review that only answers technical questions cannot give a PASS. The life axis of the rubric
  (`review-rubric.md`) must be answered with frames.

## How life is added (for authors)
1. Start from motion that already has life when one exists: motion capture, a licensed library clip, the director's own
   poses or performance reference. Do not build a full move from a handful of text-described poses.
2. If you must build from poses: block stepped (`P-TIME-03`), add breakdowns that favour and arc (`P-SPACE-04`),
   then offset the chain (`P-OVERLAP-02`), then shape spacing per part (`P-SPACE-01`), then contacts (`P-CONTACT-*`).
3. Measure (`P-MEASURE`) before showing anyone.
4. Never add random noise to fake life; it fails `P-OVERLAP-03` and `P-HOLD-02`.

## Sources
Measurements in `figures/metrics-cmu-vs-lifeless.json`; Williams and Thomas & Johnston for the principles behind each
signature.
