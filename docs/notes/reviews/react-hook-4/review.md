# react-hook-4 visual review (rule 10, cloud): landing fix and the two hits from behind, 2026-11-14

Material: `docs/notes/reviews/react-hook-4/<seq>.mp4` on `claude/project-thread-7bhwol` `06481cb2` (code `1b641dca`). All 575 video frames
of the three sequences extracted with ffmpeg; consecutive frames looked at around the hit, the hitstop, the landing and the get-up, plus the
frames after each reaction ends. az90 looked at for every range, az45 for the hit, the hitstand and both landings. States, positions and
yaw from `<seq>_frames.jsonl`; body height measured on the frames (grey pixels, top / centre / bottom). Frame numbers are VIDEO frames
(`v`, 60 fps). Evidence: `review/review_az90.jpg`, `review/review_az45.jpg` (one composite per view). Base: `react-hook-3/review.md`.

**Result: the landing pop is fixed in both views and both knockdowns. The front knockdown can go to the owner. The two hits from
behind FAIL: the turn comes 7 frames late in the knockdown, is undone after the get-up, and never happens in the hitstand.**

| Sequence | Verdict | Why |
|---|---|---|
| knockdown_getup (front) | **PASS** (code under review) | Landing pop gone, reactions no longer one frame late. Two known animation limits remain (get-up soft start, no visible bounce). |
| back_knockdown | **FAIL** (code) | Tumble pose points the wrong way for the 7 hitstop frames, then flips 180 deg in 2 frames; after the get-up the pawn turns its back to the attacker again by itself. |
| back_hitstand | **FAIL** (code + one design choice) | No turn at all: the victim recoils leaning TOWARD the attacker behind it. The turn in hitstand is still not verified. |

## What the video shows

| Moment | Verdict | Frames |
|---|---|---|
| Landing pop (react-hook-3 FAIL) | **PASS, fixed** | Front: v85 last Tumble frame, **v86 (KD+0) already shows the GroundHit clip at the same height**, v87 flat. Back: same at v61 / v62 / v63. No frame with the Tumble pose 85 cm up. Pelvis moves under 10 px between the three frames in both views (it was about 110 px). |
| Pose on the state frame (finding A) | **PASS, fixed** | Tumble v24, Hitstun v24, Knockdown v86 / v62, get-up v137 / v113: the new pose is on screen on the frame the state changes, not one later. |
| Tumble / HitStand snap in | **PASS** | v23 idle, v24 new pose (Tumble knees tucked, full at v25). Clear change on the first frame, no blend. |
| GroundHit, first frame | **Note (anim)** | KD+0 is an impact pose (legs and arms flung up) shown for exactly ONE frame, still 90 cm above the floor; the next frame is flat. The silhouette top goes up about 25 cm for that frame. It was already there in react-hook-3 (its v85). Comes from the clip: the impact pose should sit at floor contact and last 2-3 frames. |
| Descent after the state change | **PASS** | Flat body comes down in 5 frames (front v87-v91, back v63-v67), steps 6, 12, 24, 34 px: reads as a fall, no single jump. |
| Ground hit, small bounce, lie | **Unchanged: no rebound on screen (anim)** | Capsule +18 cm at KD+6 while the mesh is still coming down, so the body only goes down and lies (from v91 / v67). Lying still from v99 / v74. Waits for the new GroundHit clip. |
| Get-up, first frames | **Unchanged: soft start (anim)** | Front: v137, v138, v139 show the lying pose almost unchanged, head lifts v140, torso v141. Back: first change v115, clear at v116. Against the feel rule, known, needs the new clip. Standing v159-v161 / v135-v138, in place. |
| After the get-up | **Note** | Front: hunched, knees bent v165-v195, upright v200. react-hook-3 said upright by v161-v169; here the hunch is visible. Same Anim BP crouch as after the hitstand, smaller. |
| back_knockdown: the turn | **FAIL** | v24-v30 (hitstop, yaw 0): Tumble pose with the **head toward the attacker** and the feet away, 7 held frames. v31 yaw 180 in the data, on screen half turned (az45: feet toward the camera), v32 turned. So the body spins flat like a propeller in 2 frames exactly when it starts to fly. The capture note said 4 frames: it is 1 sim frame, 2 on screen. After v32 the flight is right (back first, 221 cm, 38 frames). |
| back_knockdown: after the get-up | **FAIL** | Free at v139 facing the attacker (yaw 180). Then, with no input, yaw 151 (v141), 117 (v143), 57 (v146), 25 (v150), 3 (v160), 0 (v170): the pawn turns its back to the attacker again in about 25 frames, hunched while it turns. |
| back_hitstand: pose | **FAIL** | v24-v59 HitStand held, identical frames, yaw 0, **x stays 183.4 (no pushback)**. The clip is a front-hit recoil: chest up, torso leaning back. With the back to the attacker this leans toward the attacker, as if hit by nobody from the front. |
| back_hitstand: after the stun | **Known** | v60 free, deep crouch v62-v85, upright v100 (finding B of react-hook-3, Anim BP). |

## The flagged point: no turn in back_hitstand, turn mid-tumble in back_knockdown

Both read wrong, for two different reasons.

- **Why the hitstand does not turn.** `ExecuteReactionMove` (`GSTN_CombatSimVictim.cpp:600`) turns only when the reaction move has a
  horizontal speed. This hit has none (the pawn does not move one cm). In react-hook-3 the slide came from the SECOND hit; here the victim
  stood at 183 cm and only one hit landed. So the capture never exercised the turn in hitstand: **still not verified**.
- **Why the knockdown turns late.** The same call runs when the launch starts, after the hitstop. The pose starts on the hit frame, so the
  pose is 7 frames ahead of the facing.
- **Why it turns back after the get-up (my guess, not checked in code).** `ExecuteSetFacing` sets the Mover facing once; the movement's
  own facing target is still the direction of the walk before the hit (yaw 0), and wins as soon as the pawn is free. A player who walked
  and let go of the stick would see the same.

## Fix list

Code (one small thread):
1. Turn on the hit frame (start of hitstop), not at the launch. The first Tumble frame is then already the right way round and the
   turn hides inside the pose snap.
2. Keep the facing after the reaction: the turn must also move whatever facing target Mover returns to when the pawn is free. Check with
   a capture that runs 60 frames past the get-up.
3. Capture: back_hitstand with a hit that pushes (two hits in range, or a move with pushback), and an automation test that names
   `bTurnBackAlongKnockback` (asked in react-hook-3, still missing).

Owner decision (move feel), one question:
- A hit from behind with NO pushback: (a) turn the victim anyway, away from the attacker, on the hit frame, behind a rules flag
  (recommended: every back hit reads right with the clips we have), or (b) keep the facing and wait for a real back-hit clip.

Animation (already queued, unchanged): new GroundHit clip starting at Tumble height, impact pose at floor contact for 2-3 frames;
GetUp whose first frame already lifts head and shoulders; hitstand / get-up exit crouch (Anim BP or held blend-out).

## Summary

PASS: landing pop fix, pose on the state frame, front knockdown as a whole (with the known animation limits).
FAIL: back_knockdown (late flip, facing lost after the get-up), back_hitstand (no turn, recoil toward the attacker).
The front knockdown is ready for the owner; the hits from behind are not.
