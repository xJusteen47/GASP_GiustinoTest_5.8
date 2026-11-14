# react-v3 visual review (rule 10, cloud): first in-game Play captures, 2026-11-14

Material: `docs/notes/reviews/react-v3/<seq>.mp4` from `claude/project-thread-j8zkjb` (0467a790), PIE on L_Test_Hub, clips
played IN PLACE on the character's anim instance (no ability travel: expected). Every video frame of all six sequences
extracted with ffmpeg, cropped per view (right side az90, behind az180, game camera) and looked at as one composite grid per
view (long sequences in two halves), plus zooms on the critical frames. Frame numbers below are VIDEO frames (`v`, 60 fps);
clip frames are `a` in the burned-in label. Evidence crops in `review/` (`hs_pick`, `kd_pop`, `kd_gh_zoom`, `rb_zoom`,
`splat_full25`).

Common to all: hard cut from idle into the first clip at v9→v10 (capture setup, not judged). Roll/RollBack start from a
hard cut to LieUp with a one-frame knees-up blend at v10 (capture setup, not judged).

| Sequence | Verdict | What the video shows | Fix |
|---|---|---|---|
| hitstand | **PASS** | v1 fix is in: from behind the chest is twisted, the left arm is up and out, the right (trailing) arm hangs low; asymmetric, reads as struck, not "startled". Snap in v9→v12 (3 f), good for hitstun. Side: head back, rear foot far back with heel up, no floor penetration. | Minor: from the side the lead arm is straight forward at shoulder height and reads a bit like "reaching". Bend the elbow 30-40° and bring the hand toward face level. Optional, not blocking. |
| tumble | **PASS** | Arched supine, horizontal, arms spread, knees hanging; reads as launched backward from all three views. Blend v10→v12 pivots the body up around the feet (in place, fine in the air). | See knockdown_getup: Tumble's body sits at standing pelvis height (~90 cm) because the clip is in place. |
| splat (wall) | **PASS (conditional)** | Side view: head and upper back on the wall plane, right arm up along the wall, knees bent, feet ~40 cm off the wall; reads as "slammed and sliding". Behind: arms spread, wide bent stance. No visible wall penetration at this resolution (back of head and raised hand are flush with the wall edge at v25). **The game-camera view is fully blocked by the wall** (camera behind the victim), and the wall is not drawn from az180. | (1) Capture the view that matters in game: the ATTACKER's camera, i.e. from in front of the victim. (2) Pelvis is ~10-15 cm off the wall while shoulders touch: move the pelvis back onto the wall plane for a cleaner "splat" (or keep it as an intentional slide-down read). (3) Check head and raised hand for 2-5 cm wall intersection in the floor/wall report. |
| knockdown_getup | **FAIL (sequence, not clips)** | GroundHit: lands v24, small bounce (knees/hips lift v26-v29, torso stays down), settles by v37: matches owner rule "ground hit, small bounce, lie". GetUp (v55-v78): sit up, hand plant, squat, rise in place: matches "A = stand in place". **Pop 1: v38** (`montGroundHit16`, no clip frame): GroundHit montage already ended and the game pose (crouched standing) shows for ONE frame, then v39 LieUp starts with a leg kicked up (blend from that crouch). Very visible in all three views. **Pop 2: v23→v24**: Tumble body at ~90 cm drops to the floor in one frame when GroundHit starts (in place: in game the same drop happens when the capsule lands). | (1) No frame without a montage between GroundHit and LieUp: chain LieUp as the next montage section of `AM_React_GroundHit` (or disable auto blend-out and hold the last frame until LieUp starts in the same tick). If the gap is only the capture script starting LieUp one tick late, fix the script, but the game state machine must avoid the same gap. (2) Tumble→GroundHit: author GroundHit f0-f2 starting at Tumble pelvis height and falling to the floor, or start GroundHit a few frames before touchdown, or lower the body via the shrinking capsule (ECB, already queued). Keep the 4/60 blend. |
| roll (forward rock-up) | **PASS** | Rock back with legs up v26-v30 (legs reach about vertical over the hips at v28-v29 from behind, 2-3 f), sit-up and hand push v31-v34, feet planted v35, squat v36-v39, rise to standing v47. Clean, no pops, no floor penetration seen. Owner already OK'd the rock-up. | From behind, the legs pointing straight up at v28-v29 could read as a back roll for an instant: keep the knees bent more (tuck) at clip f3-f4. Travel curve: see `roll-travel.md`. |
| rollback (over the right shoulder) | **PASS** | react-v2 FAIL is fixed: legs go up v27-v29 then pass to the image right (the character's right) v30-v32, hips go over the right shoulder, both hands planted beside the head v31-v33, head turned and off the floor from v31, lands on hands and knees v34-v36, rises to stand v47. From behind it no longer reads as standing on the neck. | Weak spot: v27-v29 (clip f2-f4) the legs are still vertical over the body's centre before tipping right. Start the diagonal earlier: pelvis roll ~15-20° already at f3 so the feet head for the right shoulder from the first frame. Travel curve: see `roll-travel.md`. |

## Game-side crouch after the montage (known, NOT a clip FAIL)

Confirmed, and it is **not only after lying montages**:

| After | Clip ends standing | Hunched crouch | Back to upright |
|---|---|---|---|
| HitStand (standing) | v41 | v43-v48 deepest, knees deep, chest over the knees | still crouched at v54 (video end) |
| Tumble | v41 | v44-v49 deep squat | still crouched at v49 (video end) |
| Splat | v41 | **none**: upright idle from v44 | – |
| GetUp | ~v78 | v81-v100, shoulders hunched, knees bent | ~v100+ |
| Roll / RollBack | v47 | v52-v66 | ~v68-v70 |
| GroundHit end (gap frame) | v38 | the 1-frame game pose is already the crouch | – |

So the crouch follows a standing hit pose too (HitStand), not only a montage that started lying down; Splat (also bent knees,
but no in-air/landing context) is the only one without it. Lead for the code thread: the ABP/motion matching after the
montage picks a landing/crouch pose (Tumble and HitStand both run while hitstun or air states are on); compare with a plain
jab montage, and check what movement state/tag is active on the frame the montage ends.

## Summary

PASS: hitstand, tumble, roll, rollback; PASS conditional: splat (needs the attacker-side camera); FAIL: knockdown_getup
sequence (1-frame pop between GroundHit and LieUp at v38, 90 cm drop Tumble→GroundHit at v24), the clips themselves are fine.
Next: chain GroundHit→LieUp, fix the Tumble→GroundHit drop, recapture splat from the front, then owner.
Cost: one cloud thread, about 60k tokens of context (Opus), ~$3.
