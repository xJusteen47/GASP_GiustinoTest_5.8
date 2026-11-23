# Brief: generic run start turnarounds (B / LL / RL) on the w1 standard (owner 2026-11-21 14:35Z: "poi fai i turn around")
Desktop, Opus high, budget ~15 $, updates _ue59 (lock _ue59/IN-USO.txt; owner editor open: ask in-thread before closing).
Branch claude/runstart-generic-desktop (from the w2 commit). Read first: handover runstart-generic-w1-desktop.md (+ w2 section) and
docs/notes/reviews/runstart-w1_h1/curves-sheet.md (the sprinter weight-transfer principle). Do not reread old threads.
## Goal
The approved standard is w1 (tag approved/runstart-w1, Variant 20, Handoff 1 = defaults; w2 = 21 if the owner takes the slide fix).
Today only the forward starts (M_Relaxed_Run_Start_F_Lfoot/_Rfoot rows) have our clip; starting AWAY from the facing (backward,
left-turn, right-turn: stock rows M_Relaxed_Run_Start_B_* / LL_* / RL_*, Content/Characters/UEFN_Mannequin/Animations/Run/) still
plays stock. Build the turnaround starts on the same principle, as sibling rows of the B/LL/RL stock rows at the SAME variant value
(20, and 30 for Handoff), so Variant 20 gives the w1 family in every direction. Orientation warping cannot fake a 90-180 turn (v5).
## Identity traits (rule 13a, the review checks them first)
1. Sprinter weight transfer INTO the new direction: the body turns and falls down-and-forward toward the run direction in the first
   frames (pelvis ~-10 cm, torso ~40 deg toward the new heading), low through two drive steps, one rise on the long stride.
2. Frantic limbs from frame 1 (rate ~2.3x like w1), no planted pause. 3. No clash idle -> start -> loop. Knees never backward.
## Approach (recommended; scripts exist, extend them)
- runstart_generic_compose.py: new env SRC (stock start dump to take root position AND yaw from, default the F_Lfoot dump) so the
  root follows the stock B/LL/RL turn and ramp; loop poses rotated with the root yaw; w1 recipe otherwise. Dumps: as
  Saved/AnimPipeline/runloop/startL_stock_dump.npz (Cascadeur rig dump of the stock clip, FbxSettings up_axis Z).
- The w2 lesson: in game the capsule runs ahead of the clip root (feet slide). Capture first, then TMAP/PIN_U (w2 knobs) if the
  drive contacts slide > 10 cm.
- Rows: runstart_generic_rows_ue.py takes STOCK siblings: extend it to B/LL/RL (RSG_SRC). Captures: runstart_generic_captures.ps1
  drives only a forward start (sequence mv_run_start_v5): add a sequence that starts with the stick reversed / at 90 deg
  (Tools/GASNPPTest command table, see memory "Command table & capture logging") and an ASSERT on the new clip.
- One direction first (B, the most visible), review it (rule 10), then LL/RL with the same recipe. Foley via add_foley.
## Done when
B, LL, RL clips (L and R foot) built, ASSERT OK, Fable PASS, cvar help + debug menu label say what Variant 20 covers, handover + COSTO.
Owner decision to raise only if needed: whether the 180 should be a pivot-in-place (feet planted, body spins) or a run-out turn.
