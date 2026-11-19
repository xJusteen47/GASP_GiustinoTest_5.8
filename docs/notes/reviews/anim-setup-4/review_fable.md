# Anim setup 4: review of the in-game captures (Fable, 2026-11-19)

Fixed: Saved/Captures/setup4_fixed (KIT_Test_Overlord, no cvar lines in capture.log). Base: Saved/LocoR2/cap1 (mv_run_stop,
`gstn.RunStop.Mode 1`, KIT_Test_Playtest), Saved/AnimSetup3/cap_skid2 (skids), Saved/Dash/cap5 (dash). No base for jab1_long.
Looked at: 15 grids (run stop 5, skid release 3, skid hold 2, dash 3, jab 2; az90 every 2nd frame, az0 for skid release and jab)
plus frames.jsonl of both sides (feet, pelvis, hands per frame, clip names). Videos not opened. Frame numbers = v (60 fps).
Weights grave / medio / lieve; labels posa / dati montage / gioco.

| Sequence | Verdict | vs base |
|---|---|---|
| mv_run_stop | PASS (second pass, recapture with Mode 1; first pass was FAIL for the capture) | same, loop better |
| mv_skid_release | PASS, 2 medio open | same |
| mv_skid_hold | PASS, 1 medio open | same (run start slightly better) |
| mv_dash | PASS | better |
| jab1_long | PASS, 1 lieve open | no base |

## mv_run_stop: FAIL
Identity trait 2 (both feet slide to a stop, then the torso rises) checked first: **it cannot be judged, the clip is not there.**
1. **grave, gioco (capture chain)**, v193-279 az90. The stop that plays is GASP `M_Relaxed_Sprint_Stop_F_Lfoot` (rows `combat.anim`),
   not the Ganon run stop. The base capture set `gstn.RunStop.Mode 1`; this capture.log has no cvar line and the default is off
   (GSTN_RunPresentationComponent.cpp:484, "off until a stop clip passes the review"). So the edited run stop clip was never on
   screen. What is seen instead: a lunge onto the front foot, pelvis 92 -> 58 cm (v210), crouch held to v222, torso up v216-246,
   then the front foot steps back 40 cm (v240-252) after the body is already upright. No two-foot slide, no dust. Base same frames:
   rear leg dragging with dust v186-222, torso up v222-232. Worse than base, but it is a different clip, not the edit.
   Smallest fix: capture again with `gstn.RunStop.Mode 1` (and the same kit as the base).
2. Idle -> run start (v022-030) and run start -> run loop (v053-106), az90: no pop, no frozen frame, feet on the floor; poses match
   the base frame for frame. Same. (Largest foot acceleration 17.7 cm/f² at v043 in both.)
3. Run loop: base had the right ankle 2-4 cm under the floor at v161-164; fixed has none in the whole sequence. Better.
4. lieve, gioco (spawn, not the edit), v021->022 az90: the capsule sits 12 cm above the floor until the first input (actor z 100 ->
   88.1 in one frame); the mesh stays put, the camera drops 12 cm in one frame. jab1_long is captured entirely in that state.
5. Not animation: az90 camera swings behind the platforms at v184-194 (fixed only).

## mv_skid_release: PASS
Owner design checked first: brake, turn 180 (actor yaw 180 by v142), ends standing facing the new way (v170 on). The stick is
released at about v154, after the skid clip ended (v146), so the sequence shows skid -> 8 frames of run -> GASP
`M_Relaxed_Run_Reface_Stop_180`. "Release during the skid = stop inside the same clip" is not exercised by this capture.
1. **medio, gioco**, v144-150 az90: skid exit. The extended left leg snaps under the body into a run stride in 4 frames (51 cm and
   21 cm up v146 -> 150, 30 cm/f²). Base 31 cm/f² at the same frames. Same: the 0.2 s ease did not change this hand-off.
2. **medio, gioco**, v134-146 az90: the body leaves the new way still in the low brake pose and the right foot skates 44 cm flat on
   the floor (base 42 cm). Half hidden by the dust. Same. If this is the designed slide, fine; it is not a pop any more than before.
3. lieve, gioco, v182-198 az0/az90: after the body is upright (pelvis 90 cm at v178) the left foot lifts 9 cm and re-steps 28 cm.
   Base did the same with the right foot (50 cm sideways, v178-194). Same or slightly better.
4. lieve, capture: v117-119 three identical frames, actor not moving for 2 (base: 1). A hitch when the input flips, not a clip freeze
   (mv_skid_hold has none at that point).
No foot under the floor, no both-feet-up outside run flight phases.

## mv_skid_hold: PASS
Owner design: stick held = run again. Yes: v146-152 straight into the run, no stop, no idle pose in between.
1. **medio, gioco**, v144-150 az90: same exit snap as skid release (32.5 cm/f², base 34.8). Same.
2. Run start -> loop v086-092: base had a left-foot jerk (20.7 / 18.4 cm/f²), fixed has none in the top list. Slightly better.
Grids fixed vs base v116-186 az90: same poses frame for frame. No foot under the floor, no frozen frames.

## mv_dash: PASS (better)
1. Idle -> dash, v044-046 az90: base showed one frame of the run start pose (v044, arms forward) before the dash, hands 33 cm/f²;
   fixed goes idle -> dash crouch directly, 12 cm/f². Better.
2. Dash -> run, v058-066 az90 (every frame looked at): gather with feet together v061-063, run from v065. No pop, no slide.
3. lieve, gioco, v062 (data only, not visible on az90): the actor advances 19.5 cm in one frame instead of 9.8 and the mesh absorbs
   it (pelvis 8 cm back in actor space, then stays about 14 cm behind the capsule in the run). Possible 10 cm camera nudge in the
   game view; base has no such step.
4. lieve, gioco, v063: left hand 16.6 cm/f² at the hand-off to the run (base under 14 there). Frame by frame only.
The stop after the release (v113 on) is again the GASP sprint stop: see mv_run_stop item 1.

## jab1_long: PASS
Both feet in the air while returning to idle: **no.** Right foot steps back v038-041 and lands at v042; the left foot lifts only at
v042. One foot at a time (frames.jsonl, az90 + az0 v036-082).
1. **lieve, gioco (blend-out foot lock)**, v044-058: the left foot stops 4.5 cm above the floor, frozen for 6 frames (v044-049), rises
   to 7 cm (v052) and touches down only at v058, for a 3.5 cm correction. The rest of the body is still from v044, so at real
   speed it is a small late foot tap. Smallest fix: put the foot down as soon as it reaches its place, or skip the step under ~4 cm.
2. lieve, dati montage, v021-023 az90: at the jab start the left foot goes 36 cm back in 2 frames at floor height. Reads as a snap
   only frame by frame; not part of this change.
3. v038-044: stance closes and arm drops in 6 frames, then dead still to v160. Readable, Smash-like; no item.

## Critic: the three heaviest
1. grave: the run stop on screen is the stock GASP stop (capture without Mode 1). 2. medio: skid exit leg snap v144-150 (both
skids, unchanged). 3. medio: 44 cm flat-foot skate v134-146 in the skid (unchanged). Looked for and not found: feet under the
floor (none in fixed), both feet up after the jab, frozen clip frames at any hand-off.

## Per l'owner
- mv_run_stop: vedi il secondo passaggio in fondo (PASS dopo la ricattura).
- mv_skid_release: PASS. Uguale alla base; restano aperti lo scatto della gamba all'uscita (v144-150) e il piede che striscia 44 cm (v134-146), entrambi medi e di gioco.
- mv_skid_hold: PASS. Con la leva tenuta riparte a correre senza fermarsi; stesso scatto all'uscita della base (medio).
- mv_dash: PASS, meglio della base. Sparito il fotogramma di run start prima del dash; uscita verso la corsa pulita.
- jab1_long: PASS. I piedi tornano uno alla volta; resta un lieve: il sinistro resta sospeso 4-7 cm per 14 frame prima di appoggiarsi (v044-058).

# Second pass (2026-11-19): run stop recaptured, slope captures

Captures: Saved/Captures/setup4_runstop/mv_run_stop and Saved/Captures/setup4_slope_foot2 (both KIT_Test_Playtest,
`gstn.RunStop.Mode 1` in capture.log). Looked at: run stop 2 grids (az90 v184-254, az0 v190-278) + the base grid of pass 1; slope up
stop 2 (az90 v060-130, v144-250); slope up skid 3 (az90 v024-114, v120-190, v192-258); slope idle 1 (az90). Slope foot height
= ankle above the 15 deg plane under the capsule, from frames.jsonl (planted foot reads 7.8 cm on the slope, 9.4 on the flat).

| Sequence | Verdict | Open |
|---|---|---|
| mv_run_stop | **PASS** (replaces the FAIL above) | 2 lieve |
| mv_slope_up_stop | **FAIL** for the start (runs backwards), slope part PASS | 1 grave gioco, not slope |
| mv_slope_up_skid | PASS for what it shows; **skid on the incline not captured** | 1 medio, 1 lieve |
| mv_slope_idle | PASS | none |
| mv_slope_down_stop | PASS | 2 lieve |
| mv_slope_down_skid | PASS | 1 medio (same as flat), 1 medio gioco, 1 lieve |

## mv_run_stop: PASS (same as base)
Clip on screen: `AS_RunStop_Ganon_R_V6` v192-276. Trait 2 first: **holds.** Both feet slide until v210 (right) / v214 (left), pelvis
stays low (78-79 cm) to v218, torso rises v220-242 (pelvis 92 at v242), arms settle to v250. Then one foot at a time: left steps
in v214-226, right v226-234. Same timing and poses as the base within 1-2 cm (base x differs because it released later).
1. Stop -> idle, v254-278: feet do not move at all while the idle blends in (weight 0.56 at v278). Base crept 2-3 cm from v258.
   Better so far; the capture ends before the blend is complete, so the last ~20 frames are not seen.
2. lieve, posa, v250-257: 8 fully still frames before the idle starts to blend (base 3). Frame by frame only.
3. lieve, VFX (not animation), az0 v230-278: the dust puff stays as a white lump at the feet to the end of the capture.
No foot under the floor (min ankle 9.4), no pop (max 17.7 cm/f², run start, same as base).

## mv_slope_up_stop: FAIL (start), slope PASS
1. **grave, gioco (not slope, not the clip edits)**, v048-080 az90: the sequence starts facing +x and the stick goes -x. The actor
   yaw reaches 180 by v048 but the body still faces the old way: it travels about 2 m backwards with the forward run start
   (head 25 cm on the wrong side of the pelvis v036-066), then turns with `M_Relaxed_Run_Spin_B_F_R_Rfoot` v078-105, exactly on
   the flat -> slope edge (x -3487, v076). So the flat -> slope transition is seen only inside that spin. Run start from rest with a
   180 deg stick needs its own check on the flat.
2. Run uphill v106-150: feet reach the surface every step (lowest ankle 7.2 cm), no foot through the slope, pelvis steady.
3. Stop on the incline, `AS_RunStop_Ganon_L_V6` v152-237. Trait 2 **holds on the slope**: both feet slide 51 / 61 cm to v175 at a
   constant 7.8 cm (left) and 13.6-15 cm (right, heel up), pelvis low to v180, torso rises v180-195, then right foot steps v175-185,
   left v185-195. Ends with both feet at 7.8 / 8.1 cm, no drift to v259. Reads well: brake leaning into the hill.
4. lieve, VFX: dust lump at the feet v198-249.

## mv_slope_up_skid: PASS for what it shows
1. **Capture, not animation:** the reverse at v125-150 happens on the flat top of the hill (actor z constant 409.7 from v110), so
   the skid on the incline is not in this capture. The skid seen is the flat one (same poses as mv_skid_release, same exit snap
   v152-157, 31 cm/f²).
2. Run start on the incline, v025-100 az90: clean. Planted feet at 7.8 cm, lowest 7.4, no foot in the slope, no pop; slope -> flat
   edge at v102-108 without a step in the pelvis.
3. **medio, gioco**, v176-259: after the skid the release on the way down gives GASP `M_Relaxed_Sprint_Stop_F_Rfoot`, not the Ganon
   run stop, even with Mode 1 (the flat mv_skid_release gives the GASP reface stop too). Two different stops for the same
   character. As a downhill stop it holds up: deep lunge (pelvis -35 cm), front foot on the slope at 7-11 cm, rear foot on the
   toe, no foot through the surface, right foot re-steps 37 cm v220-235.
4. lieve, gioco, v237-246: 10 identical frames, then the pose settles another 2 cm (v247-259).

## mv_slope_idle: PASS
The idle is on the incline near the top (z 383), not at the foot of the hill. Both ankles 7.6-7.8 cm above the slope for all 110
frames, no jitter (max 0.2 cm/f²), uphill knee bent, pelvis 4 cm lower than on the flat, trunk upright. Only the leftover dust
cloud around the legs for the whole capture (VFX).

## mv_slope_down_stop: PASS (Saved/Captures/setup4_slope_top3, 1 grid az90 v084-154)
Plateau -> slope edge at v092 while running: no step in the pelvis, first foot on the slope at 8.3 cm, no foot through it.
Stop going downhill, `AS_RunStop_Ganon_L_V6` v107-186. Trait 2 **holds**: both feet slide 35 / 46 cm to v124-128 (front foot
7.7 cm on the plane, rear on the toe at 13.3), pelvis low to v136, torso up v136-152, right foot steps v132-140, left v140-148.
Ends at 7.6 / 7.6 cm, still to v204. Only 15 frames of downhill run before the release (v092-106), so the downhill run loop is
barely seen.
1. lieve, posa, v108-134 az90: the brake leans 36 cm forward down the fall line, same pose as on the flat. Readable, but a brake
   going downhill would sit back; owner's call, not a defect of the edit.
2. lieve, gioco, v206-214: in the idle the right foot lifts 4 cm and shifts 3 cm by itself.

## mv_slope_down_skid: PASS (2 grids az90 v076-122, v124-216)
The skid is on the incline here (run down, reverse v080-106 at z 305-322, run back up). Design holds: brake, turn 180 (yaw 180 at
v096), runs up again; after the release at about v126 it stops facing uphill.
1. **medio, gioco**, v108-113: same exit leg snap as on the flat (32.5 cm/f²).
2. **medio, gioco**, v131-219: the stop after the skid is GASP `M_Relaxed_Run_Stop_F_Lfoot`, not the Ganon stop (third time: flat,
   up, down). Upright jog-to-stop, front foot 2-7 cm above its resting height v160-184 while it re-steps.
3. lieve, gioco (foot IK), v088-108, data only: the downhill strut leg (left, 46-56 cm ahead of the body) has its ankle 13-22 cm
   above the slope against 7.8 planted, so it does not reach the lower ground; on az90 the dust covers it and it cannot be told
   apart. The uphill foot is at 11-13 cm. No foot through the slope anywhere (lowest 7.5).
4. lieve, capture: v078-079 two identical frames at the stick flip (same hitch as on the flat).

## Still not seen
Skid while running uphill on the incline, flat -> slope transition while running forward (uphill), a longer downhill run loop.

## Per l'owner (secondo passaggio)
- mv_run_stop: PASS. Il nostro run stop ora si vede: i piedi scivolano fino a v214, poi sale il busto; uguale alla base, piedi fermi nel passaggio all'idle.
- mv_slope_up_stop: FAIL per la partenza: con la leva a 180 gradi da fermo corre all'indietro per 2 metri e poi si gira (grave, gioco, non dipende dalla salita). Corsa e stop in salita a posto, piedi sul piano.
- mv_slope_up_skid: lo skid e' avvenuto sul piano in cima, non in salita: da ricatturare. Partenza in salita pulita; dopo lo skid lo stop e' quello GASP, non il nostro (medio).
- mv_slope_idle: PASS. Piedi appoggiati sulla pendenza, fermi; resta solo la nuvola di polvere (VFX).
- mv_slope_down_stop: PASS. Stop in discesa col nostro clip: piedi che scivolano, poi il busto; piedi sul piano. Lieve: frena piegato in avanti verso valle come in piano.
- mv_slope_down_skid: PASS. Skid in pendenza a posto (frena, gira, risale). Restano lo scatto della gamba all'uscita (medio, come in piano) e lo stop GASP dopo lo skid (medio); il piede a valle resta 10 cm sopra il piano, coperto dalla polvere (lieve).
