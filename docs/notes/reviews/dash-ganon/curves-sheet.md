# L3 dash: curves sheet (rule 13c) - shared by author and reviewer

Source of truth = the key tables at the top of Tools/AnimPipeline/cards/dash/dash_ganon_gen.py (r3). Frames here count from 1
(clip frame index + 1), 60 fps, 25 frames (0.4 s). Interpolation: Catmull-Rom through the keys (moving holds, no stops at keys)
unless stated. Measured values per frame: docs/notes/reviews/dash-ganon/<round>/metrics.json.

| Channel | Timing (frames from 1) | Easing / shape | Overshoot / follow-through |
|---|---|---|---|
| Root (sim) | 1 -> 610 cm/s at ~4.3, then constant | linear accel 11200 cm/s2 (BoostDash x8 of stage 2) | none: the capsule is the sim's |
| Lean (pelvis->neck_01) | 6 -> 37 over 1-6 (even ~8/f), push 37 -> 54 on 6-8 (fastest change), lunge 55 -> 50 on 9-13 (5 f at 50+), 46.5 at 14, 33 at 18, 25 at 25 | smooth, push is the steepest segment | moving hold in the lunge (-4 deg drift) |
| Pelvis height | 0.88 -> 0.63 over 1-6 (load), 0.74 at 9, 0.75 arc 10-11, flat 0.74 on 13-16 into the contact (never rising in the air), compress to 0.705 at 18-19, rise to 0.80 by 22 | no step above 3.5 cm/f | catch compression ~5 cm (weight), no float |
| Upper spine hunch | +14 deg at 6, gone by 10 | in/out | - |
| Head (neck_01->head) | 9 -> 40 by 10, peak 44 at 11 (2 f after the lean peak), nod 35/34 at 17-18 | follows the lean 1-2 f late | nod on the catch |
| Fists | tucked by the hips 3-6; forward-low, extreme at 11 (2 f after the lean); run swing from 16 | smooth | fists overlap the lean by 2 f |
| Right (push) foot | slides with the body 1-2, toe planted in the WORLD 3-9 (heel up 0 -> 62 deg), toe-off 9, rear knee ~straight, grazes ~5 cm 10-13 (split 0.92 m), recovers forward 13-25 | - | the straight rear leg is the lunge's line |
| Left (lead) foot | slides 1-3, steps up from 4, knee drive 6-9, stomp-down 14-16 (5 cm/f), low lunge foot 9-14, heel-first contact 16 at 0.22 m ahead of the hips, flat 17-21, toe pinned 22-23, kicks back 24-25 | - | heel strike rolls onto the ball over 16-18 |
| Hip yaw | left hip forward 14 deg at 11, back to -14 at 25 (run counter-swing) | smooth | shoulders counter 80% |

Hand-over to the run: the clip ends with the right foot ~0.35 m ahead and about to contact (~frame 27); the run loop must be entered at
the phase just before its right-foot contact (phase curve from curves_author.py), blend 0.12 s.
