# Owner video 2026-11-20 21:38 (20261120-2138-18.4480488.mp4, 2520x1378, 30 fps, 6 s) — "jitter with both camera setups"
Measured by phase correlation of the far band (rows 60-260 at 1260 px wide) against frame 0 (script inline in the thread; numbers below).
1. Frames 0-41, character standing/walking in place on flat ground: the VIEW alternates between two heights, background offset
   -2..-4 px vs -9..-13 px (at 1260 px wide), switching every 1-3 recorded frames (~7-10 Hz). Near objects move more than far ones
   (frame 3 -> 4 at full res: horizon ~9 px, red disc ~17 px, character feet ~24 px): a vertical TRANSLATION of the camera, no zoom,
   no yaw. See owner_video_f3_f4.png. The character moves with the world, not against it: the camera bounces, not the mesh.
2. Not explained by the tick-order fix: on flat ground at rest the actor root does not step vertically, so a one-frame-late
   correction is ~0. Something with TWO states toggles the camera height ~10-25 cm.
3. About 1/3 of the recorded frames are exact duplicates (mean abs diff < 0.2): the game showed ~20 distinct frames/s in this
   recording (3 PIE players + Moonlight). Low, uneven fps reads as stutter on its own, separate from item 1.
4. Frames 42-80 (walking away) and the jump 81-89: no two-state bounce measured (offset steady -5..-8).
Candidates for the two states (not proven): anchor on/off toggling (IsActiveForOwner), GASP rig state switching (gait/crouch rig),
our push back sweep hitting/not hitting, Mover.IsInAir flicker. gstn.Camera.Trace 1 + camera_trace_check.py names the stage.
