# VFX v2 round 3: visual review (Fable, rule 10)
2026-11-18. Fix attempt 2 of 2 after `review-fable-r2.md`. Judged on 6 grids of `docs/notes/reviews/vfx-v2/r3/`, each looked at once: (1) r2_vs_r3_hit_large_close_22-33, (2) hit_large_game_22-39, (3) hit_debris_side_20-79, (4) r2_vs_r3_wall_side_38-49, (5) ground_bounce_side_22-69, (6) run_skid_side_18-41. Only the r2 NO items are re-judged, plus a regression pass on what had passed.

## Verdict: PASS on everything the captures can show
No NO is left. It is not a full PASS by the letter of the sheet, because several checks were never capturable (list at the end): those go to the owner's playtest, not to another fix round.

## r2 NO items re-judged
- c) Flash frame 1 biggest and brightest: YES. Game camera close: white flash with the ring around the attacker's shoulders, no black square (1 v26 ON vs OFF). Far game camera: white flash clearly visible (2 v26). Side: large white disc, the biggest and brightest frame of the effect (3 v26; 5 v35). No translucent halo.
- D1 Body readable: YES, marginal at the wall. The smaller impact balls no longer blanket the victim's legs; torso, head and arms read on every frame (4 v41-v43 ON vs OFF). The torus still crosses the victim's hips for 2 frames at contact (4 v41-v42), which is where the contact is. Hit: shards form a halo around the contact, off the bodies (1 v27-v32 ON).
- D5 Frame 1 biggest, over in 20 frames: YES. Flash is the largest frame (3 v26), ring gone at v41, last shards gone around v44-v45 (3), 18-19 frames. Two shards still climb about 2 m (3 v34-v43): inside the limit, small by then.
- D3 Countable (marginal in r2): YES. 8 separate gems on a ring-like fan in the game camera (1 v27-v32 ON); the pile in the ground bounce is gone (5 v36-v44).

## The dark flash of r2
Not present in any of the three r3 hit captures (1, 2, 3, 5). The only change was the Flash emitter's own `toward_camera(60)`; the material is the same. A material fault would still show from the side or far view, and it never did, in r2 either. So the black square was most likely the quad drawn behind the attacker and caught on its first frame, or a first-use shader frame in that one capture; I cannot tell these two apart from the grids. Watch for it on the very first hit of a session in the playtest.

## Regression pass
- Ring: on the contact, full size at once, shrinks out (3 v27-v41): unchanged, fine.
- Hitstop slowdown and restart: still correct (3 v27-v33 near still, moving at v34).
- Debris, bounces and shadows: still correct (3 v45-v77). The lone shadow of r2 is explained: it belongs to the piece that flies toward the camera, which pops out in the air before landing (3 v62-v76). Harmless.
- Whites, blue shadows, landing torus on the ground bounce (5 v46-v53): unchanged, fine.
- Run dust: now 3-4 readable cream balls behind the feet (6 v29-v41): better.
- One note, not a NO: during the hitstop the shards stay pale cream and turn yellow-orange only after the release (1 v27-v32, 3 v33-v40), because their colour ramp is slowed with them. The warm read comes from the ring. If the owner finds the hit too pale, lower Gem MidTime 0.3 -> 0.1.

## Game or capture (not VFX), still open
- Both game cameras put the victim exactly behind the attacker (1, 2): from behind, where the hit landed is read only from the ring and flash.
- Wall: the victim and trail balls hanging unchanged for 30+ frames (r2, wall_side v56-v87) was not re-captured. If the victim is in a long hitstop there, the trail runs at 0.15 and its balls last about 2 s: cap the slowdown for `NS_GSTN_TumbleTrail`.
- Ground bounce: the impact torus and balls land around the attacker's shins (5 v47-v52), because the victim bounces at his feet. Placement follows the event; judge in play.

## Still not captured / for the owner's playtest
- g) / D19 same shape on every kit: only one kit seen. Check the accent colour of the debris on a second kit.
- D9 hit S / M / L side by side, and D17 hit S at the far camera (only L was seen far: fine).
- b) / D14 second lighting spot: a shaded area of L_Test_Hub. All captures are on the lit blue floor.
- D11 run start and skid against the Toad .mov; the skid row was never in a grid.
- D13 knockdown after the size changes (last seen in r1, then weak).
- D15 honest range with hitbox debug on.
- D16 / f) from behind and 3/4 with the victim not hidden by the attacker.
- D18 tremor direction and decay; D20 first frame after input.
- Jump take-off ring (owner's rule: ring only, no dust) and sprint lines after r1 (not re-captured; fine in r1).
- Feel in motion: hitstop slowdown at 0.15, debris life under 1 s, first hit of a session (dark flash).
