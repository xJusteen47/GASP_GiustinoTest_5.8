# Run start: identity brief (rule 13a, 2026-11-20)

Scope: idle -> run start (accelerating from standstill), clean base gstn.Loco.Base 1.

IDENTITY TRAITS (owner notes; changes need an owner decision)
1. STOMP FIRST, 0.3 s. The first 0.3 s (~18 frames at 60 fps, 9 at 30) is a heavy stomp: one foot drives into the ground, pelvis drops slightly, chest loads forward. Reference: Ganondorf's Smash dash start, the commit step before the run.
2. THEN FRANTIC RUN. After the stomp the legs/arms go to a fast, short-stride, flailing cadence (not a smooth mocap accel), settling to the normal run. Reference: Astro Bot Ganondorf bar (fluid, cartoony, in character), 3 vs 5 s of rapid pump.
3. CLEAN HAND-OFF to the run loop: stride phase continuous at the exit, no foot pop.

START FROM (GASP stock, handover phase3-batch1-1)
- Stock plays M_Relaxed_Run_Start_F_Rfoot (seen at f53 in both run stop captures). No stock capture dedicated to it yet. Our existing clip: AS_GSTN_RunStart_Frantic (Content/GSTN/Animation/RunStart, modified in working tree on claude/loco-jumpland-1: do not touch). Check it first: it may already hold traits 1-2.
- Chooser: Run Start rows of CHT_GSTN_StockMover_PoseMatch (dump with stops_rows_ue.py RS_ROWS=dump generalised).
- NEXT STEP: add sequence mv_run_start to pie_capture_move.py if absent (stick (0,1,0) at f20, ~120 frames, shoot, follow, expect clip M_Relaxed_Run_Start_F_Rfoot window f20-60), capture stock, then review against traits 1-3.
CAPTURE SEQUENCE: mv_run_start (to add; mv_run_stop f0-53 shows the stock start meanwhile).

PIPELINE (rule 13a; pattern of run stop v3, phase3-batch1-1, branch claude/loco-runstop-1 d74b9a16)
1. Stock rows on the clean base (PIE_CAPTURE_CVARS="gstn.Loco.Base 1"): confirm which stock clip the chooser plays; no new asset yet.
2. Stock capture: pie_capture_move.py, 1 player, sequence below, ASSERT: OK on the stock clip window. Saved/<Name>1/base1.
3. Review the stock against the identity traits (blind Fable high, A=stock). If stock already satisfies them: stop, ship stock.
4. Only if stock fails a trait: copy clip, Cascadeur v3 rigid-group restyle (pelvis+chest translate as a group, chest/head/arms as
   rigid subtree rotations, knees IK in the original plane, feet pinned) per cards/runstop/sprintstop_restyle_casc_v3.py, with the
   pre-UE gate (top delta, max deg/frame jump) in that file; then sprintstop_restyle_ue.py-style UE diff (root_diff 0.0).
5. Rows via Tools/AnimPipeline/cards/runstop/stops_rows_ue.py generalised (RS_ROWS=dump|new|stock; fallback = stock).
6. Blind review stock vs v3 (same frame grids); v3 kept only if it beats stock.
7. Isolation: timeline identical to stock base except the clip (frames.jsonl script), life metrics (idle-feet creep, root x end),
   ASSERT: OK on the new clip. Divergence line in docs/notes/gasp-divergenze.md BEFORE commit (rule 14).
CAPTURE: Tools/GASNPPTest/pie_capture_move.py (sequences table; asserts via Tools/AnimPipeline/capture_assert.py). Editor must be
 running on virtual desktop 2 (Tools/GASNPPTest/vd2_mover.py in boot), map L_Test_Hub:
 PIE_CAPTURE_CVARS="gstn.Loco.Base 1" python Saved/AnimR2/run_capture.py <kit> Saved/<Name>1/base1 <sequence>
 (kit KIT_Test_Overlord or KIT_Test_Playtest). REQUIRES THE EDITOR (and _ue59 lock): not done in this prep.
BUDGET: handover COSTO for run stop v3 = ~22 $ (stima), 6 min desktop, ~90k token review. Per clip here: stock-only path ~6-8 $
 (2 captures + review); full v3 path ~20-25 $.

