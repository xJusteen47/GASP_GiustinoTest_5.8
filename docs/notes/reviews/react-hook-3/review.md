# react-hook-3 visual review (rule 10, cloud): reactions after the roll-back fix, 2026-11-14

Material: `docs/notes/reviews/react-hook-3/<seq>.mp4` on `claude/project-thread-wkds3r` `d27c77f5` (code `125897ce`). All 1021 video frames
of the five sequences extracted with ffmpeg; consecutive frames looked at around the hit, the second hit, the end of the stun, the landing,
the bounce and every get-up start and end, in both views. States and positions from `<seq>_frames.jsonl`. Frame numbers are VIDEO frames
(`v`, 60 fps). Evidence: `review/review_az90.jpg`, `review/review_az45.jpg` (one composite per view), plus two crops of the landing pop.

**Result: the roll back is fixed and every roll passes. One FAIL stops the set: a one-frame pop at every landing (small code fix).
The back-hit turn was not captured, so it is not verified.**

| Reaction | Verdict | What the video shows (frames) |
|---|---|---|
| hitstand, held for the stun | **PASS** | Hitstun v21-v77. Pose on screen v22 (transitional, knees tucked) and full from v23, identical until v78: frame-to-frame change under 300 px out of 430k from v24 to v48. Then the knockback slide moves the pawn 138 cm (v49-v76) in the same pose. No idle while stunned (react-hook-1 fail is gone). |
| hitstand, restart on the 2nd hit (v41) | **PASS, acceptable** | The montage restarts (a0 v41, a1 v42) but nothing moves: v40/v41/v42 differ by about 100 px, the same as two held frames. No twitch. Design note below (the second hit does not read on the victim). |
| tumble, snap in | **PASS** | v22 still idle, v23 horizontal, v24 full Tumble, held to v83. Victim faces the attacker and flies back first. Hitstop of the second hit v41-v48 in the air, same pose. |
| landing (Tumble to Knockdown) | **FAIL** | **v84 (KD+0): the body jumps UP about 85 cm for one frame**, still in the Tumble pose, then v85 shows GroundHit back at the old height. Same frame in all four sequences and both views (roll v84, rollback v84, roll_late v83). After it the descent is fine: v85 to v89, 5 frames, no big single step. Evidence `review/landing_pop_*.jpg`. |
| bounce | **PASS on size and time, no rebound on screen** | Floats at about +18 cm v89-v92, on the floor by v95-v96 (react-hook-1: 44 cm, 34 frames). But the capsule rises exactly while the mesh is still coming down, so the body only ever goes down: it reads as "sinks, hovers 4 frames, settles", not as "hits, small bounce, lies". |
| GroundHit to LieUp | **PASS** | v98 to v99: no pop. Lying still from v96. |
| get-up with A | **Behaviour PASS, first frames FAIL (anim)** | Press v134, state v135, in place, facing kept, standing v158-v160, whole body in frame in both views. **v135, v136, v137 show the lying pose almost unchanged**; head lifts at v138, torso at v139. Three soft frames against the feel rule. No crouch after it (v161-v169 upright). |
| roll forward | **PASS** | Starts v107 = KD+23 with the body flat on the floor (bounce over at v96). v108 legs already up (clear change). Travel v112-v121, 79.4 cm toward the feet, yaw 180 throughout, standing v128-v129, no slide after. |
| roll late | **PASS** | Stick at v133, state v135 = KD+52, v136 legs up. Same roll, same travel. Rolls on any frame while lying. |
| roll back | **PASS** | AM_React_RollBack, starts v107 on the floor, v108 legs up, over the head v109-v118, on hands and knees v119-v121, standing v128 **facing the attacker**. Yaw 180 from v84 to v130. Travel v112-v117, 65 cm toward the head. The turn from v131 is the capture's stick, still held 3 frames after the get-up. |
| back hit turns the victim | **NOT VERIFIED** | All five captures are front hits (facing error 0), where the turn does nothing. The code is there (`GSTN_CombatSimVictim.cpp:600`, `bTurnBackAlongKnockback`), no automation test names it, no capture shows it. |

## The two flagged points

- **RollBack 65 cm vs Roll 80 cm: acceptable.** It is clip key data, the roll looks right at 65 cm (no slide, no stretch). A shorter
  back roll is a normal choice; whether it should match 80 cm is the owner's call in the playtest, and it is a data change.
- **HitStand restart at v41: acceptable.** Invisible on video.

## Other findings

- **A. Every reaction shows one frame late.** The state label changes one frame before the pose (v21 hitstand, v22 tumble, v107 and v135
  get-ups still show the old pose). The landing pop is this same frame: `UGSTN_ReactionAnimComponent` applies the entry drop
  (`UpdateEntryDrop(0.f)` right after `PlayStage`) on the tick the montage starts, but the new pose reaches the mesh on the next one.
- **B. Deep crouch after the hitstand** v80-v104 (deepest v82-v90), about 25 frames. Not after the get-up or the rolls any more. It is
  the known Anim BP crouch, but hitstand is the most common reaction, so the owner will see it on every hit.
- **C. The second hit does not read on the victim** (same held pose). Design idea, not a bug: a short shake during hitstop.
- **D. Roll clips stall one frame at the start** (montage frame 1.6 then 1.8; v108 and v109 nearly equal). Minor.

## Fix list

Code (small, one thread):
1. Landing pop: start the entry drop on the first frame the new pose is on the mesh (one tick later), or evaluate the pose on the tick
   the montage starts; the second also removes the one-frame delay of finding A for every reaction.
2. Back hit: one capture with the attacker behind the victim (hitstand and knockdown) and an automation test on `bTurnBackAlongKnockback`.
3. Hitstand exit: give it the same held blend-out the get-ups use, so the crouch of finding B is hidden until the Anim BP fix.

Animation (already queued):
1. New GroundHit clip that starts at Tumble height. It removes the entry drop and lets the bounce read as a bounce; judge the bounce again then.
2. GetUp: the first clip frame must already show head and shoulders up.

## Summary

PASS: hitstand hold, tumble snap, GroundHit to LieUp, roll, roll late, roll back (the focus of this build), A = stand in place.
FAIL: one-frame 85 cm pop up at every landing (code). Open: back hit not captured; get-up soft start and bounce read wait for the anim job.
Not ready for the owner as a set until fix 1 is in; the rolls alone are ready.
