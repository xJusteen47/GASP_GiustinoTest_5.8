# Review: turnaround 180 L, variant 27 (pivot then run), round 1

Reviewer: independent cloud review (rules 10 and 15a). Source: `claude/runstart-generic-desktop` @ b568cf61, this folder.
Looked at: every frame v84-v123 of variant 27 in `ab_26_vs_27_{az180,az90,game}_real.mp4` (v = 80 + video frame index),
plus v26 beside it at v85-v99. Sheets: `review_v84-107_az180.jpg`, `review_v84-107_az90.jpg`, `review_legs_v84-99.jpg`.

## Verdict: FAIL (not for the owner). 1 item gioco, 3 posa, 1 cattura.

The first four frames are what the owner asked for and clearly better than v26. Then the turn stalls side-on for about
8 frames in a hunched one-leg pose, which is the complaint he already made about v21/v26.

## LIFE (video first, no numbers)
- v85-v88 GOOD: upright, L foot steps out toward the new heading and plants about 90 deg turned, stance opens wide,
  arms tucked at the ribs, head leads. Reads as a pivot step. v26 at the same frames is already crouched and running.
- v89-v97 BAD: the body stays in profile (pelvis about 90-110 deg from the start) for about 8 frames. In that time the
  torso drops to roughly 30-40 deg with the head down, the R leg lifts and hangs bent behind (heel toward the glute),
  and the character stands on the L foot without travelling. From behind it is a hunched side-on pose held in place.
- v98-v102: the last 90 deg happens on the L foot, R foot lands at about v102, then the rev.
- Whole turn reads v85-v102 (17 frames), in two beats with a stall, not one pivot of 6-9 frames. In the game camera
  it reads "turn, crouch and wait, go".
- v102-v123 rev: fast cadence, feet near parallel tracks, no crossing seen from the front, no toe bend seen.

## Identity traits (rule 13a)
1. Pivot step with the turn-side foot, planted turned: PASS (v85-v88).
2. Upright during the pivot, no hunch, arms tucked: FAIL. True only for v85-v88; hunch from v89 while still side-on.
3. Frantic rev starts when the pelvis is within 20-30 deg of final: FAIL in game. The rev limbs start at about v93
   (hand-off H = 4 clip frames) with the pelvis still about 70-90 deg away.

## FAIL items
1. **gioco** (first, does not count as a pose round). The body yaw seen in game does not follow the authored root yaw
   after the first 90 deg. Authored (`runstart_pivot_compose.py` FRAC `0,0.05,0.27,0.58,0.85,0.95`, 30 fps clip started
   at v85): 104 deg turned at v91, 153 at v93, 171 at v95. Seen: about 90 at v88-v89, still about 90-110 at v96, final at
   v101-v102. So the rev plays 8 frames before the body faces the run. Fix: dump per frame v84-v104 the clip time and
   weight, actor yaw, and root offset yaw (offset root bone), and compare with `info.yaw_ue_deg`. Find what holds the
   yaw back (offset root rotation limit or smoothing, Mover orientation rate, blend-in). Question for the Unreal
   Assistant (rule 14): "In GASP Mover, when a start/pivot clip carries authored root yaw (180 to 0 in 8 frames), what
   limits how fast the mesh follows it: Offset Root Bone rotation mode and max rotation error, or the Mover orient
   rate? How do the stock reface clips reach their authored yaw?"
2. **posa**: the sprinter drop starts too early. The spine blend idle to rev f0 runs across the pivot, so by v89-v90
   the torso is already near the full w2 lean. Fix: keep spine lean at or under 10 deg and the head level until yaw
   FRAC >= 0.85 (frame H), then drop to the w2 lean over the next 4-6 game frames. Pelvis dip of 3.5 cm can stay.
3. **posa**: the R (pivot) foot after toe-off stays in the air v91-v101, folded behind. Much of this is item 1, so
   judge it again after that fix. If it remains: the low drive swing must land within 3 clip frames of H, shin
   never above horizontal before the first rev contact.
4. **posa**: arms open after the hand-off. From the side at v92-v97 the R elbow flares out and the hand is open and
   hanging. Fix: keep TUCK through H + 2, fists closed, elbows within about 10 cm of the ribs until the pump starts.
5. **cattura**: the ASSERT window is f97-f140, taken from the run start runs, so it does not cover the pivot
   (v85-v96), the part under review. Fix: window from the stick flip frame to +44 for turn sequences, and print the
   clip weight at v85-v96.

## Builder's questions
- (a) Turn longer than target: confirmed, v85-v102, see item 1. The clip itself is authored in range (8 frames to 27 deg).
- (b) Lean from f98 reading hunched: the lean after v102 is the w2 rev's own lean (same as 24/25, tunable with
  `gstn.Loco.TorsoLean`) and is fine once the body faces the run. The problem is the lean before that (item 2).
- (c) R knee zigzag 9.2 cm at rev f12 (about v117): not visible at real speed or frame by frame from behind or the
  side. Under the 10 cm gate. No action.

## Kept (do not make worse)
v85-v88 pivot step, stance width, tucked arms in those frames, no foot crossing, no toe bend, gates on the clip
(`gates_w2t180Lp.json` fails: none).

## Not checked
The three other directions (not built). Slow 4x files have the same 81 frames as the real ones, so they were not
opened. Yaw angles seen are estimates from two views, not measured.
