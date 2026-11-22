# runstart stance width (desktop, 2026-11-21): w2 wide = variant 22, ASSERT OK, A/B to the owner; crouch pop r1 PASS
Branch claude/runstart-generic-desktop (continues 5a83eb4b). Lock _ue59 18:37-19:05Z; the owner closed his editor, I reopened it.
DA_Locomotion_Default (owner's uncommitted change) untouched. Re-check (rule 11): w2 base recaptured, ASSERT OK (reviews/runstart-w2s_h1/assert_w2base.txt).
## Done
- Rule 14: Unreal Assistant: stock GASP has no stance-width setting; stock-friendly = Control Rig moving ik_foot_* before Leg IK
  (docs/notes/research/unreal-assistant-stance-width.md). Divergence row 21d.
- Node GSTN Stance Width (GSTNAnimationCore FGSTN_AnimNode_StanceWidth) at the Output Pose of ABP_GSTN_StockMover: each foot outward
  along the root's lateral axis, two-bone leg IK (knee plane, foot rotation, heights kept), ik_foot_* moved too. Weight per foot =
  pose curves GSTN_StanceWidth_L/_R; cvars gstn.Loco.StanceWidth (cm/foot, default 5), StanceMinHalf (floor, 0), StanceWidth.Force
  (0..1 on every clip); all three in GSTN Debug Options (gstn.Loco.Stance*). Variant 22 in the drop-down ("w2 wide feet").
- Clip AS_GSTN_RunStart_Generic_w2s = copy of w2 + curves (L in f0-2 out f35-50, R in f0-7 out f47-59 of 62), rows 22/32
  (runstart_stancewidth_ue.py via Save-Assets, ESITO OK, 4 rows). w2 untouched (approved lock).
- Captures Saved/RunStartWide (mv_run_start_v5, views az180/az90/az0/game): w2base and w2s ASSERT OK. Feet 10 cm further apart
  v87-v150, back to the loop by v171 (lateral_ab.txt); heights equal; planted drift same (planted_drift.txt); evidence slide
  L 98.8/99.5 R 74.5/74.4, x200 969.4/970.0 (Saved/RunStartWide/*.evidence.txt). A/B mp4 + grid in docs/notes/reviews/runstart-w2s_h1.
- Gemini: two agy-bridge attempts timed out (old IDE conversation not resumable from CLI); owner pasted its answer:
  docs/notes/reviews/runstart-w2s/gemini.md (5 cm right, no pop at hand-off; turns: min-separation clamp 12-15 cm, pivot foot out).
- Side: dropkick Gemini advice docs/notes/reviews/dropkick-kick-k5/gemini-k6.md (ff486dfa). Crouch pop: merged 9828bca5, build OK,
  crouch_pop_timed r1 ESITO PASS (spam_fix mesh 0.00 root 0.00 spikes 0, 24 crouches; spam_old root_step 30.54, 24 spikes;
  Saved/CrouchPop/r1/check.txt). Assistant on Offset Root (rule 14): it reacts to a real component move (Interpolate mode decays it): docs/notes/research/unreal-assistant-crouch-pop.md.
## Open / to the owner
- Owner 19:03Z OK on 22 ("vai") + rule: every capture also from the SIDE (az90), not only behind. Step 2 = new thread: turnarounds w2t* -> copies with curves (or StanceMinHalf 12-15 cm per Gemini, the axis
  already follows the root yaw), and the jittery arms at 90/180 (cause first: Cascadeur curve noise, blend, rotation retarget).
COSTO: ~8 $ | ~30 min desktop | 0 token review
