# react-v1 visual review (rule 10, cloud) — 2026-11-14

Material: `docs/notes/reviews/react-v1/*.mp4` from `claude/project-thread-yhnbvx` (bf82599d). Every frame extracted
with ffmpeg (60 fps clips, 3 views per frame: right, back, 3/4 front), looked at as one grid per clip plus one zoom of
Roll f0-f16. Silhouette bounding boxes measured per frame (scale ≈ 1.2 px/cm from the standing end pose).

**Caveat: these are EDITOR captures, not Play/PIE.** No floor, no wall, no capsule, no game camera, no blend from the
previous state. Rule 10 is only half met: the in-game look (hub Z6 / GA TL Montage) is still required before the owner sees them.
New clips: no base to compare.

Continuity check: GroundHit last frame, GetUp f0 and Roll f0 are pixel-identical to LieUp (mean diff < 0.2/255). Good.

| Clip | Verdict | Reason | Fix |
|---|---|---|---|
| HitStand (still) | PASS (weak) | Side view reads as a recoil (head snapped back, chest back, knees soft). From BEHIND (owner's main view) the arms are flung out almost symmetrically: reads as "startled/balancing", not "struck". | Asymmetry: twist chest 15-25° away from the hit side, trailing arm lower and behind, lead arm up; shift pelvis toward the hit direction. Later: high/low variants. |
| Tumble (still) | PASS | Arched, horizontal, limbs trailing: reads as "launched" in all three views. | Note: its curve (arched back) is OPPOSITE to GroundHit f0 (piked). Game side give the blend into GroundHit 3-4 frames, or re-key GroundHit f0 closer to Tumble. |
| Splat (still) | PASS (conditional) | Upright, arms flung, one knee bent, head back: reads as "hit a wall" from back and 3/4. Wall not rendered, so contact plane unverified; side view is thin and one forearm is in front of the chest. | Check against a real wall in game: back, heels, both hands/forearms on the wall plane; push chest slightly off the wall for the rebound read. |
| LieUp (still) | PASS | Supine, flat, clearly "down" from side and 3/4; from behind it is a compact lump but still reads as lying. | Game side: pelvis is ~10 cm behind the root (known); align capsule/hurtbox. |
| GroundHit (18 f) | PASS | Matches owner rule: lands f2, small bounce (apex f6-f8, top of body ~13-17 cm above rest), second contact f10, settles into LieUp by f14. | (1) The impact frame f2-f3 sits ~15 cm lower on screen than the rest pose in BOTH right and back views: floor is not rendered, check the floor report at f2-f3 for penetration/squash below floor. (2) Air time 8 f for ~15 cm = about 7 g: snappy, fine for feel, but if it reads as a twitch in game give it 2 more frames at the apex. (3) f0→f1 pose jump (pike to almost flat in 1 frame): OK as an impact snap, recheck after the Tumble blend. |
| GetUp (26 f) | PASS | Owner rule "A = stand up in place" met: end stance is centred on the lying body's middle (within ~2 cm horizontally). Clear beats: sit-up f3-f6, hand plant f5-f8, tuck f8-f11, rise f12-f25; feet stay planted from f12. No visible joint breaks. | (1) Feet tuck under the hips f7-f10 is fast (~45 cm in 3 f): check heels don't slide on the floor in game. (2) Ends on a relaxed stand, not the game idle/guard: needs a blend-out or a last key matching idle. |
| Roll (24 f) | FAIL | Owner rule "hold stick = roll THAT way" not met by the clip alone: it is a BACK shoulder roll (legs over the head toward the head side) with **net travel ≈ 0** (end stance is at the same spot as GetUp's end). Only one direction exists. Pose flow itself is clean (no pops f0-f23, knees land f11-f12). | Decide (owner/design): A) keep the clip IN PLACE and let the GA move the capsule toward the stick (then the body must visibly travel the same way: today legs go toward the head while the body stays, so in game it will slide), or B) author with travel (~1-1.5 m) and add a FORWARD roll (over the knees, toward the feet) so the GA picks by dot(stick, head direction) instead of turning the lying body (which would pop). Also: f5-f8 the weight is on the neck; move it over one shoulder. |

Summary: 6 PASS (HitStand, Splat with fixes), 1 FAIL (Roll: direction/travel). Next: in-game PIE capture of all seven
from the game camera (behind) + side, then owner.
