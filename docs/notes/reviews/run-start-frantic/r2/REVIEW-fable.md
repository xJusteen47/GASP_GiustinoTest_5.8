# Review r2, frantic run start (Fable, 2026-11-19): FAIL on the arms only, r3 is a parameter pass

Judged from grid_side.png, grid_q34.png (stills every 2nd frame, looked once) and README + metrics.json summary. No motion seen. r2 hit every number I asked for. The two problems below come from my r1 spec, not from the authoring.

## Identity traits (checked first)
1. FRANTIC LIMBS: **FAIL** (legs pass, arms fail)
   - Legs PASS: high knees while stamping (f1-f9), short choppy run steps (f13-f25), no bounding stride, heel kick reads.
   - posa: the forward fist sits at the face (f7, f13, f15, f25, f27): a boxer's guard. Author's doubt confirmed.
   - posa: the back swing does not open the silhouette: in the side view the arms stay in front of or on the torso on most frames,
     so the pump is not readable. Cause: with a 30 deg lean, a swing measured from the torso puts +55 at the face and -60 barely behind the back.
   - Chicken wing gone, elbow range fine, fists keyed: keep.
2. HEAVY: **PASS** on weight, with one required fix
   - Low, loaded, flat feet, slow pelvis sink in f1-f9: this is the mass I wanted.
   - posa: the back is a C curve with the head sunk in front of the chest (all frames, worst f7-f15). It reads as a cowering goblin,
     not Ganondorf. Ganon leans from the hips with a broad, nearly straight back. Author's doubt confirmed (I over-specified the hunch).
3. SHORT AND CLEAN: start **PASS**; hand-over still open
   - dati / gioco only (not posa): f27 phase, pelvis height and lean must be read from M_Neutral_Run_Loop_F in the editor.

## r3 spec (change only these; everything not listed stays as r2)
1. Arm swing S is measured from the WORLD vertical in the side plane, not from the torso. Write S_world per frame in metrics.
2. Arm extremes, f1-f10: forward S_world +30, E 90; back S_world -50, E 95.
   f11-f27: forward S_world +35, E 90; back S_world -62, E 95. Sine ease, timing and wrist lag as r2.
3. Arm gates (add to metrics): fist centre always >= 5 cm below the shoulder joint and >= 25 cm from the head centre;
   at each back extreme the elbow is >= 10 cm behind the back surface line (pelvis-to-neck_01) and >= 10 cm below the shoulder (was 12).
4. Abduction 12 deg on the back swing, 6 deg on the forward swing (elbows flare a little going back: wider shoulders, clears the torso).
5. Lean L (pelvis-to-neck_01, as r2): f1 18, f3 21, f6 25, f9 28, f11 27, f13 26, f19 22, f25 17, f26 15, f27 16.
6. Lean split: pelvis pitch 55% of L, spine_01-05 share 45% evenly. Extra flexion on spine_04-05: +3 deg (was +8).
   Gate: the angle between pelvis-to-spine_03 and spine_03-to-neck_01 is <= 12 deg on every frame (straight strong back).
7. Shoulders broad, not rolled in: clavicles 8 deg up, 4 deg forward (was 12 / 10). Head 2 cm ahead of the chest line (was 4-5).
   Head counter 25% of L (was 15%), so the face looks ahead under the brow, not at the floor.
8. Pelvis height: f3 -7 cm vs idle, f6 -9, f9 -10 (was -8 / -10 / -12), ripple -1.5 as r2; f10 -7, f11 -3. Run part as r2.
   With the straighter back the body is taller; check knee interior angle stays inside 60-150.
9. Chest nod and head nod (r2 item 11): keep, but halve them (+1.5 chest, +2 head).
10. f27: unchanged until the editor read. Rules as before: root in place, ankle z >= 0.094, ball z >= 0.

## Process (round 3 of 3)
- r3 is a parameter change on r2's script. If gates 3 and 6 pass in metrics and the author's own look at the grids shows the back fist
  behind the torso line at f14, f20, f26, go straight to the editor import; I review r3 together with the capture, not as a separate pose round.
- In the editor (dati): read the run loop's frame with left foot flat and right knee passing; key f27 to its pelvis height and L + 2 deg,
  ease f24-f27. Then capture (gioco): foot slide with warping, blend-out pop at f27.
