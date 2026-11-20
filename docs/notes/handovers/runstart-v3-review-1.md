# runstart-v3-review-1: run start v3 Overlord, recapture + Fable review (desktop, 2026-11-20)
Branch claude/runstart-v3-review-1 (from claude/loco-skid-runstart-1). Lock _ue59 freed at the end.

## Results
- Capture gate fixed (6a8f911a): the "leans" dip was a logging artifact. GASP lean BS are additive on a Use Cached Pose; the logger gives
  the base clip max(1-a, a) while the lean alpha ramps (old capture f86-99: exact, deviation 1e-16). capture_assert.py takes
  `additive` patterns (spec key / --additive), test added (tests/test_capture_assert.py ESITO OK). Gate v3 now f86-167 (input f83 + 0.05 s blend).
- Base 1 (clean GASP): v3 as pose-matched sibling rows of M_Relaxed_Run_Start_F_L/Rfoot (cb5ef9d0, divergence 11, rollback RS_ROWS=stock).
  Attempt 1 ASSERT FAIL 68% (assert_base1_a1.txt): stock picks Walk_Start first and reselects at f90 KEEPING its time, so v3 starts at
  0.15 s blended under walk start (wind-up lost), and "almost complete" (<= 0.75 s left) exits it at ~0.85-1.0 s (frantic tail lost).
- Base 0 (our ABP, divergence 9): attempt 2 ASSERT OK, v3 f86-167 100% min 1.00, loop f172-210 92% (assert_base0_a2.txt). Evidence:
  docs/notes/reviews/runstart-v3-review/{stomp,run}/grid_*.png (stock vs v3), runstart_v3_lowres.mp4.
- Fable review r1 (review_fable_r1.md): FAIL. Keep f87-124 (knee lift, stomp, low crouch: brute, heavy). Owner (a) frantic rev-up FAIL;
  (b) brute PASS f87-124, FAIL f136-156 (torso near vertical = sprinter). Smash/AAA: no / no. Its fix 3 withdrawn (see correction).

## Next (pose round 1, posa, Cascadeur via gstn-move-animation; Fable judges poses)
1. f120-160 frantic arms/legs readable at game camera: elbows out 20-25 deg, fist hip -> chin, hands 1-2 frames late + overshoot, thigh
   horizontal at passing, shoulder counter-rotation +-15 deg; optional 2-3 fast pedals in place f99-108 (wheel-spin before traction).
2. f128-160: +10-12 deg lean over the run loop, head low, shoulders hunched/wide, 4-6 cm pelvis drop per contact.
3. Base 1 (if the owner wants v3 on the clean base): clip +0.75 s run-loop tail and v3 rows sampled from time 0 (SamplingRange), else
   it stays truncated. Owner: 1 base 0 route vs base 1 rows (rule 14).
No owner video sent (FAIL, rule 10).

COSTO: ~11 $ | ~10 min desktop | review ~0.09M token
