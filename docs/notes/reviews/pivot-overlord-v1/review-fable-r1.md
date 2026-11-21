# Review Fable r1: Overlord run pivot v1 (wc8ksn)

Source: the 8 grids only (f112-158 step 2, 256 px cells). No video, no code read.
Limits: capsule path is not readable in the game grids (lateral shift under 1-2 px), so "body vs capsule" side is judged
from the body rotation direction in az90 only. Feet are hidden by dust at var_l f126-130 and var_r f126-134.

## Life by eye

| Item | Result | Frames / note |
|---|---|---|
| Anticipation | FAIL (pose) | var f118-122: full run stride goes straight into the turn. Only var_l f120 leans back for one cell. No readable brake pose before the spin. |
| Arc / turn readability | PASS (weak) | var f122 old facing, f124 mid turn, f126 new facing: about 180 deg in 4 frames. Reads as a snap, clean, no broken silhouette. |
| Weight | FAIL (pose) | var f124 (both sides): torso upright, arms open, hips high. Base holds a low crouch f126-146 (base_r sits very deep f128-140). The retime removed the hold that sold the weight. |
| Follow-through | FAIL (pose) | var f128-134 has a good forward drive lean, but f136-146 is already an upright short-stride jog: no settle, no overshoot of torso or arms. |
| Body turn direction (trait 3) | PASS | var_l f124 shows the chest to camera (left turn seen from the left side), var_r f124-126 shows the back (right turn). Opposite and consistent. |
| Root vs capsule | PASS | var faces the new direction from f126-128, capsule minimum at f124, 420 cm/s at f134. Base is still facing the old way or mid turn until about f148 while the capsule already travels back: variant removes that backward glide. |

## Technical

| Item | Result | Frames / note |
|---|---|---|
| Pop at row entry f118-124 | PASS, with note | No head-height jump (head stays level f120-124). Left and right differ at f122: var_l is tucked with a raised knee, var_r is an upright long stride. Likely a different foot phase at entry, not a mirror error. Check on video. |
| Pop at exit (blend out) | PASS | var f130-138: lean to upright run is gradual in both sides. |
| Foot slide | PASS (marked) | f112-122 run cycle at full stride while the capsule decelerates: slide, same in base, before the row, intended brake zone. f136-146 short jog strides at 420+ cm/s: probable light slide after exit, locomotion side, verify on video. |
| Mesh through floor | PASS | None visible. Not judgeable where dust covers the feet (frames above). |
| Mirror errors, left side | PASS | var_l silhouettes f124-134 match var_r mirrored; no crossed limbs or twisted wrists visible at this size. |
| Timing vs trait 1 | PASS | Row entry f118 to body running the new way f128-134: 0.17-0.27 s. Base needs until f150-158 (0.5-0.65 s), base_r is still turning at f158. |
| Game view | PASS | No visible pop or teleport in var_ovl_l_game / var_ovl_r_game; figure too small for more. |

Not judgeable here: latency from stick reversal (f95) to row entry (f118), outside the grid range.
Side note: stock straight reversal (base_az90 f136-146) shows the back, so it turns right, while the variant straight case
is said to pick the left clip. Not a fail, but the straight variant was not in the image set.

## Traits

1. Toad timing: PASS.
2. Brute style: POSE PASS NEEDED (not failing v1). The retimed stock pose reads as a light, athletic jog spin, not a heavy brake.
3. Both sides: PASS on body direction and mirror; capsule side not verifiable from grids.

VERDETTO: PASS

## Trait 2: what the Cascadeur pose pass must add

- f118-122, brake key: hips dropped and back, wide stance, lead foot dug in ahead of the body, torso leaning against the
  travel direction, shoulders hunched. One strong silhouette held 2-3 frames (the low sit of base_r f128-136 is the reference for depth).
- f122-126, turn: keep the hips low through the spin, no rise to upright at f124. Head and chest lead, hips follow, so the
  3.7x section reads as a heavy body dragged round, not a pirouette.
- f124, arms: replace the open, balanced arms with heavy asymmetric ones (lead arm swung across, rear arm trailing, fists low).
- f126-134, push-off: deeper lean into the new direction, long first step with a visible ground shove; this is where the
  cartoon exaggeration should sit, since the timing has no room before the turn.
- f134-146, follow-through: torso and arms overshoot and settle over 4-6 frames, first two strides long and low, so the
  blend to the run does not land on the upright jog seen at f136-146.
- Both sides: redo the mirror bake after the pose pass and recheck f120-122 so left and right enter on the same foot phase.
