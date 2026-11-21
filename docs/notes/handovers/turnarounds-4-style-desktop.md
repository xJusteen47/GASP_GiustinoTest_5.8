# turnarounds-4 style (desktop, 2026-11-22 00:00): 180 L style pass in Cascadeur = variant 28; GATES FAIL after 2 tries, stopped (rule 5)
Branch claude/runstart-generic-desktop (from 5b92370a). Lock _ue59 22:58-23:05Z, editor never opened (headless Save-Assets only).
Re-check of turnarounds-3 (rule 11): 27 (w2t180Lp) UE clip gates rerun now -> GATES: OK (joints json, gates script below).
## Done
- Cascadeur (MCP): RunStart_Generic_w2t180Ls.casc from w2t180Lp. Key poses on arms/hands/head at f0, f2, f4 (step plant), f6 (H),
  f8 (H+2), keys 1/3/5/7 deleted, clamped bezier + key weights = curve sheet (180a_style/curve_sheet_180L.md). L elbow back+out
  toward the new heading a frame ahead, R elbow out to its side (fixes 27's twinning: both elbows ~20 cm back at f4-f6), hands
  outside the hips. Pelvis phase-1 drop 9 cm -> 3 cm (identity -3/-4). Feet and rev (f9+) untouched. Export export_joints Z-up.
- UE: AS_GSTN_RunStart_Generic_w2t180Ls (runstart_generic_g_ue.py, RSGG_TAG=w2t180Ls, curves json = copy of w2t180Lp, same H=6
  so enable_warping/steeringtargettime keep their timing). Variant 28 rows (runstart_turn_ue.py RST_SUFFIX=s RST_K=28
  RST_KEYS=180L RST_FALLBACK=f RST_LO1=27.5 RST_LO2=37.5): 180L at 28 row 117 / 38 row 121, other directions = f clips.
  CHT_GSTN_StockMover_PoseMatch NOT committed (it already carried uncommitted variant 27 rows before this thread, as before).
## Gates (python runstart_pivot_gates.py --json Saved/AnimPipeline/runstart/joints_AS_GSTN_RunStart_Generic_w2t180Ls.json 12)
- r0: FAIL jitter knee_l f1 12.8, hand_l f1 12.1, elbow_r f7 10.9, hand_r f7 14.5. r1 (pelvis f1 +0.5, R f6 more back, f0 weight
  0.33): FAIL knee_l f3 11.8, hand_l f1 10.9, hand_r f7 11.6 (180a_style/gates_joints_*.json). Gap 13.6, bones 0.0 %: OK.
- No capture, no review (rule 15b). Nothing goes to the owner.
## Cause (diagnosis, not proven) and what is needed
- The scene is in the ROOT frame and the 180 deg turn is added on the root bone at import, accelerating (9, 18, 33, 40 deg/frame).
  Sparse arm keys are interpolated in the root frame, so in component space the hand-vs-shoulder second difference is ~11-13 cm
  every frame with sign flips (27 dense: 5-8). Knee_l f3: the pelvis lift changes the near-straight support leg's bend f1-f3.
- Options for the next thread (coordinator picks; A/B are Cascadeur work, C is a pipeline change -> owner, rule 6):
  A (recommended) breakdown keys in Cascadeur at f1, f3, f5, f7 on the arm layers (skill: "add breakdown keys where arcs go
     off"), tuned until the UE-clip gate passes; pelvis lift eased over f1-f3. ~4 $.
  B keep sparse keys, loosen the jitter gate for keyed turn arcs: NOT recommended (gate was asked for by the coordinator).
  C bake the root yaw into the Cascadeur scene (pose in world, importer re-extracts yaw): clean for all turnarounds, ~10 $.
- Then: capture behind+side (variant 28 vs 27 base, same frames), ASSERT, commit to 180a_style/, cloud review.
COSTO: ~6 $ | ~8 min desktop | 0 token review
