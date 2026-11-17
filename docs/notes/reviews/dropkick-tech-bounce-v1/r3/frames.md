# Dropkick tech bounce round 3: frames. Base = Saved/DropkickBounce/base_pt (playtest kit before option A), variant = KIT_Test_PlaytestTBR3 (Saved/DropkickBounce/variant_r3)
Contacts base/variant: whiff v140/v140, hit v142/v142, low whiff v138/v138, low hit v142/v141 (grids aligned on contact, `--vshift`).
Hand-off (hands world z, last clip frame -> GroundHit's first frame on screen -> its next): whiff 44.7/47.8 -> 35.5/38.7 -> 6.3/6.4 (v151-153);
hit 55.3/58.1 -> 39.7/42.9 -> 9.2/9.4; low whiff 42.9/46.0 -> 32.9/36.1 -> 4.4/4.6; low hit 43.5/46.7 -> 34.7/37.9 -> 6.2/6.4.
The clip-to-GroundHit step is now ~10 cm (r2: W pose vs GroundHit f1); the remaining ~30 cm one-frame drop is GroundHit's own f1 -> f2 slam
(reaction clip in DA_ReactionAnims, owner asset: not touched).
Grids (base top): g1-g2 high whiff side/back, g3-g4 high whiff/hit GAME ZOOM (contact-1..+14), g5 low whiff side, g6-g7 low whiff/hit GAME ZOOM,
g8 low hit side. mp4: dropkick_whiff_base / dropkick_whiff_r3 / dropkick_low_hit_r3.
