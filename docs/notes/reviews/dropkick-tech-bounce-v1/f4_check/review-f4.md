# Dropkick tech bounce, honest recheck (F4 thread, 2026-11-17)
Why: the owner could not see in dropkick_*_r3.mp4 what review r3 claimed. Rule 10: capture chain first.
**Capture chain (r3)**: right montage (AM_OverlordDropkickTechBounceR3 plays v140-v151, Saved/DropkickBounce/variant_r3/dropkick_whiff/frames.jsonl),
kit KIT_Test_PlaytestTBR3, pad idle (heldButtons 0). But the mp4 sent to the owner was an 8-view mosaic (1440x720, 480 px views shrunk to 360) lasting
0.67 s; the whole bounce is 12 frames (0.2 s); and the contact frame has the pelvis under the floor (-33 cm). Review r3 judged zoomed grids, not the video.
**Recapture** KIT_Test_Playtest, Saved/DropkickBounce/f4_check (3 PIE players requested; clients' players did not join in 40 s, other pawns 600 cm away),
heldButtons 0 in every frame. Same timing as r3: contact whiff v138, low_whiff v138, hit/low_hit v142; pelvis at contact -6 / -33 / -31 / -15 cm.
Videos here: <seq>_realspeed.mp4 and <seq>_slow4x.mp4 (side | behind | game camera, frame labels). Grids grid_*.jpg.
**Fable 5.2 high review: FAIL against the r3 claims**
| Claim | Result |
|---|---|
| Legs together, straight | PARTLY: together v138-147; knees bent 20-30 deg, legs piked up (side v138-149) |
| Arms crossed at contact, open in arc | TRUE in side/behind views only (crossed v138-141, open v142-147) |
| Readable contact pose | PARTLY: v137->v138 one-frame snap, pelvis -50 cm, head-high to supine |
| Squash, head last | FALSE: squash is 1 frame (v138), head already on the floor |
| Rebound as consequence | PARTLY: rigid pike rises to 51 cm, hangs ~4 f, reads as a float |
| Knockdown | TRUE (v150-151) |
- gioco: body through the floor at contact (low_hit v142 torso buried).
- dati montage: no held squash; GroundHit starts mid-air (v149, pelvis 30 cm), pike -> spread-eagle in 1 frame (v149->v150).
- posa: rise is one rigid pose pivoting head-down; 51 cm peak too high.
- gioco camera: body ~45 px tall seen along its axis: traits 1-2 not visible at real speed (teleport to floor, small hop, spread-eagle).
