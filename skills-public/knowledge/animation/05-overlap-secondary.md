# L1 · Overlap, drag, follow-through, secondary motion (`P-OVERLAP`)

Level 1. Body parts do not start and stop together. The parent leads, the children follow later and settle later.
This is the property that interpolated animation loses first and the one viewers feel most as "alive".

Terms. **Drag**: a child lags behind its parent when the parent starts. **Follow-through**: parts keep going after the
main action stops. **Overlap**: the timing offset between parts. **Successive breaking of joints** (Williams): a
movement propagates joint by joint along a chain. **Secondary motion**: motion caused by the main action (hair, cloth,
belly, a loose hand), not a separate intention.

### P-OVERLAP-01 — Parts do not start and stop together
- **Rule:** in any movement involving the whole body, the big joints reach their speed peaks and their stops at
  different frames.
- **Why:** every segment has its own mass and is moved by its parent; synchrony is the signature of pose interpolation.
- **Measure:** `stop_sync_share` = share of joint speed minima that coincide (±1 f) with the minima of ≥60% of the
  other joints. Measured: free mocap **0.00-0.25**; the same clips rebuilt from keys every 0.2 s **0.88-0.96**
  (`P-MEASURE-03`). Indicative: > 0.5 FAIL, 0.35-0.5 look again.
- **Broken looks like:** the whole body arrives in each pose at once, then leaves at once; on a speed graph all joints
  dip to zero at the same frames (`figures/A-overlap-punch.png`, right).
- **Severity:** FAIL.
- **Exceptions:** an impact that stops the whole body (it still shows lag in the recoil); very short clips (< 0.5 s)
  where the metric has too few minima, judge by eye.
- **Figure:** `figures/A-overlap-punch.png`, `figures/D-overlap-chain.png`.

### P-OVERLAP-02 — Lead and lag along the chain
- **Rule:** the movement starts in the part that drives it (usually hips for a body action, shoulder for an arm action)
  and each child follows it by a small delay; on stopping, the children keep going a little longer.
- **Measure:** onset and peak frames along the chain. Indicative delay between adjacent segments 1-4 f at 60 fps for
  fast actions, more for loose or heavy parts; the hand peaks last.
- **Broken looks like:** shoulder, elbow and hand start on the same frame and arrive on the same frame.
- **Severity:** FAIL for actions; WARN for small gestures.

### P-OVERLAP-03 — Secondary motion has a cause, decays and ends
- **Rule:** secondary motion is triggered by a change in the main motion, is delayed, decays and ends; it does not
  move on its own.
- **Why:** motion without cause reads as noise or as the character shivering.
- **Measure:** each secondary oscillation starts after a main-motion change, amplitude decreasing.
- **Broken looks like:** a held pose with a hand that keeps waving; a lock of hair that starts swinging before the head moves.
- **Severity:** WARN (FAIL if it hides the read of the main action).

### P-OVERLAP-04 — Do not overcook it
- **Rule:** overlap supports the action; it never becomes the action. Too much delay or overshoot reads as rubber.
- **Measure:** a lag so long that a child is still moving the previous way when the parent has already done the next
  thing twice; overshoot larger than the movement's intent.
- **Severity:** WARN.

### P-OVERLAP-05 — Follow-through after the strike
- **Rule:** after a strike or a stop, the parts that were moving fastest continue past the end pose and settle last.
- **Severity:** WARN (FAIL on attack recoveries that stop dead).

## Sources
Williams ("successive breaking of joints", overlap); Thomas & Johnston (follow-through and overlapping action);
measurement on CMU 02_05, 16_08, 16_05, 16_41, 09_01 vs their lifeless rebuilds.
