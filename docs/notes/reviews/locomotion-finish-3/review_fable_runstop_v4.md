FAIL - run stop V4 (capture rs4/mv_run_stop, v172-279; reviewer Fable, rule 10)
Sources: 3 grids, 4 single frames (az90 v217/219/236/250), frames.jsonl, run_stop_L/R_bones.json. Clip f0 = v186 (f = v - 186).
The capture plays the RIGHT-foot clip (run_stop_R). It matches run_stop_R_bones.json to 0.2 cm on every frame f26-f72.

Identity traits
- Trait 1 (front foot planted, facing forward, no turn): OK. Plant v190-191, no yaw, front foot down until v226.
- Trait 2 (torso rises only after the feet stop): OK. Feet stop v217, chest push v218-220, rise v221-238.
- Trait 3 (head/arms land after the torso): FAIL in this clip. Pelvis and head land v238, hand L v238, hand R v239.
  No overshoot (head x 4.3 -> 3.0 -> 2.3 -> 1.8 -> 1.5 -> 1.3, monotone). From v233 the upper body is equal to V3 to 0.1 cm.

Fixed since V3 (verified)
- Brake is no longer frozen: pelvis sinks 3.3 cm v192-204 and recovers 1 cm by v211; head z 44.8 -> 42.5 -> 44.7.
  Subtle in the side view, but the pose breathes.
- Stop push is in and reads (v217 vs v219): head +4.4 cm forward / -3.2 cm down, pelvis -1.5 cm on v218-220, recoil by v223.
- Feet end 10 cm apart (L +5, R -5). Front foot step back is 36 cm (was 51). No creep at the clip end: v260-279 moves
  1.6 cm per foot (was 9 cm). V3 item 4 closed.
- No pops into the idle. Actor/mesh offset steps at v188 and v212 are continuous in world space (not visible).

dati montage (generator, main FAIL)
1. run_stop_gen.py numbers(f, "R"): the handover to the true idle blends ALL channels with
   w = ease((f - (STOP+8)) / 14), so it is complete at STOP+22 (f53, v239). The V4 follow-through keys sit after that
   (trunk overshoot STOP+25/29, head STOP+31/35, arms STOP+34/35), so the R clip never plays them. Result in game:
   everything lands together at v238-239 and v239-258 are 20 identical frames (pixel diff at noise level). This is V3
   item 2 unchanged, with a longer dead tail (20 frames, was 10). The L clip does contain the late landing (pelvis f58,
   head overshoot f56-62, hand R f66), but it is not in any capture, so it is not reviewed.

posa
2. v218: the push starts at full speed. Head moves 3.9 cm in one frame from rest (then 1.1, 0.1); hands drop 2.3 cm on
   the same frame. Reads as a small tick, not a weight push. Ease it in (segment STOP -> STOP+3 is "out"; use "io" or 4 frames).
3. v218-235: still two quick 8-frame steps (49 cm, then 36 cm) while the trunk rises. Better than V3, still a bit light and
   busy for a medium-heavy body. Not blocking alone. Recheck once item 1 is fixed, because the late trunk changes the read.
4. L clip only (from the bones file, not seen): hand R travels 29 cm in f56-66 after the trunk is home. Check in the next
   capture that it reads as drag and not as a second gesture.

gioco
5. v185-186: two repeated frames (actor step 0, pose frozen) right before the clip starts at v187. Same as V3 item 6.
6. Stick is still not logged (the "st" field is constant for all 280 frames), so the input delay is still not verifiable.
7. Dust: a pale puff is still at the feet at v236-258 (v250: about 40 cm wide, 33 frames after the stop). V3 item 8, open.

Next fix (one): in numbers(f, "R") do not blend the upper body early. Either mirror the offset from the idle
(true_idle + mirror(action - idle)) so no handover is needed, or blend each channel only after its own last key
(feet as now; pelvis/trunk from STOP+29, head from STOP+35, arms from STOP+35) and cut the tail to 2-3 still frames.
Then capture BOTH sides (one stop on the left foot, one on the right) and log the stick per frame.
