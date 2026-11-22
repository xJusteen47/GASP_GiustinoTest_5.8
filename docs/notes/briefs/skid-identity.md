# Skid: identity brief (rule 13a, 2026-11-20)

Scope: run -> reverse-input braking skid (Run F Loco Transitions / IsPivoting), clean base gstn.Loco.Base 1.

IDENTITY TRAITS (owner notes; changes need an owner decision)
1. BACK FOOT PLANTED. The rear foot stays on the ground for the whole skid (no slide, no hop). Reference: Ganondorf's Smash dash-brake / turnaround, braking weight on the rear leg.
2. ONLY THE FRONT (braking) FOOT SLIDES, and its slide is driven by a designer marker/curve (slide window authored on the clip, not baked by root motion), so the length is tunable.
3. SMOOTH BLEND IN AND OUT. Run -> skid contact and skid -> idle/run blend without a pop or pose snap. Reference: Astro Bot Ganondorf fluid/cartoony bar (animation-review-bar memory).

START FROM (GASP stock, handover phase3-batch1-1)
- Stock plays M_Relaxed_Run_Turn_R_180_Rfoot (pose-matched, from f137 in mv_skid), ASSERT: OK *Run_Turn_*180* f140-175. Evidence: docs/notes/reviews/skid-stockbase/ (pack stock_mv_skid_*).
- Our own clips in play from the earlier owner work: SS_180_Turn2 (one motion) and SS_180_Turn_Root180 (Content/Blueprints/Animation/Ported57/SkidOwner, currently modified in the working tree on claude/loco-jumpland-1: do not touch). Row table: Run F Loco Transitions/IsPivoting in the chooser CHT_GSTN_StockMover_PoseMatch.
- NEXT STEP: review stock base against traits 1-3 (foot planting of the rear foot, blend). Stock Run_Turn_*_180 is a mocap turn, likely fails trait 1/2 (both feet move); restyle only if the blind review shows it beats stock.
CAPTURE SEQUENCE: mv_skid (200 frames, stick (0,1,0) f0, (0,-1,0) f95, release f170); variants mv_skid_base, mv_skid_release, mv_skid_hold.

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

