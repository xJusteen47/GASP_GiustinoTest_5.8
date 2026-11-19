# Anim setup 4: review of the in-game captures (Fable, 2026-11-19)

Fixed: Saved/Captures/setup4_fixed (KIT_Test_Overlord, no cvar lines in capture.log). Base: Saved/LocoR2/cap1 (mv_run_stop,
`gstn.RunStop.Mode 1`, KIT_Test_Playtest), Saved/AnimSetup3/cap_skid2 (skids), Saved/Dash/cap5 (dash). No base for jab1_long.
Looked at: 15 grids (run stop 5, skid release 3, skid hold 2, dash 3, jab 2; az90 every 2nd frame, az0 for skid release and jab)
plus frames.jsonl of both sides (feet, pelvis, hands per frame, clip names). Videos not opened. Frame numbers = v (60 fps).
Weights grave / medio / lieve; labels posa / dati montage / gioco.

| Sequence | Verdict | vs base |
|---|---|---|
| mv_run_stop | **FAIL** (capture does not show our run stop) | start: same, loop: better, stop: worse (other clip) |
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
- mv_run_stop: FAIL. La cattura non mostra il nostro run stop (manca gstn.RunStop.Mode 1, gira lo stop GASP): va ricatturata. Partenza e corsa a posto, piede sotto il pavimento sparito.
- mv_skid_release: PASS. Uguale alla base; restano aperti lo scatto della gamba all'uscita (v144-150) e il piede che striscia 44 cm (v134-146), entrambi medi e di gioco.
- mv_skid_hold: PASS. Con la leva tenuta riparte a correre senza fermarsi; stesso scatto all'uscita della base (medio).
- mv_dash: PASS, meglio della base. Sparito il fotogramma di run start prima del dash; uscita verso la corsa pulita.
- jab1_long: PASS. I piedi tornano uno alla volta; resta un lieve: il sinistro resta sospeso 4-7 cm per 14 frame prima di appoggiarsi (v044-058).
