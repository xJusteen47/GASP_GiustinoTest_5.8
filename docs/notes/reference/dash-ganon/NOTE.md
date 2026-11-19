# Ganondorf SSBU dash: reference for the L3 dash clip (owner videos, 2026-11-18)
Source: project files `handoffs/crouch-ref/ganon_9541.mov` (phone filming the Switch, 30 fps, hand-held; game runs at 60 fps,
so 1 phone frame = ~2 game frames). Owner (11-19 02:12Z): "la reference del dash ce l'hai dai video di ganondorf".
Grids (30 fps, every frame, label p<n> = phone frame): `dash_A_turn_right_9541_t7.50-8.04_30fps.jpg` (cleanest, side view),
`dash_B_turn_left_9541_t8.40-8.94_30fps.jpg` (same dash back the other way, closer camera), overview `overview_9541_6fps.jpg`.
Other dashes in 9541 at ~t11.7-12.5 and ~t13.3-14.0 are cropped by the camera: not usable.
Both usable dashes START WITH A TURN (dash back = turnaround dash). No straight dash from idle facing forward in the videos.

## Timing (grid A; game frames = phone frames x2, counted from 1 at the first frame that moves)
| Phone p | Game f | Phase | Pose key |
|---|---|---|---|
| p0-1 | - | idle (facing left) | upright guard |
| p2 | 1-2 | turn | body swings round, cape flung sideways, knees start to bend |
| p3-4 | 3-6 | LOAD | compact, facing the dash: knees deep, torso hunched forward, arms tucked; dust burst at the feet |
| p5 | 7-8 | push-off | rear leg drives, torso tips forward, cape lifts |
| p6-7 | 9-12 | LUNGE (identity key) | torso pitched ~45-50 deg forward, near-airborne; front thigh up/forward, knee bent; rear leg STRAIGHT back (wide split, ~ a body length heel to toe); fists forward-low, cape horizontal; long dust trail |
| p8-9 | 13-16 | extended stride | torso ~35 deg, legs still wide; front foot reaches down |
| p10-11 | 17-20 | catch | front foot lands under the hips, the body rises a little |
| p12+ | 21+ | run cycle | torso ~25 deg, normal run stride |
Grid B matches within 1 phone frame: turn p1, load p3, push p4 (dust), lunge p5-7, run from p8.
Rhythm: short compact load (~4 game frames), explosive push, the lunge HOLDS ~4 game frames (the dash's silhouette), then it settles into the run.

## Identity traits (rule 13a)
1. Compact load before the push (knees deep, hunched), with a dust burst.
2. The lunge: strongly pitched torso with a wide leg split, rear leg straight, fists low forward; held ~4 game frames.
3. Heavy settle into the run (the body rises as the front foot catches), no float.

## Not measurable here
- Distance/speed: the in-game camera pans with the dash and the phone moves, so the grid squares do not give a fixed scale.
  Use the game's own dash speed data (sim values) or SSBU stats fetched from the desktop; the clip should keep its feet planted to
  whatever speed the sim gives (warping on, owner rule).
- A straight (non-turn) dash start: not in the videos; author it from the same load -> lunge keys without the turn.
