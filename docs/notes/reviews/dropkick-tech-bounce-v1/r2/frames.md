# Dropkick tech bounce round 2: frames (video v, 60 fps). Base = KIT_Test_Playtest before option A (Saved/DropkickBounce/base_pt), variant = KIT_Test_PlaytestTBR2
Captures: `Saved/DropkickBounce/variant_r2/<seq>/frames.jsonl`. Grids aligned on ground contact (`--vshift`), contact base/variant:
dropkick_whiff v140/v139, dropkick_hit v142/v142, dropkick_low_whiff v138/v138, dropkick_low_hit v142/v142.
Bounce clip on screen (then GroundHit): whiff v139-v149 (to clip f10.3), hit v142-v152 (f10.4), low whiff v138-v149 (f11.0), low hit v142-v153 (f11.0):
the r2 clip reaches GroundHit f0 at f11, so the hand-off has no pop.
Contact frame (variant dropkick_whiff v139): pelvis world z 7.5 cm (round 1: -25.7, 1 frame under the floor): fixed by the capsule change
(FGSTN_BodyCapsule::GroundedRestore).
Grids (base top): g1-g2 high whiff side/back, g3-g4 high whiff/hit GAME ZOOM (contact-1..+12, crop 140,300,300,460), g5 low whiff side,
g6-g7 low whiff/hit GAME ZOOM, g8 low hit side.
Owner kit after option A (Saved/DropkickBounce/playtest_kd): KIT_Test_Playtest dropkick_whiff downs its user (log "SelfKnockdown ... landed"),
hop actor up to 116.3 cm, generic GroundBounce + GroundHit (the own bounce ability is NOT on TL_Overlord_Dropkick until the clip passes review).
