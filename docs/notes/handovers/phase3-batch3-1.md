# phase3-batch3-1: skid, run start, jump squat, land v2 (orchestratore desktop, 2026-11-20)
Branch claude/loco-skid-runstart-1 (from claude/loco-jumpland-1). Not pushed. Lock _ue59 freed at the end.

## Results
- JUMP SQUAT, f445633c: JumpSquatFrames=3 (Config/DefaultGSTNCombat.ini, 62.5 Hz = 48 ms). "Jump Squat" ABP state with a PLACEHOLDER pose (load part of M_Relaxed_Jump_F_Start_Stand_Rfoot). ASSERT OK (Tools/GAS/jump_squat/squat_assert.py): takeoff exactly 3 frames after the press. Divergence #7. Still to do: animate the real pose; check the squat -> In Air hand-off frame by frame.
- LAND v2, 361beb3e: AS_GSTN_LandStand_Retime_v2 (retime of stock, impact unchanged), blend in 0.05 s. Recovery from idle 2950 -> 1383 ms (docs/notes/reviews/land-v2/recovery.txt, land_recovery.py). Review PASS (review-blind.md; NOT truly blind, the grids had labels). Still to do: ~650 ms frozen tail f148-187 (trim it), gate on a pre-declared window. Divergence #8. Active only with gstn.Loco.Base 1.
- RUN START v3, ae14a28a (Cascadeur, gate OK) + b2e64e19 (in game): stomp at 0.27 s, frantic run, hand-off to the run loop, SS_180 never enters. ASSERT FAIL 85% < 90% (dip v87-95 to 0.50, mirrored by BS_Relaxed_Walk_Leans). Blind review NOT done (rule 10). Rollback: RUNSTART_VARIANT=frantic + run_start_states.py. Divergence #9.
- SKID, a2ddb6f5: the controller no longer follows the stick during the skid. The capsule follows the root of the owner's clip (RootTrack) until the clip ends. Back foot f117-167: 171.0 -> 153.9 cm (skid_foot_metric.py on Saved/SkidFix/{before,after}/mv_skid/frames.jsonl). ASSERT FAIL 89% < 90% (blend-in f117-122). The remaining slide is in the in-game clip SS_180_Turn_Root180 (58.6 cm in the window where the source has the foot fixed), probably root_fix_skid_180.py. Divergence #10: it is NOT a return to stock (own layered move, Mario brake bypassed; SKID_ROOT_TRACK=0 brings it back).
- Stock skid review (review-stock.md) was written before the owner's correction (the problem was the controller, not the pose). The skid Cascadeur v3 has been dropped.

## Open
1. Capture gate: the "leans" dip appears in run start, Frantic and land. The logs have no state or slot (abp_err get_state_machine). One editor run is needed: log the state + transition alpha and read the ABP transition times, then excuse N frames with N taken from the ABP.
2. Skid: try SS_180_Turn2_Src or fix the root of SS_180_Turn_Root180 (owner clip: work on a variant, rule 13b). Walk turn 180 (Mixamo "sword and shield 180 turn.fbx") is not in Content and needs the owner's conversion that produced SwordAndShield180Turn.UE.fbx.
3. For the owner: the skid with the custom layered move vs a return to the stock pose-matched pivot (rule 14, owner said "prefer stock").
4. d1856683 = WIP leftover uncommitted saves after batch2-1 (32 files, incl. L_Test_Hub.umap): check before merging.
5. Air idle / air crouch not started (budget).

COSTO: ~66 $ | ~180 min desktop | review ~0.4M token
