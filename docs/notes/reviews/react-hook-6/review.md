# react-hook-6 visual review (rule 10, cloud): the two hits from behind after the hit-turn fix, 2026-11-14

Material: `docs/notes/reviews/react-hook-6/<seq>.mp4` on `claude/project-thread-a5idak` `9d92bfec` (code `08338b8d`). All 358 video
frames of the two sequences extracted with ffmpeg. Consecutive frames looked at around the hit, the hitstop, the launch, the landing
and the get-up, plus the 70 frames after each reaction ends, in both views. States, positions and yaw from `<seq>_frames.jsonl`.
Frame numbers are VIDEO frames (`v`, 60 fps). Base: `react-hook-4/review.md` (`claude/project-thread-7bhwol` `31d2f686`); its
first-frame captures were extracted again and put next to the new ones. Evidence in `review/`: `review_az90.jpg`, `review_az45.jpg`
(one composite per view), `hit_and_landing_az90.jpg`, `first_frame_vs_hook4.jpg`, `hold_drift.jpg`.

Screen directions: attacker off screen on the LEFT in az90, launch goes right. Before the hit the victim faces right (back to the
attacker) and az45 shows its chest; after the turn it faces left and az45 shows its back.

**Result: both sequences PASS as the temporary fix. The three FAIL points of react-hook-4 are gone: the victim is turned on the hit
frame, the hitstand turns too (without pushback), and the facing holds to the end of the capture. Two small new side effects of the
instant turn are noted below; neither blocks the owner.**

| Sequence | Verdict | Why |
|---|---|---|
| back_hitstand | **PASS** | v35 idle, v36 recoil already turned to the attacker and leaning AWAY from them. Held to v71, facing kept to v142. |
| back_knockdown | **PASS, one note** | v28 first Tumble frame already turning, v29 fully turned; no flip at launch any more; landing clean; facing kept through get-up to v214. The first frame is an in-between (see note 1). |

## What the video shows

| Moment | Verdict | Frames |
|---|---|---|
| Hitstand, first reaction frame | **PASS** | v35 upright facing right. **v36 (Hitstun, yaw 180): recoil pose, head back to the right, arms out to the left, az45 shows the back.** It is the mirror of react-hook-4 v24 (same clip, unturned). Clear change on the first frame, no blend. |
| Hitstand, pose | **PASS** | The front-recoil clip now leans away from the attacker, which was the react-hook-4 FAIL. No pushback in this hit (x stays 206.5), so the new "turn without knockback" path is what was exercised. |
| Hitstand, hold | **Note 2** | v37-v71 held, upper body still. The feet keep creeping for about 20 frames (v38-v57, about 12 cm, strongest v42-v44), then still. react-hook-4 hold frames were identical from the second frame. |
| Hitstand, after the stun | **PASS (facing), known crouch** | Free v72, deep crouch v73-v85, upright about v100 (Anim BP, known). Back to the az45 camera on every frame to v142: facing held 70 frames. |
| Knockdown, first reaction frame | **PASS, note 1** | v27 upright. **v28 (Tumble, yaw 180 in the data): tucked body seen almost end-on in az90, about a quarter turn short. v29: full profile, feet toward the attacker, head away.** So on screen the turn takes the hit frame plus one. In react-hook-4 it started 7 frames late. |
| Knockdown, hitstop | **PASS, note 2** | v29-v35: the right way round for all 7 frames (react-hook-4: head toward the attacker). Not a full freeze: the body sags about 5 cm and the limbs droop a little over the 7 frames. |
| Knockdown, launch | **PASS** | v36 first moved frame, same orientation as v35: the flat propeller flip of react-hook-4 (its v31-v32) is gone. Flight back first, 194 to 415 cm, v36-v66. |
| Landing | **PASS, no regression** | v65, v66 last Tumble frames; v67 (KD+0) GroundHit impact pose at the same pelvis height, v68 flat; descent v68-v72 in steps of about 13, 10, 20, 35 px; lying at v72 (KD+5), still from v79 (KD+12). Same timings as react-hook-4. No frame with the Tumble pose popped up. |
| GroundHit first frame, bounce | **Unchanged (anim)** | Impact pose for one frame, silhouette top up about 30 cm on v67; capsule bounce +18 cm (v67-v78) is not visible as a rebound. Waits for the new GroundHit clip. |
| Lie | **PASS** | v83-v117 identical frames. |
| Get-up | **PASS (facing), soft start unchanged (anim)** | Input v117. v118-v120 almost the lying pose, head lifts v121, shoulders v122 (known, needs the new clip). Sitting v126, crouched v132, standing v138-v143, in place (x 415). |
| After the get-up | **PASS** | Free v144, yaw 180 on all 71 frames to v214, back to the az45 camera throughout. react-hook-4 turned back to yaw 0 within 25 frames. Small hunch v150-v180 (known, Anim BP). |

## Notes for the next code or Anim BP pass (not blocking)

1. **One in-between frame at the snap (knockdown).** On the hit frame the data says yaw 180 but the mesh is drawn about 90 degrees
   short; the next frame is right. The hitstand capture shows almost the full turn on its first frame, so the amount probably depends
   on where the video frame falls between two simulation steps (my guess: the mesh is interpolated between the old and the new
   capsule rotation; not checked in code). At speed it reads as a snap. If a literal one-frame turn is wanted, the smoothing has to
   be skipped on the hit frame.
2. **Slow settle after the snap.** Feet creep about 12 cm during the first 20 hitstun frames; the body sags about 5 cm during the 7
   hitstop frames of the knockdown. react-hook-4 was frozen in both places, so it comes with the instant 180 turn (my guess: leg IK
   or root offset in the Anim BP catching up; not checked). Belongs with the queued Anim BP crouch and drift fix.
3. Still not captured: a hit from behind WITH pushback in hitstand (turn along the knockback). `GSTN_CombatNPTests_HitFacing.cpp`
   names the flags; the tests were not run in this review.
4. `[HitTurn]` debug log lines are still in `GSTN_CombatSimComponent.cpp` and `GSTN_CombatSimVictim.cpp` (grep only).
5. Capture script: in a combined run the second sequence starts with the victim already turned, so the walk-away placement ends at
   382 cm and the hit misses (README). Reset the facing between sequences.

## Summary

PASS: back_hitstand, back_knockdown. Turn on the hit frame, facing kept to the end, landing unchanged, nothing else regressed.
Ready for the owner as the temporary fix until the dedicated back-hit and launch clips.
