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
