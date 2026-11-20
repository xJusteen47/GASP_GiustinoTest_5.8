# Run start v3 (Overlord) - review Fable r1 - 2026-11-20

Verdict: **FAIL** (the stomp half is good, the "frantic" half and the hand-off are not there yet).

Evidence: 6 grids (stock top, v3 bottom), each looked at once, plus metrics_summary.json after the eye pass.
Limits of this review: cells are ~190 px wide, the character ~80 px tall (az90/az0) and ~30 px tall (game camera); the run
grids sample every 4 frames, so cadence is an estimate (+-1 step/s) and small pops between samples cannot be seen. No video.

## 1. Owner criteria

| Criterion | Result | What I see |
|---|---|---|
| (a) Frantic "engine revving" | **FAIL** | f87-99: ONE big knee lift + stomp (right leg, torso twist), then a low crouch f99-108. That is a wind-up, not a rev: no body part repeats fast. f120-160: legs cycle a bit faster than stock (a back-leg extreme about every 12 frames vs ~16 in stock, so ~5 vs ~3.75 steps/s, by eye) but with normal run amplitude and arms tucked against the chest. It reads as "a quick run", not legs spinning / arms pumping. No wheel-spin: dust and travel start together at f111 exactly as in stock. |
| (a) Readable at game camera | **FAIL** | In grid_game (both phases) v3 and stock are indistinguishable apart from the stance at f93-105. The frantic part changes nothing in the silhouette at that size. |
| (b) Brute, not athlete | **PARTIAL** | f87-117 PASS: low centre, torso well forward, heavy stomp, crouch. f136-156 FAIL: torso comes up almost vertical (stock leans MORE than v3 at f128-140), elbows in, fists at chest, narrow from the front: sprinter posture, the same fault as the old run stop. |
| Style bar (Ganondorf in Astro Bot) | **FAIL** | Snap is there on the stomp; no overshoot or late hands visible in f120-160, no squash on contacts. |

## 2. Identity traits (brief)

| Trait | Result | Frames / note |
|---|---|---|
| 1 Frantic limbs: ~5 steps/s | PASS (weak, estimate) | f120-160, about 12 frames per step |
| 1 Arms pumping wide, swing >= run | **FAIL** | f120-160 az0: elbows stay inside the shoulder line, fists never leave the chest area |
| 1 Knees driving high | **FAIL** | f124-156 az90: thigh never reaches horizontal; only the f93 stomp lift does |
| 1 "Scrambling" silhouette from game camera | **FAIL** | grid_game f120-160 |
| 2 Pelvis drop 4-6 cm per contact | **FAIL** (not visible) | f120-160 head/pelvis line is flat; only f96-105 is low |
| 2 Flat stomping feet | PASS | f96-99, f111-114 |
| 2 Shoulders hunched, torso +10-12 deg vs run, head low | PASS f99-124, **FAIL** f136-156 | torso rises too early, well before the hand-off |
| 3 Short and clean, no walk-in | PASS | run pose from f108-111; stamp f87-108 matches timing B (21 frames) |
| 3 Hand-off with no pop | **FAIL** (data) | see 4 |

## 3. Life

| Item | Result | Note |
|---|---|---|
| Anticipation | PASS | f87-99 lift + stomp + crouch is clear and heavy; best part of the clip |
| Arcs / follow-through | FAIL | arms and head move with the torso in f120-160, no lag, no overshoot at the arm swaps |
| Weight | PASS f87-117, FAIL f120-160 | no vertical bounce once it runs |
| Root vs capsule | PASS | actor x at f200 identical to stock (784.5), poses lean with the travel, no moonwalk seen |
| Foot skate | PARTIAL | by eye nothing obvious at this size; metrics: L 26.2 cm (better than stock 40.5), R 73.3 cm (stock 79.1). The right foot still slides a lot, same as stock, so it is not a v3 regression |

## 4. Technical

| Item | Result | Note |
|---|---|---|
| Blend-in | **FAIL** | metrics v3 weight: 0.81 at f86, falls to 0.50 at f90, back to 1.0 only at f99. The clip does NOT own the pose at 1.0 from f86 as the capture gate says: the knee lift (f87-96) is played half-blended with something else |
| Hand-off | **FAIL** | weight 1.0 f170, 0.94 f171, 0.78 f172, 0 f173: a cut from 0.78 to 0 in one frame. Brief asks a 0.1-0.15 s blend (6-9 frames). Not visible in the 4-frame grid (f172 and f176 look similar), so it may be small, but it is a pop by construction |
| Interpenetration | PASS | none seen; arms close to the chest at f99-105 but not through it |

## Fix list (none reduces amplitude, speed or root motion)

| # | Label | Frames | Fix |
|---|---|---|---|
| 1 | **posa** | f120-160 (clip ~f11-23 at 30 fps) | Make the frantic part big enough for the game camera. Arms: elbows OUT 20-25 deg from the ribs, fist travels from behind the hip to chin height each step, hands 1-2 frames late with one overshoot per swap. Legs: thigh to horizontal on each passing pose, heel kicks up behind. Add shoulder counter-rotation +-15 deg around the spine. Keep the cadence. |
| 2 | **posa** | f128-160 | Keep the brute posture to the end: torso 10-12 deg more forward than the run loop until ~f160, head low and leading, shoulders hunched and wide; come up to run lean +1-2 deg only in the last 3-4 frames. Add the 4-6 cm pelvis drop on each contact (ease-in), so the run half has the weight the stomp has. |
| 3 | **dati montage / gioco** | f84-99 and f170-173 | Blend-in: find why the weight dips to 0.5 at f90 (two blends overlapping, probably Idle->RunStart against the Blend Stack blend) and get 1.0 by ~f88. Blend-out: 6-9 frames (0.1-0.15 s) instead of the f172->f173 cut. Also correct the capture gate: it reports weight 1.0 from f86, the data says f99. |
| 4 | **posa** (optional, for criterion a) | f99-108 | The crouch after the stomp is a held pose. Put the "rev" here: 2-3 very fast in-place foot pedals or a fist/shoulder shake while the capsule is still held by StartStamp, so there is wheel-spin BEFORE traction. |
| 5 | **gioco** | f111-176 | Right planted foot travels 73 cm (stock 79): stride warping / sim speed vs clip speed, present in stock too. Separate from v3, track it apart. |

## Owner questions

- "E' della stessa qualita' di un gioco action stilizzato alla Smash Bros?" **No.** The stomp is at that level, the run half is a slightly faster mocap-like run with no cartoon amplitude.
- "E' alla pari di Smash Bros o altri tripla A simili?" **No.** The identity trait of the move (frantic limbs) is not readable at the game camera, and the hand-off cuts.

What already works and must be kept: the f87-99 knee lift and stomp, the low crouch, the forward lean up to f124, left foot planting.

## Correction by the thread (rule 11, checked on Saved/RunStartV3R/a2/mv_run_start_v3/frames.jsonl)
- Fix 3 is withdrawn. f86-99: the "dip" in metrics_summary.json is the raw logged weight; the lean BS are additive on a cached pose
  and the logger gives the base clip max(1-a, a) (exact match f86-99 on the old capture, max deviation 1e-16). The pose is 100% v3
  (capture_assert.py with additive=BS_Relaxed_*Lean*: f86-167 weight 1.0). Hand-off: v3 1.00 f170, 0.94 f171, 0.78 f172, 0.50 f173,
  0.22 f174, 0.06 f175, loop 1.00 f176 = 6-frame blend, not a one-frame cut (metrics_summary lists only f80-175 top-3 entries).
- Verdict stays FAIL on fixes 1-2 (posa) and owner criterion (a).
