# VFX v2 round 1: visual review (Fable, rule 10)
2026-11-18. Judged on 8 grids of `docs/notes/reviews/vfx-v2/r1/`, each looked at once: (1) before_after_hit_large_close_22-33, (2) before_after_jump_side_80-91, (3) hit_large_side_22-51, (4) hit_large_side_186-231, (5) ground_bounce_side_22-69, (6) wall_side_40-87, (7) run_skid_side_18-41, (8) sprint_game_200-247. Rules: `vfx-stile-v2-astrobot.md` a)-h), `stile-sheet-vfx-review.md` D1-D21. Not seen: hit S/M, ceiling, wall bump, skid, jump take-off.

## Verdict: FAIL
The direction is right: gems, neon torus and wall torus read as lit 3D toys, not stickers, and the hitstop slowdown works. It fails on 5 VFX points: white pieces turn grey, the hit ring sits on the attacker and peaks late, the shards are an unreadable clump over the bodies, shadows read black, and bounces could not be shown by these captures.

## Brief checks a)-h)
- a) 3D glossy object at mid life: YES for gems, neon ring, wall torus (3 v28-v32; 6 v44-v52). Dust balls too small to tell (7 v30-v41).
- b) Takes scene light without going grey/black: NO, colore/materiale. Landing ring and balls (2 v85-v91 ON vs OFF), knockdown ring and balls (4 v214-v230), run dust (7 v29-v41), trail balls on the black sky (4 v186-v212) are mid grey with a dark underside; r4 was clean white. Only one lighting spot captured.
- c) Flash frame 1 biggest and brightest, no halo: NO, timing. Side view the flash is right (3 v25). From the game camera it is a dull cream smudge on the torso (1 v25 ON) and the ring at v32-v33 is larger and brighter than frame 1. No translucent halo seen.
- d) Hitstop slowdown and restart: YES (3 v26-v32 near still, moving at v33 with the launch; 5 v36-v44, release v45).
- e) Bounces: NOT JUDGEABLE. The camera follows the victim and the debris leaves the frame (3 v38-v41); landing balls show no visible hop (2 v85-v91). No endless sliding seen.
- f) Shadows of airborne effects readable: YES for position (3 v34-v41; 4 v192-v212; 6 v40-v55), but they are black, see D8. Behind / 3-4 view not captured.
- g) Same shape on every kit: NOT JUDGEABLE (one kit).
- h) Before/after vs r4: done on (1) and (2). Better: volume, hit no longer a sticker. Worse: landing weaker and greyer than r4 (2 v85-v91), shards less countable than the r4 stars (1 v28-v33).

## Checklist D
1. Body readable: NO, forma. Ring frames the attacker's torso for 8+ frames (1 v26-v33; 3 v26-v36); gems cover the victim's chest and head (3 v26-v32; 5 v36-v44).
2. Vocabulary shapes only: YES (all grids).
3. Countable at a glance: NO, forma. Hit shards overlap in one gold clump (1 v26-v33; 3 v26-v32).
4. Frame 1 present and readable: YES for the hit (3 v25). Run start NOT JUDGEABLE (event frame unknown; dust first seen 7 v29).
5. Hit frame 1 biggest, over in 20 frames: NO, timing. Ring keeps growing to v32-v33 (1). Duration is fine (3 v25-v37; 5 v35-v50).
6. Death by shrink, no halo: YES (ring 5 v46-v51).
7. Motion brakes: YES for shards (3 v26-v33). Dust NOT JUDGEABLE at this size.
8. Colours: NO, colore. Grey dust/rings (see b). Shadows black, not dark blue (3 v34-v41; 6 v40-v55). Gems read ochre, no white-yellow-orange steps (1 v26-v33).
9. Three hit sizes: NOT JUDGEABLE (only L seen).
10. Glossy 3D at mid life: YES (same as a).
11. Toad reference: NOT JUDGEABLE (reference not opened; skid not in the grid). Run dust is 2-3 balls of about 10 px (7 v30-v41), likely too small.
12. Generic Unreal look: YES, it does not look generic.
13. Hierarchy: NO, gerarchia. Landing and knockdown are weaker than r4 and the knockdown ring is hidden under the body (4 v214-v230); it should be a heavy landing. The hit itself dominates correctly.
14. Two backgrounds: NO, colore (grey on both blue floor and black sky, see b). Second hub spot not captured.
15. Honest range: NOT JUDGEABLE (no hitbox debug).
16. Depth: NO, forma. From behind the ring marks the attacker's back, not the contact (1 v26-v33); no ground mark under the impact. Shadows along the launch do help (4 v192-v212).
17. Far camera: NOT JUDGEABLE.
18. Hitstop/tremor: slowdown YES (d); tremor NOT JUDGEABLE on grids.
19. Kit coherence: NOT JUDGEABLE.
20. Start: NOT JUDGEABLE (pose change around 7 v26-v27, input frame unknown).
21. Bounces: NOT JUDGEABLE (see e).
Sprint lines (8): white capsules at the sides only, never in front: fine; very small from the game camera.

## Fixes (VFX)
1. colore/materiale, grey whites. `setup_presentation_vfx_toy.py` LOOKS "Dust" and "Gloss": add a flat fill, EmissiveAmount 0 -> about 0.3 in DUST, RimStrength 0.25/0.35 -> 0.6, RimPower 3 -> 2. Target: shadow side no darker than #D9C7B0.
2. forma, ring on the attacker. `hit_spark` Ring and Flash offset (-30, 0, 0) -> (+10, 0, 0) so they sit on the victim side of the contact; ring size 105 -> about 85.
3. timing, ring peaks late. LOOKS "Neon": StartScale 0.35 -> 0.85, GrowEnd 0.6 -> 0.15. Flash particle size 83 -> about 110 and check it is not occluded by the body from the game camera (1 v25).
4. forma, shard clump. `hit_spark` Shards: place them at regular angles on the fan (jitter up to 10 degrees) instead of a random cone; size (24, 28) -> (18, 22); `toward_camera(60)` -> about 30 so they stop stacking on the bodies; make the white start visible (Gem MidTime 0.15 -> 0.3).
5. colore, black shadows. `build_shadow` ShadowColor (0.035, 0.05, 0.12) -> about (0.03, 0.08, 0.32): match the character's own blue shadow on the floor.
6. gerarchia, weak landing. `land_ring` Puffs size (14, 18) -> (20, 26), ring 80 -> 95; knockdown must show outside the body silhouette (4 v214-v230). Same sizes for `impact` Puffs.
7. forma, run dust. `run_start_dust` Big (16, 20) -> (22, 28), Small (12, 13) -> (15, 17), after fix 1.
8. Captures for r2: a fixed camera on the hit point for 60 frames (bounces, e/D21), one spot in light and one in shade (b/D14), hit S/M/L side by side (D9), jump take-off.

## Game or capture problems (not VFX)
- Close game camera: the victim is fully hidden behind the attacker (1, all frames), so the hit location cannot be read whatever the effect does. Camera or test setup.
- The victim hangs motionless in the air and the trail balls beside it do not shrink for 30+ frames (6 v56-v87; 5 v54-v69). If the replay is paused there, it is the capture; if not, check that the Niagara time scale goes back to 1 after hitstop (trail life is 0.3 s).
- v2 hit lands one frame earlier than r4 (v25 vs v26): noted, not a defect.
- Debug markers (dashed drop line, floor circle) are in every side capture and sit on top of ground effects.
