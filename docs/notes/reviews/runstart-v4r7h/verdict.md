# Verdict run start v4r7h: FAIL (narrow: 1 item at the entry; the hand-off fix itself is good)

Identity: frantic rev-up from frame 1 OK (arms and knees pump from v83, 8 frames earlier than v4r7) / brute heavy
forward torso OK / continuous ramp, no planted first step OK (actor 0.1 -> 7.5 cm/frame by v111, no stall).

Life (mp4, side view): rev-up in place v83-v95, dust and push-off v96, clean arc and lean into the run; weight and arm
follow-through unchanged from v4r7. Hand-off v158-v185: every step planted, alternating L/R, no hop, no double R step.

FAIL items
1. gioco, v82-v84 (entry blend): idle -> start is a 1-frame cut, not a 3-frame blend. L foot v82->v83->v84:
   z 9.6 -> 33.8 -> 45.9 cm and x 1.3 -> 11.9 -> -0.8 cm (forward 11 cm then back 13 cm = one-frame zigzag); the stance
   R foot snaps 7 cm on the floor v83->v84 (x -8.4 -> -1.3). On the sheet v82 is upright, v83 is already the full crouch.
   v4r7 had no snap here (L foot rose 2-4 cm/frame over v84-v90). New defect from the fast entry.

Fixed vs v4r7 (earlier note closed)
- Hop / double R step at the hand-off: fixed. v4r7 v165-v175: L foot hovers at z 19-26 cm, never lands, both feet in
  the air ~10 frames. v4r7h: L lands v173 and holds to v182, R lands v191; loop is entered on the matching foot.

Notes (not FAIL)
- dati montage: start clip weight is 0.82 at v85, falls to 0.50 at v90, back to 1.0 at v100 (v4r7 rose
  monotonically). No visible glitch in v86-v94, but the "instant entry" does not hold its weight; worth checking.
- Brief vs data: the brief gives the hand-off window as v119-v141; the metrics show loop weight 0 -> 1 at v158-v185,
  the same timing as v4r7 (v157-v184). The grids (v84-v144) therefore do not show the fixed zone; judged on mp4 + metrics.
- Root vs capsule: start plants slide 4-7 cm (v105-108 R 7, v114-118 L 5, v125-130 R 6, v138-145 L 4), R v155-163 10 cm,
  loop plants 12-14 cm (inherited). Summary R travel 52.8 vs 33.0 cm is higher than before; L 37.4 vs 36.6.
- The two captures use different idle clips and lanes (Idle_Break v08 vs v07), so v84-v90 is not a like-for-like pose.
- Capture hitches only (not animation): duplicate frame v96=v97, double actor steps at v105/v129/v153/v177.

What the owner will notice: the run now flows into the loop with no hop; at the very first frame the character snaps
from standing into the crouch with a small foot twitch (v83), instead of revving up out of the idle.
