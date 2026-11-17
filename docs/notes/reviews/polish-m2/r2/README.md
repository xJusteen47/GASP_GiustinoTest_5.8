# Polish M2, round 2: review package

Captures (not in git): VFX ON `Saved/PolishM2/r2c/vfx1`, VFX OFF `Saved/PolishM2/r2a/vfx0`. Each sequence has 3 views of 480x480:
`side` (on the host's right; it follows the victim in hit_large, wall, ceiling, splat and ground_bounce), `game` (the player's camera)
and `close` (the player's camera at half its field of view). Frames `<seq>/<view>/fNNN.png`, one per 60 fps frame. Same inputs and same
start in both runs, so frame N OFF and frame N ON show the same moment.

Grids `<seq>_<view>_<from>-<to>.png`: for each frame, OFF above and ON below. GIFs: OFF | ON side by side, half speed (no ffmpeg on the laptop).

Sequences: jump (jump at v20, landing ~v84), hit_small / hit_medium / hit_large (attack at v20, hit ~v31-36; hitstun 31 / 40 / 67 frames,
intensity 0.56 / 0.74 / 1.10, accent stars only from 1.0), hit_large follows the victim to the downed landing (~v213), wall and ceiling
(a box summoned across / over the launch), splat (KIT_Example_Impacts strong into a wall box), ground_bounce (KIT_Example_Impacts spike),
run_skid (run from v20, stick back at v130), sprint (sprint 2 lines from ~v176, release at v270).

Events with VFX ON: `events_on.txt` (`PRES` lines of the host world). `events_off.txt` is from the OFF run, which still had the round 1
derivation (one extra Jump.Ground and Land per knockdown, fixed in round 2; the simulation is the same).

What the game did not produce (label "gioco", not a VFX defect): no WallBounce in `wall` or `ceiling` (the victim stops against the box
and drops: a knockdown, no rebound), no GroundBounce in `ground_bounce` (knockdown, no bounce event). The Impact system is seen only on the
splat (v~48-61).
