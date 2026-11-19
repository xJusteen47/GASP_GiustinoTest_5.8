# L1 · Arcs and paths (`P-ARC`)

Level 1. Joints rotate, so the ends of limbs travel on curves. A straight path on an organic part is read as
mechanical or as IK pulling a point through space.

### P-ARC-01 — Extremities travel on arcs
- **Rule:** hands, feet, head and the tip of anything swinging follow arcs between their extremes; the in-between bulges
  out of the straight chord.
- **Why:** a limb is a chain of rotations; its end describes circles around the joints.
- **Measure:** path of the part relative to the pelvis between speed minima; `arc_bulge_median` = max distance from the
  chord / chord length. Free mocap hands and head: about **0.03-0.10** (measured, `P-MEASURE-03`). Near 0 on a long
  movement = straight line.
- **Broken looks like:** a hand that slides in a straight line from guard to target; a foot that lifts and lands along a
  straight diagonal.
- **Severity:** WARN; FAIL when a whole action's leading part is straight (bulge < 0.01 over a chord > 10% of height).
- **Exceptions:** the striking part of a thrust in its last 2-3 frames (a straight jab is straight at the end, its
  wind-up and recoil are not); IK-driven contacts on purpose (a hand sliding on a wall).
- **Figure:** `figures/A-arcs-turn.png`.

### P-ARC-02 — Arcs are smooth: no zig-zag, no kink
- **Rule:** consecutive frames do not reverse direction on the path except at an extreme.
- **Why:** a zig-zag is a key fighting its neighbours (wrong breakdown or gimbal flip); the eye catches it as a pop.
- **Measure:** angle between consecutive frame steps of the part; a reversal > 90° away from a speed minimum is a kink.
- **Broken looks like:** a wrist that wobbles on its way; a "V" in an onion-skin trail.
- **Severity:** FAIL.

### P-ARC-03 — FK arcs vs IK paths
- **Rule:** rotation-driven (FK) parts make natural arcs by themselves; position-driven (IK) parts move in straight lines
  between keys **unless** the path is shaped by extra keys or curves. Choose the space on purpose and shape IK paths.
- **Why:** interpolating a position interpolates in a straight line.
- **Measure:** if a part is IK-driven, check its in-betweens for `P-ARC-01`.
- **Severity:** WARN (process); the visible result is judged by `P-ARC-01`.

### P-ARC-04 — The centre of mass in the air is a parabola
- **Rule:** once airborne, the body's centre of mass follows a parabola; limbs may move freely, the centre may not.
- **Why:** gravity. Rotating the pelvis in the air does not move the centre.
- **Measure:** pelvis-height curve in the air fitted by a parabola; residual small compared with the jump height.
- **Broken looks like:** hang time with a flat top that is not in the gameplay; a body that "floats down".
- **Exceptions:** gameplay-driven air motion (the root is the game's; the clip's own centre must not fight it).
- **Severity:** WARN for clips; the game's trajectory is not the clip's fault.

## Sources
Thomas & Johnston (arcs); Williams (arcs, "wave" in the in-between); measurement on CMU 16_41, 02_05.
