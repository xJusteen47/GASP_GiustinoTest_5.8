# L1 · Anticipation, action, settle (`P-ANTIC`, `P-SETTLE`)

Level 1. Most actions have three beats: prepare, do, recover. In games the first beat fights responsiveness, so it is
shaped rather than removed.

### P-ANTIC-01 — Every significant action is prepared
- **Rule:** a significant change of direction or effort is preceded by a smaller movement the other way (wind-up,
  dip, lean back, look).
- **Why:** it tells the viewer what is about to happen and gives the action force; without it the action pops.
- **Measure:** the leading part or the COM moves opposite to the action before the action; frame counts recorded.
- **Broken looks like:** a strike that starts from the guard pose straight toward the target.
- **Severity:** FAIL for strong attacks, jumps, starts; WARN for light/fast actions.

### P-ANTIC-02 — Anticipation size and length scale with the action
- **Rule:** bigger or heavier action -> bigger or longer anticipation; quick actions get a tiny, fast one (even 1-3 f
  or only in the trunk or the eyes).
- **Measure:** ratio anticipation frames : action frames. Indicative: fast strike 1:1 to 2:1 in frames but with the
  anticipation's spacing slow and the action's fast; a big slam 3:1 or more.
- **Severity:** WARN.

### P-ANTIC-03 — In games, anticipation is compressed, not deleted
- **Rule:** when input must respond within a few frames, keep a short anticipation in the first frames (pose changes
  on frame 1) or move it into the trunk, the head or an effect; never remove it entirely from a heavy action.
- **Why:** responsiveness and readability are both requirements; see `P-GAME-05`.
- **Severity:** WARN.

### P-ANTIC-04 — Frame 1 is already a new pose
- **Rule:** on an input-driven action the first frame already differs clearly from the pose before; no soft blend-in that
  delays the read.
- **Measure:** pixel/pose difference between the frame before input and frame 1 is of the order of the following frames.
- **Broken looks like:** "still, still, then go": two or three near-identical frames at the start.
- **Severity:** FAIL for attacks and input-driven starts.

### P-ANTIC-05 — Anticipation opposes the action's line
- **Rule:** the anticipation pose bends the line of action the other way from the action pose: spine curve, pelvis and
  chest rotation and weight go one way, the action the other. The contrast must read when the two poses are flipped.
- **Measure:** sign of spine curvature and pelvis/chest yaw at the anticipation key vs the action key: opposite.
- **Severity:** FAIL for strong attacks; WARN for light ones.

### P-ANTIC-06 — Reach the loaded pose fast, spend the frames holding it
- **Rule:** in game attacks the body gets to the anticipation pose quickly and spends the anticipation frames in a
  (moving) hold, so the read is early and the responsiveness budget is kept.
- **Measure:** frames from the first pose change to the anticipation extreme vs frames held there.
- **Severity:** WARN (a common game choice; the profile may set it).

---

### P-SETTLE-01 — Overshoot then settle
- **Rule:** a fast movement that stops passes its final position slightly, comes back and settles; the overshoot is
  bigger on the lighter, further segments.
- **Why:** momentum. A dead stop on the target frame reads as hitting a wall or being animated.
- **Measure:** after the extreme, the part reverses briefly then rests. Indicatively a few % of the movement, 4-12 f.
- **Broken looks like:** every part stops on the same frame with no overshoot (`P-LIFE-01`).
- **Severity:** WARN; FAIL as part of `P-LIFE-01`.

### P-SETTLE-02 — Recovery reads and returns to a usable pose
- **Rule:** after the action the body returns toward neutral in a readable way: the leading part leads, swings decay,
  and the pose at the end is one the next action can start from.
- **Measure:** at the last frame (or the first frame the game allows another action), body near its neutral or the next
  clip's start pose; no pop when the next clip blends in.
- **Severity:** FAIL when the next clip pops; WARN otherwise.

### P-SETTLE-03 — Settles decay, they do not repeat
- **Rule:** each oscillation is smaller and shorter than the previous one; two or three at most.
- **Broken looks like:** jelly: equal wobbles that keep going.
- **Severity:** WARN.

### P-SETTLE-04 — The recovery is designed, not interpolated
- **Rule:** between the impact and the hand-off pose there is a designed breakdown (an arc, a favour) and a small rebound
  of the parts that hit; the body does not travel in a straight eased line from hit to idle.
- **Measure:** at least one breakdown in the recovery; the striking part shows a small reversal before settling.
- **Severity:** WARN (FAIL when the recovery is a single eased segment, see `P-LIFE-02`).

## Sources
Thomas & Johnston (anticipation, follow-through); Williams (anticipation, overshoot); game-animation practice on
responsiveness (Cooper, *Game Anim*).
