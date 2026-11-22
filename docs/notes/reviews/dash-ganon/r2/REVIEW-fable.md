# Review r2 - dash-ganon (offline, stick renders + metrics.json) - reviewer: fable

## Verdict: PASS (import and capture in-game)
All 3 traits pass and the two certain defects of r1 are gone (toe slide f22-23, push-leg pop f8->f9). The fixes below are
not gating; 1 and 2 are cheap and worth doing in the same pass as the import. Seen: r2 grid_side, grid_front (2 images) and
metrics.json, against the dossier and the reference grid seen for r1. Limits: stick figure only (no mesh volume, no hands);
mp4 not opened; knee angles and lateral sizes are read off the grids (~).

## Identity traits
1. Compact load - PASS. f2-6: pelvis 0.88 -> 0.63 (0.25 m, r1 0.17), lean steps +7/+8/+8/+5/+3 (stutter gone), no hold at the
   bottom, fists tucked (0.29). It is a sprinter's crouch more than a ball (right foot planted at 0.119 from f3, so it trails
   from f5), which reads fine for a brute.
2. Lunge - PASS. f9-12: lean 55/53.7/52.3/51, pelvis 0.74-0.75 (under the run's 0.80), both fists forward-low and peaking at
   f11 (0.365/0.521), head 2 f late (peak 43.6 at f11), moving hold of 4 f. Weak point: split only 0.81 -> 0.88 m (r1 1.02,
   dossier ~0.6 body heights) because the rear knee is still kinked ~25 deg.
3. Heavy settle - PASS (weak). f16 heel-first ~0.26 m ahead of the hips, compression 0.765 -> 0.715 (f16-18), bottom held f19,
   rise +2.5/+3.5/+2.5 cm over f20-22, head nod over the lean at f17-19 (37.5/36.3/32.8 vs lean 35.4/33.0/31.3). But half of
   that compression is bought with a rise in the AIR: pelvis 0.730 -> 0.749 -> 0.765 on f14-16 with no foot on the ground.

## Other findings
- Push is now the fastest change: lean f7 +10, f8 +7; split grows +0.15/+0.14/+0.14 then +0.04 (smooth, no pop). The right toe
  stays at 0.119 from f3 to f9 (slide 0.0), then drags forward at ball_z 0.05-0.06 on f10-12.
- Contacts: left plant f16-23 slides 1.3 cm (1.756 -> 1.743), toe pinned at 1.743 on f22-23, lift f24-25 moving forward only.
  No penetration worth noting (ball_min 0.029). Stretch 0.0.
- Left foot f4-6: on the ground (ball_z 0.033/0.032/0.029) and moving 8-10 cm/f in the world while the right is planted:
  one foot nailed and one skating in the same frames.
- Front view: wider (ankles ~0.34 m, knees and elbows out on f4-13); no limb crossing seen.
- Stance on the left foot lasts 8 f (f16-23, ~0.7 m of travel): long, acceptable for the weight; judge in game.

## Fixes (by impact)
1. [posa] Float before the catch, f13-19: pelvis_z 0.732/0.730/0.749/0.765 (f13-16) -> 0.745/0.742/0.740/0.740 (flat, never
   rising in the air), then f17/f18/f19 0.735/0.715/0.715 -> 0.720/0.705/0.705, rise as now to 0.80 by f22-23. Make the foot
   come down as a stomp: left ankle_z f14/f15 0.160/0.124 -> 0.19/0.15 (last step 5-6 cm, now 3).
2. [posa] Lunge split, f10-13: straighten the rear (right) knee ~155 -> ~172 deg, split 0.844/0.880/0.883/0.844 ->
   0.90/0.95/0.97/0.93, toe still grazing at ball_z 0.05-0.06. Lean and pelvis stay.
3. [posa] Load step, f4-6: lift the left foot so it reads as a step, not a skate: left ball_z 0.033/0.032/0.029 ->
   0.05/0.07/0.08 (ankle_z 0.091/0.094/0.130 -> 0.11/0.14/0.16).
4. [timing] Lunge as the longest-held pose: f13 lean 47.8 -> 50 and f14 44.0 -> 46.5 (5 f above 50 instead of 4), then
   f15/f16 40.9/38.0 as now. Only if fix 2 is done (otherwise the hold shows the kinked leg longer).
5. [gioco] Run-loop entry (from r1 fix 8, still open): f25 right foot in the air (ball_z 0.128), left toe trailing (0.071),
   pelvis 0.80 flat: enter the run loop ~2 f before its right-foot contact (blend start phase / sync marker).
6. [gioco] Dust: burst on f1-6 (both feet slide ~6 cm f1-3, the left skates f4-6) and a trail on f10-12 under the dragging
   right toe (ball_world_f 0.203 -> 0.373).

Most important change: remove the rise in the air on f15-16 (fix 1: pelvis flat at 0.74 into the contact, bottom at 0.705).
