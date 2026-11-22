# runstart-v3-pose-1: Overlord run start, pose rounds 1-2 after review FAIL (desktop, 2026-11-20)
Branch claude/runstart-v3-pose-1 (from claude/runstart-v3-review-1 c5797b02). Lock _ue59 freed at the end. Re-check (rule 11):
assert_base0_a2.txt of the previous thread read, ASSERT: OK f86-167 as stated.

## Results
- Base with tail: runstart_compose_casc_v4.py (T_END 68) -> RunStart_v4base.casc, 69 f at 30 fps = 1.5 s authored + 0.75 s stock loop
  tail (coordinator's applied decision; Unreal Assistant: the exit threshold is in the transition-rule graph, a tail is the stock-compatible
  way; Saved/Assistant/replies.json). Divergence 12 in gasp-divergenze.md (tail + Pose Search Block Transition notify from 0.05 s).
- Pose (Cascadeur, runstart_restyle_casc_v4r1.py, gate OK, stomp f0-11 drift 0.001 cm): arms pump on an arc about the shoulder hip->chin
  through a lagging spring (overshoot), elbows out (pole), arm rotations from arm-plane frames (rot_between flipped the roll 180 deg);
  knee drive to ~88 deg (FK about the hip); +18 deg lean, head -14, hunch, +-15 deg shoulder counter-rotation; -4 cm pelvis dip per contact.
- Round 1 AS_GSTN_RunStart_v4r1: ASSERT OK f100-180 (min 0.98); Fable r2 FAIL narrow: torso released in ~8 frames at f166-182.
- Round 2 AS_GSTN_RunStart_v4r2 (release clip f39-59): ASSERT OK f100-180 (min 0.96, assert_base1_a1.txt + hires); Fable r3 **PASS**
  (review_fable_r3.md). Hi-res recapture: planted feet L 49.2 / R 52.1 cm (Fable's 60.0 was the a1 capture spot; metrics in
  Saved/RunStartV4R2/hires_ev/metrics_summary.json via runstart_v4_evidence.py). I watched the 3/4 sheet f88-196 once: no pop seen.
- Base 1 chooser: v4r2 sibling rows enabled, v3 and v4r1 rows disabled (stops_rows_ue.py stock/new, reports Saved/AnimPipeline/rows).
- Owner videos: docs/notes/reviews/runstart-v4r2/owner/runstart_v4r2.mp4 (real time) and _025x.mp4.

## Open (not pose)
1. gioco: on base 1 the row is reselected from Walk_Start at f90 KEEPING its elapsed time: v4r2 enters at 0.15 s, weight 0.15->0.99 over
   f90-100, stomp contact ~87 %. Block Transition does not change it (it is the start time the reselect passes, not pose search).
   Code thread: start the row at 0 or shorten that blend.
2. posa, owner proposal: faster cadence of the first 2-3 steps (Fable: frantic now comes from amplitude, not rate).
3. Not verified: clip in isolation (clipcheck), GASP curve list vs ABP, hand lag at 1-frame sampling.

COSTO: ~22 $ | ~14 min desktop | review ~0.21M token
