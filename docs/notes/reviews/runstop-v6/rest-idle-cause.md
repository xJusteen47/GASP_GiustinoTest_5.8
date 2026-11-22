# Run stop: why the body drifts 3-5 cm after the clip (locomotion-finish-4, 2026-11-19)

Capture Saved/RunStop7/mode{1,0}/mv_run_stop_rest (KIT_Test_Playtest, 420 frames, stick released v180, gstn.RunStop.Mode 1 / 0).
Rows now carry combat.anim (the clips the main graph plays, weight, time). Summary: python Saved/RunStop7/rest.py <frames.jsonl>.

## The two rest poses are two GASP idle clips
- At spawn (v20) the game plays **M_Relaxed_Stand_Idle_Break_v08**: feet (1.3,-14.7) / (-8.7,14.5).
- After the stop it plays **M_Relaxed_Stand_Idle_Loop**: feet (0.6,-11.8) / (-12.2,19.3) from v300 on, constant to v439.
- AS_RunStop_Ganon_R_V6 ends on the Idle_Break stance (v240-255: (0.6,-15.3) / (-9.8,14.7)), so it matches the spawn idle, not the
  one the game goes to.
- The hand-off is a slow blend: Idle_Loop weight 0.00 at v255, 0.25 v270, 0.52 v277, 1.00 v300 (~45 frames). Feet go
  -15.3 -> -11.8 (3.5 cm) and 14.7 -> 19.3 (4.6 cm) over it: that is the drift the V6 review saw (2-4 still frames, then 35 frames).
## Mode 0 (GASP stop M_Relaxed_Sprint_Stop_F_Lfoot) for comparison
- Same Idle_Loop after the stop, same ~0.5 s blend (v330-360), but the feet do not move: the stop ends with feet (15.1,-12.5) /
  (-9.5,19.5) and Idle_Loop is shown with the same feet to v439. So in mode 0 the feet of Idle_Loop follow the stop's plant; in mode
  1 they do not. Curves are not the difference (Saved/Routing/clip_curves.txt: both clips have contact_l/r; GASP stop also has
  movedata_speed and a PoseSearchBlockTransition notify state, ours has neither).
## Options (not applied: the clip is in its 4th pose round, rule 10 -> owner)
- A (pose): end the R/L clips on the Idle_Loop stance above instead of Idle_Break. Cheap, but one more pose round.
- B (gioco, data): blend into the idle after "Run Stop" in ~0.1 s instead of ~0.5 s (chooser row). Turns the drift into a small pop.
- C (gioco, code, recommended): hold the feet through the Blend Stack hand-off out of Run Stop the way BlendOutFootLock does after
  montages (step when the gap is > 2 cm). Also helps skid and landings (same frozen/slide hand-offs).
