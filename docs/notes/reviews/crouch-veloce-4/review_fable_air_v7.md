# Review Fable: air crouch (crouch-veloce-4, capture air_v7)

**Verdict: FAIL.** The guard arms pass. The legs on screen are not a tuck and do not match the authored IK numbers; the enter is a one-frame pop.

Seen: air_side.png (az90 v46-v80, plus 2x/3x crops of v49-v52 and v61-v63), air_front34.png (az45, plus 3x crop of v62-v66), air_game.png, land_side.png, the v4 side grid, review_fable_v6.md. Not seen: the mp4s. Frame counts are +/- 1 (land_side is step 3, so +/- 3 there). Distances are my estimates from pixels (about 1.8-1.9 cm per px, error about +/- 10 cm), not measurements from the data. The game-camera grid is too small (character about 25 px) to judge the pose.

## Identity traits

1. **Readable tuck: FAIL** (v51-v70, az90 and az45).
   - Rear leg hangs almost straight under the body: knee about 45 cm below the pelvis, foot about [+10, ?, -85]. Authored target was 26-30 cm under.
   - Front leg: thigh about 45 deg down-forward, knee about [+35, ?, -30], shin continues forward, foot about [+60..70, ?, -50]. In az45 (v54-v70) this leg is also splayed outward to the side.
   - Torso is upright or leaning slightly back; no curl over the knees.
   - Net: the silhouette is taller than the fall pose at v46-v50 (which has both knees bent), so pressing crouch in the air makes the character look longer, not more folded. It is the same read as v4 (one leg hanging, one foot kicked forward), only the foot is lower.
2. **Guard arms like the ground crouch: PASS** (v51-v70). Both fists in front of the lower face, elbows outside the torso in az45, lead fist slightly forward in az90. Matches the v6 ground guard and holds steady for the whole hold.
3. **Quick enter with follow-through, no pop on release: FAIL on enter, narrow pass on release.**
   - Enter: v50 is the full fall pose (chest leaning forward, arms out, both legs trailing); v51 (a1) is already the final pose: chest upright, fists at face, legs swapped. v52 equals v61. No 9-frame enter, no overshoot at frame 3, no arm lag visible.
   - Release: v70 hold, v71 arms drop to the waist, v72 legs back to the fall pose, v73 onward normal fall. 2 frames, arms lead legs: acceptable, a little abrupt.

## FAIL items

1. **dati montage (or gioco: check first), v51-v70: the authored tuck is not what plays.** The stated IK targets (feet 26-30 cm under, 0-4 cm forward, knees at pelvis height) are off from the screen by 40-60 cm, which is too much for a tuning error. Compare the clip in the editor preview with the game at v60. If the preview shows the tuck, the leg pose is lost in the game (wrong chooser row / blend stack state, legs taken from the fall layer, or IK targets read in the wrong space) and the item is gioco. If the preview shows the same splayed legs, the IK-to-FK bake is wrong (targets not relative to the pelvis, or pole vectors).
2. **posa, v51-v70: target as it must read on screen** (cm relative to the pelvis, [forward, right, up]):
   - Left foot [+8, -12, -30], right foot [+4, +12, -34]. Both shins point down or slightly back under the thighs, never forward of the knee.
   - Left knee [+38, -14, -2], right knee [+34, +14, -18] (keeps the 18 cm asymmetry). Knees no wider than the shoulders: in az45 pull the front knee in by about 15 cm.
   - Spine curled forward 15-20 deg in total (spread over 3 spine bones), chin down 5 deg, so the fists sit just above the knees with a 10-15 cm gap.
   - Check: total height head-top to lowest foot must be at most 70% of the fall pose at v50 (now it is about 110%).
3. **dati montage, v50-v52: enter pop.** Blend in over 3 frames (0.05 s) instead of 1, and make the enter clip actually play: knees rise first (frames 1-3), overshoot at frame 3 with feet 4-5 cm higher than the hold and spine 5 deg more curled, fists arrive 2 frames after the knees (frames 3-5), settle by frame 7-9.
4. **dati montage / gioco, land_side v76-v82: guard breaks on landing.** v76 (a26) is still the air crouch; v79 is a near-standing landing pose with the fists dropped to waist height; v82 is the ground crouch with the guard back. With crouch held, the arms must stay in guard through the touchdown: keep the upper body (spine_03 up) on the crouch pose for those frames, or blend air crouch to ground crouch directly in 3-4 frames.

## Game items (gioco, not counted in the verdict)

- v50 to v51: the whole mesh jumps up about 25-30 cm in one frame (head from y=118 to y=105 px, red disc fixed), and drops back at v71-v72. This is the capsule resize with crouch in the air. It makes the enter pop look worse; smooth the mesh offset over 4-6 frames or shrink the capsule from the bottom in the air.
- air_game v70-v74: camera horizon shifts on release, same known camera item as the ground crouch.

## Keep

Guard arms (fist height, elbows out, lead fist forward), head tucked behind the fists, steady hold v53-v70 with no jitter, release order (arms first, legs second), ground crouch after landing v85-v130 (matches the passed v6), landing ring and dust timing.
