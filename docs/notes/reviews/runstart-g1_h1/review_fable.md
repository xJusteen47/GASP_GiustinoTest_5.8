FAIL (close: entry and hand-off are good, the rev-up itself does not yet read as "engine revving")

Evidence: 3 grids, Cascadeur sheet, metrics_summary.json, 8 video frames (v90 v98 v104 v110 v116 v124 v134 v150). Stills only:
timing and cadence are inferred from poses, not watched.

## 1. Life
- OK, entry, v84-v98. g1 is already crouched and leaning at v88-v90 while the base still stands until v96. No planted pause,
  no visible pop from idle, lean about 30 deg at v98 with the dust puff: reads as a push-off. Shadow stays under the body
  in all 8 frames, root coherent with the capsule.
- FAIL posa, v104-v112: vertical hop instead of drive. At v110 both feet are off the ground, both knees bent, torso almost
  upright (it was 30 deg forward at v98, and is forward again at v116). The loop's flight phase played with a short stride
  becomes a skip in place, and the torso pitch flips forward / upright / forward in 0.3 s. Reads as fast-forward, not weight.
  Fix: during the rev, remove the loop's pelvis vertical bounce (scale pelvis Z to about 0.3) and hold an additive forward
  lean of 20-25 deg that decays to 0 over the 16 rev frames, so the body stays low and falling forward while the legs churn.
- FAIL posa, v98-v112: arms do not pump. In every rev frame the elbows stay at about 90 deg with the hands at the chest,
  same amplitude as the relaxed loop; at 3.2x that is a small jitter. Fix: arm swing amplitude x1.6-1.8 (shoulder pitch)
  during the rev, decaying to x1 with the rate; elbows a little more open on the back swing.
- WARN posa, v116: the stride is already loop-size, so the visible rev is about v90-v112 (0.37 s, two to three steps) and is
  half hidden by the dust puff. Short for a signature beat. Fix: rev 20-22 clip frames instead of 16, R0 3.2 -> about 2.6
  so individual steps stay readable, and keep the stride floor low until the last 4 frames (ease out, not linear).
- WARN posa, v104 (az0 and az180): the raised thigh swings out to the side (knee splayed, shin crossing) instead of
  straight ahead. May be the lean layer or perspective; check hip abduction on clip frames 2-6 in Cascadeur, clamp to the
  loop's value.

## 2. Identity traits
1. Frantic rev-up from frame 1: HALF. Legs yes (high knee at v104, thigh horizontal), from frame 1 yes, arms no (above).
2. Engine revving, short quick steps settling into the stride: HALF. High knees are there, but v110 is a hop and the
   settle is abrupt (short step at v110, full stride at v116). Same fixes as Life.
3. No clash: OK. Idle -> start clean (v84-v98). Start -> loop: v124, v134, v150 are stock loop poses, v134 and v150 match
   (same foot forward, 16 frames apart), no hitch or wrong-foot step seen in the frames sampled.

## 3. Feet vs ground
- WARN posa, f80-240: planted-feet travel L/R 96.8/65.0 cm (base 43.3/93.5), total +18%, left foot more than doubled.
  Some spin is intended and the dust covers it, but it sits on one foot. Fix: raise the stride floor slightly on the first
  left plant, or start the rev on the other phase so the slide splits evenly; target each foot under about 80 cm.

## 4. Technical
- FAIL dati montage: metrics_summary.json reports clip_weight_f80_210 = 0 on every sample for g1 (and base), and
  ss180_max_w 0. The brief claims weight >= 0.8 on f97-140 from assert.txt, which this summary cannot confirm: the
  summary is reading the wrong curve/asset name or the field is not filled. Fix the summary export so the weight series
  matches the assert.
- OK gioco: actor x at f200 960.4 vs 1017.0 cm (base), 57 cm behind. Not a problem by the owner's rule (animation, not speed).

## To pass
Low body with held lean, bigger arms, 4-6 more rev frames at a lower R0, then re-capture with the weight series filled in.
