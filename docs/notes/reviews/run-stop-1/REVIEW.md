# Review run-stop-1 (2026-11-19, desktop, thread run stop) - verdict: FAIL (posa), nothing goes to the owner as a PASS
Capture: KIT_Test_Overlord, sequence mv_run_stop_base (stick let go at the base run stage), view az90, real speed (t.OverrideFPS 60).
Variants, same frames: base = GASP motion matching stop (gstn.RunStop.Mode 0), clip = state "Run Stop" with GASP
M_Neutral_Run_Stop_F_L/Rfoot speed-matched to the sim (Mode 1), blend = state "Run Stop Blend", 0.3 s cross-fade into idle (Mode 2).
Files: base_vs_clip_az90.jpg (rows base/clip/blend, f62-102 step 4), run_stop_base_clip_blend.mp4 (f50-139, 60 fps).
Evidence the clip state ran: Saved/RunStop/clip/run.log.ue.log, `grep -c "RunStop mode=1 active=1"` = 375 (all pawns); the player's
stop: entry 470 cm/s, clip time 0.722 -> 1.033 (stop) while the sim slows 460 -> 0, then the settle.

Identity traits (brief section 2, ref smash_run_stop.jpg):
1. Front foot planted, facing forward, no turn - clip: feet plant, no turn, but no lunge: the front leg is under the body. PARTIAL.
2. Torso rises AFTER the feet, arms open low - clip: torso already upright when the feet stop (f82), arms forward. NO.
3. Soft parts arrive last - no cape on the mannequin; head and arms settle with the body. N/A.
Blend: a slowing jog that sinks into a knees-bent idle (f94-102); no braking pose at all. NO on 1 and 2.
Smash/AAA parity (owner's two questions): clip NO, blend NO, base NO. The GASP Neutral stop is a realistic jog-to-stop;
Ganondorf's stop is a heavy brake with the torso still leaning over a planted front foot.
FAIL items: posa (clip: no lunge, no lean-then-rise), gioco: none found (the timing works: the brake follows the sim).
