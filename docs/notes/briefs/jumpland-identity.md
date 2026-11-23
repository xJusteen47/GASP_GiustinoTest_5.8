# Jump squat + land: identity brief (rule 13a, 2026-11-20, phase3-batch2-1)

Scope: ground jump take-off (squat) and the land from any jump/aerial, standing and running. Base = GASP stock clips on the clean
base (gstn.Loco.Base 1): take-off M_Relaxed_Jump_F_Start_Stand_Rfoot / _Sprint_Rfoot (In Air Transitions), land
M_Relaxed_Jump_F_Land_Stand_Heavy_Lfoot (Stand Mode Transitions) / M_Relaxed_Jump_F_Land_Run_Heavy_Rfoot (Run Mode Transitions).
Stock capture: Saved/JumpLand1/base1 (KIT_Test_Overlord) and base1_pt (KIT_Test_Playtest), sequences mv_jump_rest, mv_run_jump.

IDENTITY TRAITS (checked FIRST by the review; changing them needs an owner decision before the build)
1. SQUAT = a loaded spring. Knees and hips fold, pelvis drops, chest pitches over the knees, arms swing back and down; on take-off
   the whole chain extends from the feet up (ankles, knees, hips, chest, arms forward/up last). Reference: Ganondorf's Smash jump
   squat; never "idle pose floating up" (what stock shows today, takeoff_az90.png v41-47).
2. LAND = impact with weight. Near-hard cut into the contact pose (owner: landing from aerial = almost a hard cut), pelvis drops on
   the contact frame, knees absorb, chest pitches forward and comes back with follow-through; heavier and SHORTER than the stock
   mocap (stock settles in ~70 frames hunched, land_az90.png v104-166). Landing dust already exists: VFX untouched.
3. RETURN TO IDLE = feet re-plant one at a time (owner), not both sliding into the idle stance together.

CURVES SHEET (13c, starting point = stock timing, to be written next to the poses): squat compress 3 sim frames (owner 1A
2026-11-17), extension 2-3 frames into the air pose; land contact on frame 0, max compression frame 3-5, recovery to stance by
~frame 25-30, foot re-plant one foot ~f20, the other ~f28.

GAME vs POSE (13e, rule 10): items that are game, not pose, go to the code queue:
- G1 take-off has no ground squat frames in KIT_Test_Overlord (pelvis rises the frame after the press) although the owner chose
  3 frames (1A): squat poses cannot show on the ground until the kit's squat is 3.
- G2 the stock land blend is 0.2 s from the fall pose (frames.jsonl f102-114), so the contact is soft whatever the clip does.
</content>
</invoke>
