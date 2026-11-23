# VFX v2 round 2: visual review (Fable, rule 10)
2026-11-18. Fix attempt 1 of 2 after `review-fable-r1.md`. Judged on 7 grids of `docs/notes/reviews/vfx-v2/r2/`, each looked at once: (1) r1_vs_r2_hit_large_close_22-33, (2) hit_debris_side_20-79, (3) hit_debris_game_20-49, (4) r4_vs_r2_jump_side_80-91, (5) ground_bounce_side_22-69, (6) run_skid_side_18-41, (7) wall_side_40-87. Only the r1 NO items and e)/D21 are re-judged; r1 YES items stand (no regression seen) and r1 NOT JUDGEABLE items not listed here stay so.

## Verdict: FAIL (close)
Most of r1 is fixed: whites are clean, shadows are blue, the ring sits on the contact and is full size at once, the gem colour ramp reads, the landing is as strong as r4, and bounces are now visible and correct. Two VFX items still fail: frame 1 of the hit from the game camera, and impact balls over the victim at the wall.

## Brief checks re-judged
- b) Scene light without grey: YES. Landing ring and balls warm white (4 v86-v91 ON vs r4), trail balls white (2 v36-v48), wall torus and balls white (7 v40-v52), run dust (6 v29-v41). Shaded spot of the hub still not captured.
- c) Flash frame 1 biggest and brightest: NO, materiale. Side view is right: flash larger than the ring, white (2 v26; 5 v35). From the game camera frame 1 is a black square behind the attacker (1 v26 ON), and in the far game view no flash is seen at all, the ring is the first thing visible (3 v26).
- e) Bounces: YES. Debris falls, hops once or twice low, no sinking into the floor, gone by v78, about 0.87 s after the hit (2 v53-v78). Note: the pieces are still drifting sideways when they pop out (2 v70-v78), they never come to rest; acceptable at that size.
- f) Shadows: YES, now dark blue and readable (2 v36-v72). One shadow travels toward the bottom of the frame for 30+ frames with no piece visible over it (2 v40-v72): probably a piece outside the frame, check it once.
- h) Before/after: r2 beats r1 on ring place and size, shard readability and colour (1 v27-v33); landing now matches r4 in strength with volume added (4 v86-v91).

## Checklist D re-judged
1. Body readable: NO, forma, for the wall only. The bigger impact balls sit on the victim's legs and hips for 3-4 frames (7 v40-v43). Hit: fixed, shards are above and beside the victim and the ring is a thin hoop on the contact (2 v27-v33; 5 v36-v44).
3. Countable: YES, marginal. 6-7 separate gems in the strong hit (2 v27-v32); in the ground bounce they still stack into one pile of 4-5 (5 v36-v44). The regular fan (construction rule, not done) would settle it.
5. Frame 1 biggest, over in 20 frames: NO, timing/materiale. Side: yes for size (2 v26), ring gone at v41. Game camera: see c). Shards fly about 3 m up and are still visible at v45, 20 frames after the hit (2 v34-v45): at the limit.
8. Colours: YES. White to yellow to orange on the gems (2 v27, v33, v38), blue shadows, clean whites.
13. Hierarchy: YES for landing (4). Knockdown not captured this round: NOT JUDGEABLE.
14. Two backgrounds: YES on blue floor and black sky (2, 7); shaded spot not captured.
16. Depth: YES from the side (ring on the contact, shadows under the debris, 2 v27-v40). From behind it cannot be judged: the victim is hidden by the attacker (1, 3), a camera or test setup matter.
17. Far camera (new): the L hit ring is clearly visible from the far game view (3 v26-v38). Hit S still not captured.
21. Bounces: YES (same as e).

## Remaining fixes (VFX)
1. materiale, black square on frame 1 from the game camera (1 v26 ON). `hit_spark` Flash / `MI_GSTN_VFX_HitFlash`: the quad shows unmasked and black from behind while it is correct from the side. Check the mask and emissive on its first frame at size 110 and whether the body occludes its bright part; if the flash ends up behind the attacker, give the Flash emitter its own `toward_camera` (about 60) so it draws in front like the shards. Re-capture close and far game camera.
2. forma, wall balls on the victim (7 v40-v43). `impact` Puffs: size (20, 26) -> (16, 22) and spawn radius 58 -> about 72, or lower the wall cues' `toward_camera` 30 -> 15. Keep `land_ring` at (20, 26).
3. timing, shards travel too far after the hitstop (2 v34-v45). `hit_spark` Shards: drag 14 -> 20, or speed 3000 -> 2200, so they stay within about 1.5 m and end by frame 18.
4. forma, optional: shard pile in the ground bounce (5 v36-v44). Without the scratch module: Shards spawn sphere radius 4 -> 12 and cone 120 -> 150.
5. forma, optional: run dust still small, 2-3 balls of about 12 px (6 v30-v41). `run_start_dust` Big (16, 20) -> (22, 28), Small (12, 13) -> (15, 17) (r1 fix 7, not applied).

## Game or capture (not VFX)
- Close and far game camera: the victim stands exactly behind the attacker (1, 3). An offset camera or a 3/4 angle is needed to judge c) and D16 from behind.
- Wall: the victim keeps one pose in the air and 5-6 trail balls hang beside it unchanged for 30+ frames while the camera moves (7 v56-v87). Either the replay is paused or the victim is in a long hitstop and its trail runs at 0.15. Say which; if it is the second, cap the slowdown for `NS_GSTN_TumbleTrail`.
- r1 note corrected: in the ground bounce the victim is not frozen, the camera follows it upward (5 v54-v69).
- Captures still missing: hit S/M/L side by side, a shaded spot of L_Test_Hub, knockdown, jump take-off.
