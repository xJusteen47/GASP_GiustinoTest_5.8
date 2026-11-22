# Review anim-setup-1 (Fable, 2026-11-19)
Source: Saved/AnimSetup1/cap1, 60 fps, f = v. Looked at the three az90 grids and 8 single frames (run stop az0 f298, f302, f304,
f308; skid hold az90 f136, f137, f141, f150). Numbers from frames.jsonl (world = actor + foot offset; planted = foot z -77/-79).
Not judged: mp4s played as video, sound, the hold run after v153.

## 1. Run stop hand-off: PASS
- No slide in the blend: both feet 0.0 cm from f240 to f298 (before: 3.5 / 4.6 cm drift). Pelvis drifts 2.4 cm to the right
  f270-f300 with the idle blend, feet do not follow. Grid v250-v298 shows no foot change.
- Step f299-f307: right foot lifts 4.0 cm (peak f302-303), travels 5.2 cm, max 0.8 cm per frame, eased up and down. Not a pop,
  not a skate. It is small: about 6 px in the 480 px frame, and it reads as a foot adjust more than an acted settle.
- Left foot 3.4 cm f299-f304: on the floor (z constant -78.8), max 0.8 cm per frame, under the right foot's step. Not readable
  as a slide in az0 f298-f308.
- Minor, gioco (no FAIL): the left foot slides while the right foot is in the air, so the support foot is the one that moves.
  The pelvis moves 0.1 cm during the step (no weight shift). Better order: left foot adjusts after the right one lands, with a
  1 cm pelvis dip. Only worth doing if the step is ever seen close up.
- Capture note: a dust cloud stays on the feet for the whole grid (v250-v320, the pawn is still from f240). It hides the feet in
  every view, so the step is judged mostly from numbers. For foot checks capture without VFX. Dust lasting 80+ frames on a
  standing character is a VFX question for the owner.

## 2. Skid turn side: PASS for the change, 1 old FAIL still open
- Same side in both runs: actor yaw rises 22.5 deg per tick for 8 ticks (hold f131-f138, release f133-f139; the 45 deg step at
  release f137 is a capture frame with two ticks). Mesh yaw is smoothed (23.4 deg per frame), so no yaw step reaches the feet.
- Body and clip agree: the right foot is the pivot (hold f130-f136 moves 7.5 cm, release f132-f137 5 cm), the left foot sweeps
  a 42 cm arc on the floor (hold f130-f135) and then holds 1 cm (f135-f138). Same wide low lunge in both runs (a5b: two
  different poses). No double turn, no body turning against the legs. The 45 deg frame moved the feet 0.2 cm.
- The two runs up to the split: same motion, not identical. The turn starts at speed 0 (hold f131 = clip 0.233, release f133 =
  clip 0.250), so it is one tick later on the clip in the release run. Result: mesh yaw 17 deg apart for 6 frames (hold
  f135-f140 / release f136-f141), left foot lands 11 cm wider in release (45 vs 35 cm from the lane). a5b was 38 deg and 32 cm.
  Minor, gioco: latch the turn start to the clip time, not to speed 0. The release run also skids 7 cm longer (entry speed).
- Reading note: the hold pawn starts at yaw -180, so its az90 grid is seen from the opposite side. The grids look mirrored;
  the data say same side.
- Overshoot: actor yaw 180 at hold f138 -> 196.9 at f141 -> 177 by f152 (release f139 / f142 / f153). The mesh follows it.
  Not visible as an error: the feet do not swing (f138-f141 left 6 cm, right 6.6 cm; 17 deg would be 18 cm), head 2 cm per
  frame, az90 f141 reads as follow-through of the turn. Leave it.
- FAIL, gioco (old "gioco 1", not caused by this change): right foot pop while planted (z -79.4). Hold f136 -> f137 12.9 cm,
  release f137 -> f138 14.8 cm, the same clip time in both (0.317 -> 0.333), then slow slip 12 cm (hold f137-f144). It is tied
  to clip time, not to a yaw step. In az90 f136/f137 the skid dust covers most of it. First suspect as before: foot lock
  released by contact_r; second: a step in the clip itself (then it is dati montage). Check the clip frame 19-20 alone.
- Minor, gioco (old "gioco 4", unchanged): exit pops. Left foot 28 cm in one frame while lifted (hold f145 -> f146, release
  f146 -> f147); planted right foot zigzags 10 + 13 cm (hold f146-f148, release f147-f149).
- Capture note: two movement ticks in one frame at hold f142 and release f137 (head jumps 5 cm). Not animation.

## Next fix (one)
The right foot pop at clip time 0.317 -> 0.333 of SS_180_Turn_Root180 (hold f137, release f138).
