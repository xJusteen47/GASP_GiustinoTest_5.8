# Review skid-runstop-3 (Fable, 2026-11-19) - SKID: FAIL (gioco) - RUN STOP: FAIL (narrow: posa + capture too short)
Source: Saved/Review3/cap1/{mv_skid,mv_run_stop}, 60 fps. Looked at 5 images (skid) + 4 (run stop): the side sheets and my own
full-size sheets of az90 / az45 / game frames. Numbers from frames.jsonl (actor loc, feet/pelvis/head relative to the actor).
Frames below = capture frames (fNNN = vNNN). Not judged: sound, the mp4 in motion (stills + numbers only).

## SKID (SS_180_Turn_Root180, stick reversed f116) - FAIL
Identity traits
1. Brake reads as a heavy brake: YES. f122-134 sits back, pelvis -9 -> -22 cm, arm up, dust at the heel, 509 -> 0 cm/s in 14 frames.
2. One continuous motion, brake THEN 180 turn: NO. The turn never shows. The body keeps facing the OLD direction from f134 to f188
   while the actor already runs the new way (az90 f140-178, az45 f146-188: chest and lean point at the dust he is moving away from).
3. Comes out facing the new direction: only by a snap, f188 -> f194 (6 frames, feet 90 cm apart sideways at f198-204).
FAIL items
- gioco 1 (blocker): backwards run, f134-f188 (0.9 s), up to 586 cm/s. This is the known "double turn": clip root yaw 180 plus the
  actor/Offset Root Bone turn cancel out. Fix: log actor yaw, root bone yaw and mesh yaw per frame in frames.jsonl (missing today),
  then keep ONE turn only: either consume the clip's root yaw upstream of Offset Root Bone (rotation Accumulate, as in the 5.7
  project) or play the root-locked SS_180_Turn2_Src and let the actor yaw turn. Cause not proven here, only the symptom.
- gioco 2: feet slide about 95 cm, f140-f154. Both feet are still in actor space (L -28,-44 / R 0..-7) while the sim goes
  250 -> 540 cm/s. The sim restarts at f134, the clip is still in its low turn. Fix: hold the sim near 0 until the clip's push-off
  (clip rest is at 0.567 s = f152) or drive speed from the baked root track; restart the run only after the feet leave.
- gioco 3: clip end held, f168-f172 feet frozen (same values 3 samples) at 586 cm/s, then the blend to the run. Fix: leave the
  state at the push-off frame (before the last 4-6 frames) with a 0.15 s blend, not at clip end.
- dati montage: none found (clip plays 1x, 50 frames f118-f168, entry f118-122 clean, no pop).
- Capture note: single frames where the mesh lags the actor by one tick (pelvis jumps ~10 cm): f108, f158, f182, f206, f118 (speed
  0 then 506). Looks like a capture hitch every ~24 frames; confirm it is not in a real-time run.

## RUN STOP (AS_RunStop_Ganon_L_V1, stick released f180) - FAIL (narrow)
Identity traits
1. Front foot planted, facing forward, no turn: YES. Right foot plants f188 and stays to f220. Facing never changes. Stable pose now.
2. Torso rises after the feet, arms open low: PARTLY. Lean held to f214, rise f214-220 (6 frames, fast). Right arm opens low (az45
   f190-204), left arm just hangs.
3. Soft parts last (head, arms overlap): NOT readable in game.
What works: entry f184-188 without pop, lunge f188-196 is a clear readable brake silhouette, speed 586 -> 0 in 31 frames (f184-215).
FAIL items
- posa 1: weight is light after the lunge. f198-f212 he stands tall on almost straight legs (pelvis only -6 cm), leaning forward
  over the toes while still sliding 300 -> 30 cm/s. Reads as tipping forward, not as a heavy man sitting into the brake. Fix: keep
  the pelvis low (-18/-22 cm) and the chest behind the front knee until speed is under ~80 cm/s (clip frame ~28), back foot drags in
  later (now f194-204, make it clip frames 24-36), then rise in 12-14 frames with head and arms 3-4 frames late.
- posa 2: small up-bump of pelvis/head, f190-f194 (+4.5 cm then down) in the middle of the brace. Fix: flat or falling pelvis curve there.
- gioco 1: front foot skids 109 cm flat on the floor (f188-f222, actor x 1260 -> 1369) with NO dust (the skid has dust). Fix: spawn
  the dust puff at the plant and a trail while speed > 100 cm/s; otherwise it reads as ice.
- gioco 2 (capture): the idle settle is not on film. The capture ends at f229 with the right foot still stepping back (x 30 -> 0.6,
  idle wants -9) and the left foot 11 cm ahead of its idle place. Fix: record 40 more frames, then check for a pop or slide into idle.
- dati montage: none found.
Parity: skid no (facing bug); run stop close, first half yes, second half not yet medium-heavy.
