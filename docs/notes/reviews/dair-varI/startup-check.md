# Dair variant I: startup check (2026-11-03, read-only)
Verdict: startup NOT shifted. Variant I has the same timing as the base overlord_dair_v4.
- Card diff (base v4 vs I, frames 0-17): author poses at 0,3,6,8,12,14,15,17 identical; no key added/moved/retimed;
  timing_csv, spacing, part_delays, anim_active [15,17] and faf_game 39 identical. Only edits at <=17: upperarm_l/r FK at
  frame 12 (variant G arm co-motion, inherited, not timing). New keys 18-30 and 37 are all after the hit.
- Built dumps (sheets/OverlordDAirV4_Build_curves.json vs _VarI_): pelvis, com, knees, feet speed differ <0.004 cm/f at 0-15
  (identical). Only hands/elbows differ (<=2.4 cm/f, frames 8-13) because of the frame-12 arm edit. foot_l/pelvis z unchanged.
- ingame_judge.py: a0 = card windows()["anim"]["active"][0] = 15 (the card's active window, hard-coded per card, no notify).
  The video plays 0..15, then repeats f15 `hit` times (v16..v23), then continues. "v21 (f15) HITSTOP" = video frame 21 is
  the 6th freeze frame; f15 is the FIRST HIT frame (stomp pose), not the frame before it.
- Likely source of the mismatch: the owner's "f16" is the 1-based count (design note: "hitbox from anim frame 15 (16th frame)");
  anim 15 = 0-based. His Cascadeur f15 (charge) = anim 5/12, a different numbering. Base in-game raw/log not re-rendered:
  base card has the same a0=15, so its label is the same "(f15) HITSTOP".
- Hit on anim 16 would need active [16,17]: owner decision, not a variant I effect.

## Follow-up: pose vs hit frame (owner: the stomp pose shows 1 frame after the freeze)
Verdict: card/window problem (shared by base v4 and I), NOT a capture off-by-one in ingame_judge.py.
- Captured raw frames (sheets/ingame/_overlord_dair_v4_varI_az90/raw, side camera): lowest-pixel row of the body 221 @f14,
  228 @f15, 285 @f16, 285 @f17 and a frame-to-frame diff of 0.27/0.75/1.42/0.15 (f14..f17). The feet drop ~57 px between f15
  and f16: the stomp lands on anim 16; f15 is the last frame before it (the screenshot at "(f15) HITSTOP" shows it).
- Capture is honest: grab() waits until set_frame(f) puts hand_r within 0.5 cm of sample_pose(f) (hand_r moves 15 cm/frame
  at 15->16, so a stale frame cannot pass); all raw frames 10-18 are distinct; the freeze repeats raw f15 as the label says.
- The card puts anim_active at [15,17] (a0 = 15), so the hitbox opens one frame before the impact pose. Not verified: why the
  key at 15 lands visually on 16 (build/sequencer offset vs poses). Dump speed uses central differences, so its indices
  cannot say it; the old "step 14->15" in the handover is that metric, not the image.
- One-line fixes: card = anim_active [16,17] + FAF +1 (cards/overlord/overlord_dair_v4*.json, timing CSV active window);
  judge = none needed (a --at=16 run already freezes on the impact frame). Card NOT changed here.
