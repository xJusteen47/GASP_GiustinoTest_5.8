# Review r1 - dash-ganon (offline, stick renders + metrics.json) - reviewer: fable

## Verdict: FAIL (close; 2 of 3 traits pass)
Not worth an in-game capture yet: trait 3 fails and two defects are certain from the numbers (20 cm toe slide f22-23,
push-leg pop f8->f9). Fixes 1-4 are required for r2; 5-8 can ride along. Seen: reference grid, grid_side (+2 zooms), grid_front.
Limits: stick figure only (no mesh volume, no hands, no head tilt); mp4 not opened; lateral sizes are read off the front grid (~).

## Identity traits
1. Compact load - PASS (weak). f4-6: pelvis 0.88 -> 0.71, lean 31-38, fists tucked (0.25 m from spine), reads as a ball at f4-5.
   But the drop is 0.17 m, the dossier asks ~1/6 body height (~0.30 m); and the left foot already leaves the ball at f5-6
   (split 0.21/0.33). Shallow for "too much is just right".
2. Lunge - PASS. f9-12: lean 55/53.7/52.3/51, split 0.90 -> 1.02 m, rear leg ~52-55 deg from vertical, moving hold of 4 f, head 2 f
   late (peak 43.7 at f11). Weak points: left arm hangs straight down (not "fist forward-low"), rear knee kinked ~20-25 deg,
   pelvis 0.79 = as high as the run (dossier: lower than the run), and the entry is a pop (fix 3).
3. Heavy settle - FAIL. f16: the left foot lands ~0.37 m AHEAD of the hips on a near-straight leg (not "under the hips").
   Compression is only 2.2 cm (0.767 -> 0.745, f16-18), then +4.1 cm in ONE frame (f18->f19), then the toe slides (f22-23).
   It reads as a light skip, not as weight going through the foot.

## Other findings
- Timing: phases land on the dossier (load 1-6, push 7-8, lunge 9-12, stride 13-15, contact 16). But the fastest lean change of
  the clip is in the load (f3->f4 +13.5 deg), not in the push (f6->f7 +8, f7->f8 +6.8); load lean steps are 9 / 3 / 13.5 = stutter.
- Push: at f8 the right leg is still folded (~100-110 deg knee, thigh vertical) with the foot planted; at f9 it is straight and
  0.15 m in the air. The extension happens in the air; the ball jumps BACKWARD in the world 0.227 -> 0.060 (-17 cm in 1 f).
- Airborne f9-15 with pelvis flat (0.782-0.790-0.767), both feet 0.15-0.21 m up: a hover, no arc, no toe graze.
- Contacts: plants are clean (slide 0.9 / 1.2 cm). f5 right ball_z 0.016 = 1.6 cm into the ground. f22-23 left ball on the
  ground (0.032) but moving 10 cm/f in the world (1.893 -> 1.995 -> 2.096), then going backward f24-25 (1.999, 1.899).
- Front view: narrow from f1 to f25 (ankles ~0.15-0.20 m apart, elbows on the ribs, thighs overlap f7-13). In a 3D brawler seen
  from behind the lean and split are foreshortened; width and drop are what is left, and both are small. Not a brute.
- No knee pops, no stretch (0.0), no arm through the torso seen in the sticks (mesh volume not checked).

## Fixes (by impact)
1. [posa+timing] Catch, f16-22, pelvis_z 0.767/0.760/0.745/0.786/0.800/0.815/0.830 -> 0.765/0.735/0.715/0.715/0.740/0.775/0.800
   (compression ~5 cm, bottom f18-19, rise eased over f20-22, no step above 3.5 cm/f). f16 left ankle 0.37 -> 0.22 m ahead of the
   pelvis, knee bent ~140 deg at contact (now ~160), toe up: ball_z 0.067 -> 0.10 with ankle 0.094. Head nod on impact: head
   f17/f18 30.8/28.1 -> 35/34, back on the current curve by f21.
2. [dati] f22-23 left ball_world_f 1.995/2.096 -> 1.893 +-0.01 (toe pinned, ankle_z 0.141/0.170 -> 0.18/0.26, knee extends
   = real push-off); lift at f24 with ball_world_f >= 1.90 and rising (never backward): f24 ~1.95, f25 ~2.10.
3. [posa] Push, f7-8: extend the right leg ON the ground: f8 knee ~100-110 -> ~160 deg, ankle_z 0.175 -> 0.22 (toe stays at
   0.227). f9 right ball_world_f 0.060 -> >= 0.22 (no backward jump), ball_z 0.151 -> 0.06; split f8/f9/f10 0.59/0.90/0.97 ->
   0.68/0.80/0.90, still reaching 1.0-1.02 at f11-12 (the legs keep spreading during the hold).
4. [posa+timing] Load: pelvis_z f4/f5/f6 0.742/0.720/0.710 -> 0.70/0.65/0.63; keep both feet together until f6 (split f5/f6
   0.21/0.33 -> 0.10/0.18). Smooth the entry: lean f2/f3/f4 15/18/31.5 -> 14/23/31, pelvis f2/f3 0.827/0.810 -> 0.83/0.77.
   Then the push is the fastest change: lean f7/f8 46/52.8 -> 48/54 (pelvis must rise 0.63 -> 0.74 over f7-9).
5. [posa] Lunge f9-12: pelvis_z 0.782-0.79 -> 0.74 at f9, 0.75 at f10-11, 0.74 at f12, 0.73 at f14 (small arc, lower than the
   run's 0.80-0.83). Rear toe grazes: right ball_z 0.15-0.19 -> 0.04-0.07 on f9-12. Straighten the rear knee to ~170 deg. Left
   (front) foot ball_z 0.21 -> 0.12. Lean 55 -> 51 stays.
6. [posa] Lunge arms f9-13: left arm now hangs vertical with the fist at knee height: bend the elbow to ~90-100 deg, fist forward
   beside the front knee at hip height (fist_from_spine l 0.39 -> ~0.45). Right fist f9-12 0.43-0.49: keep, drop it ~8 cm.
   Follow-through: both fists reach their extreme at f11 (2 f after the lean peak at f9), not with it.
7. [posa] Width (front view), all frames: ankles ~0.18 -> 0.35 m apart in the load f3-6 and at the f16 contact; knees out ~8 cm
   per side on f4-13; elbows ~10 cm off the ribs on f4-12. Head stays forward of the shoulders.
8. [dati+gioco] f5 right ball_z 0.016 -> 0.032. End pose f25: right foot still in the air (ball_z 0.128, ~0.35 m ahead), left
   toe trailing, pelvis 0.80 falling: the run loop must be entered ~2 f before its right-foot contact (set the blend start
   phase / sync marker), otherwise the feet pop in the blend. Dust burst needed f1-8 (feet slide ~0.14 m before the plant).

Most important change: make the catch carry weight (fix 1: land at 0.22 m ahead, compress to 0.715, rise over 3 f) and pin the toe on f22-23 (fix 2).
