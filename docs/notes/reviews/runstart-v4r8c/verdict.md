# v4r8c run start: VERDICT FAIL
Basis: grid_az90, grid_game, mp4 sheet + az90 crop v92-v134 (step 2), per-frame feet in metrics_v4r8c.json vs metrics_v4r7.json. grid_az0, Luigi ref and Cascadeur png not opened.

Traits
1. Reads as run cycle sped up, cadence slowing: KO
2. Feet planted on contact: OK for skating (no treadmill slide); "short scrambling steps" absent
3. Arms/lean as run, no pop at rate=1.0 or hand-off: arms/lean OK (chest height, brute lean kept), rate=1.0 point (v101-v105) OK, hand-off KO

FAIL items
- F1 [dati montage] v82-v100: rev-up hidden by the stock 0.3 s blend-in. Clip time is 0.150 at v91 (so it starts ~v82), weight 0.23 at v91, 0.68 at v95, 0.99 at v100: the whole 0.27 s rev-up plays under partial weight. On screen: ONE small L step (ankle z 40 at v90, down at v98), then R knee hanging high v94-v108 (z 54-60, ~0.23 s); R swing lasts v93-v113 (0.33 s) vs ~0.25 s in v4r7. Early cadence is not faster than v4r7: no frantic revving, reads as a hop plus a held knee.
- F2 [dati montage] v116-v134: clip blends out at v116-v129 (v4r7: v155-v175), mid-stride and out of phase with the loop. v122-v134 neither ankle goes below z 19 (floor contact = ~10): L bottoms at z 21 (v128) and rises again, R swings low at z 24-28. Az90 crop v126-v130: legs gathered under an upright body, a floating half-step where v4r7 has a full planted stride (R z 10-12 at v124-v128). Brief says "then v4r7's normal strides 1:1"; that is not what plays.
- F3 [dati montage, minor] v93: L foot X pops 5.1 -> -5.6 -> 10.0 cm in one frame (in air, clip weight 0.45); absent in v4r7.

OK / not defects
- Planted-foot skating: L contact v98-v104 moves 5.5 cm, R contact v114-v118 moves 7.6 cm (v4r7 L v113-v118: about 5 cm). Not the old 1/3-cell skate. The 48.7 cm summary figure is probably inflated by the F2 floating window (not verified).
- Root motion: actor_x identical to v4r7 frame by frame (795.6 at f200); no amplitude/speed reduction. Dust puff and capsule coherent.
- Life: no glitch/fast-forward look, but only because the fast part is not visible. Weight and lean fine v104-v116.

What the owner will notice: no Mario-style scramble at the start (one quick step, then a hanging knee), then a soft floaty stutter step about 0.6-0.8 s in where the run should already be stomping.

Fix direction (no amplitude cut): shorten the montage blend-in to ~0.05-0.08 s or start the rev-up after the blend; restore clip length / blend-out to v4r7's so the hand-off is phase-matched.
