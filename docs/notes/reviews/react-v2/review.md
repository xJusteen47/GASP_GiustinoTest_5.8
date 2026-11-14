# react-v2 visual review (rule 10, cloud) — 2026-11-14

Material: `Roll.mp4`, `RollBack.mp4` + sheets from `claude/project-thread-0kekzb` (3faa4695). All 24 frames of each clip
extracted with ffmpeg (3 views per frame: right, back, 3/4 front), looked at as one grid per clip
(`Roll_grid_review.jpg`, `RollBack_grid_review.jpg`) plus 2x zooms (f0-11, f12-23) and one detail sheet. Silhouette centroid
and bbox measured per frame on the right view (scale ≈ 1.19 px/cm from the standing end pose).

**Caveat: EDITOR captures, not Play/PIE.** No floor, no capsule, no GA travel, no game camera. Rule 10 still needs the
in-game capture (back + side) before the owner sees them. Floor contact can only be estimated (perspective).

Common checks (both clips): f0 = LieUp (mean diff vs react-v1 GroundHit last frame 0.19-0.21/255), last frame = GetUp end
(0.28/255): blend in and out is seamless. Frame-to-frame change is even (1.0-3.4/255, no spike = no pops). Root in place:
end stance within 2 px of the f0 centroid. Both end on GetUp's relaxed stand, NOT the game idle/guard (same note as v1).

| Clip | Verdict | Reason | Fix |
|---|---|---|---|
| Roll (forward rock-up, 24 f) | PASS (conditional) | Clear beats: rock back with knees to chest f1-f5 (weight on upper back, legs never pass vertical, so no neck load), sit-up with left hand push f7-f8, feet planted f10, squat f11-f15, rise f16-f23, feet planted from f10. Reads as "getting up toward the feet" in all three views; from behind the sit-up/squat is readable. No limb breaks or visible interpenetration. | (1) **In-place vs travel**: the body's own centroid moves ~31 cm TOWARD THE HEAD f0-f4 (rock back), then ~45 cm toward the feet f4-f16. If the GA moves the capsule at constant speed toward the stick, the body slides on its back f0-f6 and the left hand slides at f7-f8. Give the GA a travel curve: ~0 until f6, most travel f7-f15 (after the feet plant at f10 the feet must not slide either, so travel mainly f6-f10), 0 after f16. Better: author the travel in the clip and use root motion. (2) f6→f7 is the biggest pose jump (lying tuck → half sitting): fine at 60 Hz as the "pop" of the rock, recheck in game. (3) Not a roll as such (owner asked "roll that way"): the handover's rock-up choice must be confirmed by the owner when he sees it in game. |
| RollBack (back roll, 24 f) | FAIL | Owner rule "weight on the shoulder, not the neck" still NOT read. From BEHIND (owner's main view) f6-f9 the head is at the bottom centre, chin tucked, legs stacked straight up above it: it reads as standing on the head/neck. The 18-20° pelvis roll is invisible from behind and from the side the folded neck is the lowest body point besides a hand (handover: head 4 cm up). Also: centroid swings ~29 cm toward the head f0-f5, then ~33 cm BACK toward the feet f10-f13 (knees land on the feet side of the root): with the GA moving the capsule toward the head, the body will jerk backwards f10-f13 in game. Lowest pixel f6-f8 is ~13 px under the f0 floor line in the side view (hand nearer the camera, perspective: check the floor report). Landing on hands and knees f12 → squat f15 → stand f23 is clean. | (1) Make it a real over-the-shoulder roll: pelvis/chest roll 35-45° at f5-f9 so the legs pass diagonally over the RIGHT shoulder (from behind the feet must travel to one side of the head, not over its middle); head turned to the LEFT and kept ≥10-15 cm off the floor; right shoulder blade/upper arm is the lowest body part f6-f8, both hands planted beside the head (not one hand reaching down). (2) Land the knees on the HEAD side of the start (travel direction), i.e. keep the local centroid monotonic toward the head, or author travel + root motion; otherwise GA travel curve 0 until f4, then f4-f12, 0 after f13. (3) Floor report at f5-f9 (hand and head). |

Summary: Roll PASS (conditional: GA travel curve + owner confirms rock-up instead of somersault), RollBack FAIL (still
over the neck when seen from behind; local centroid goes back against the travel). Next: RollBack rework, then in-game
capture of both from the game camera (behind) + side, with the GA travel, before the owner.
