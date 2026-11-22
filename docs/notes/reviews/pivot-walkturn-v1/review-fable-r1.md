# Review r1 (Fable): run pivot, walk turn variant v1

Source: brief.md and the four grids only (base/var, game and az90, f110-157 step 2). No video, code or logs read.
Limits: the game-camera grids show the character about 30 px tall, so they only support coarse position reads.
All pose reads come from az90. Foot slide and root-vs-capsule are low confidence for that reason.

**Overall verdict: FAIL.** The variant reads worse than the base on life (it stands up instead of braking) and gains
nothing on timing: both take about 0.55 s from pivot entry to a run pose, against the Toad target of 0.3 s.

## Life

| Item | Base | Variant | Label | Evidence (az90) |
|---|---|---|---|---|
| Anticipation | PASS | FAIL | pose | Base f124-f126 throws the torso back against travel and plants. Variant f120-f124 just straightens up, feet nearly together at f124: it looks like stopping to stand. |
| Weight | PASS | FAIL | pose | Base f128-f138 hips drop into a wide low crouch. Variant f126-f138 hips stay high, legs near straight at f134-f138: no braking load, a walk posture at run speed. |
| Arc | PASS | FAIL | pose | Base head path is one dip (down f126-f138, up into the run). Variant goes up (tall at f122-f124), stays high to f138, then drops at f140-f142: a bump, not an arc. |
| Follow-through | PASS | PASS | - | Variant f150-f156 leans into the new run with a trailing leg; this part is the GASP Reface start, not our clip. |
| Body turns with capsule | PASS | PASS (caveat) | - | Variant turns right (chest to camera f126-f146), base turns left (back to camera f128-f138). For an exact 180 either side is valid; the grids cannot show a capsule yaw side. Without a mirror, a left-leaning reversal will turn the wrong way. |
| Root vs capsule | PASS | PASS (low confidence) | - | Shadow stays under the hips in both; no visible drift of the body off the shadow through f126-f148. |

## Technical

| Item | Base | Variant | Label | Evidence |
|---|---|---|---|---|
| Pop at entry (~f117) | PASS | FAIL | montage-data | Variant f118 -> f122: run lean goes to upright in about 4 frames while the legs collapse from a full stride to feet together (f124). Base keeps the stride through f124. |
| Pop at hand-off (~f135) | n/a | FAIL (mild) | montage-data | Variant f138 is an upright wide stand, f142 is a deep sideways lean with bent knees: a posture change in 4 frames, right where Reface_Start_090_R takes over. The chest is still facing the camera until f146, so the turn is finished by the GASP clip, not by ours. |
| Foot slide | PASS (low confidence) | not verifiable, suspected FAIL | game | Variant holds a planted wide stance f126-f138 while the stock capsule is still braking and then re-accelerating; a walk-turn clip has far less root travel than a run brake. Needs a foot-lock overlay or a top view to confirm. |
| Turn direction | PASS | PASS (caveat) | montage-data | Right only, as stated in the brief. Mirror row missing. |
| Timing vs Toad (0.3 s +-0.1) | FAIL | FAIL | game | Entry f117; first clear run pose the other way at about f150 in both grids: about 33 frames = 0.55 s. From the stick flip at f95 it is about 0.9 s. The clip swap does not change this: the stock capsule brake and re-accel set the duration. |
| Quick, not locked in place (goal 1) | PASS | FAIL | pose | Variant f124-f138 (about 0.25 s) reads as a standing pause. Base is the same length but reads as a continuous brake. |

## Ranked next fixes

1. **Timing is a capsule problem, fix it there (game).** With `gstn.Skid.Sim 0` the reversal takes about 0.55 s whatever
   clip plays. To reach 0.3 s the brake and re-acceleration of the capsule must be shorter (stock deceleration / turn
   boost tuning, or our sim skid with a short fixed duration). Do this before any more clip work, or the clip will be
   judged against the wrong duration.
2. **Replace or reshape the pose source (pose).** A walk turn has an upright torso and high hips, which is exactly what
   reads wrong at f120-f138. Either use a run-based pivot as the source, or keep this clip and add a brake pose on top:
   torso back at entry (as base f124-f126), hips down through the turn (as base f128-f136), and start the clip later
   so the feet-together frame (f124) is never shown.
3. **Fix both seams and add the mirror (montage-data).** Entry: longer blend in or pose-matched start frame so the run
   stride is not dropped at f118-f122. Hand-off: keep our clip weighted until the chest has passed 90 degrees (about
   f142-f146) or pose-match into the Reface start, so f138 -> f142 is not a posture jump. Add the mirrored row for
   left reversals before any owner test.

Follow-up capture request: a top-down or foot-lock debug view for f120-f150 to settle foot slide, and capsule
velocity over the same frames to place the Toad comparison on numbers.
