# runstart-turn-foot (cloud, 2026-11-23): P5, the turnaround entry hides the R foot spin - cause narrowed, capture ready, no fix yet
Branch claude/project-thread-vqce2q (from claude/integ-2026-11-23 e0fcf520). No editor (laptop locked by K8 r2), nothing compiled.
Re-check (rule 11): P5 weights confirmed in reviews/turn3-v27/180a_A/assert_pivot_weights.json (runstart-generic-desktop): v84 0.17 .. v89 1.0 = 0.1 s Linear of the +10 rows.
## Three suspects (R = pivot foot of 180 L, spins on its ball at clip f3-f6 = v84-v87 while contact_r = 1)
1. POP (our code, most likely): GSTN_AnimNode_BlendOutFootLock pop guard fires on a STILL foot (< 60 cm/s) that moves > 8 cm more
   than its speed in one frame. The body yaws 27-41 deg/frame at v85-v88 (face_hips, turnarounds-4): the idle R foot, 10-15 cm
   off the root, jumps ~8 cm in world -> held at its old spot + rotation, faded (1-t)^2 over 0.15 s = shown from ~v91. Matches.
2. PIN (GASP stock): CR_Biped_FootPlacement pins a foot while contact_r is up; a ball pivot keeps contact_r = 1 (pin = heel spot).
3. BLEND: weight 0.17-0.70 during the spin; the idle foot dominates f3-f5 (also why round-1 lead keys f0-f2 were never seen).
Zero-cost check first: the t4c capture on the DESKTOP (Saved/Turn4/t4c/.../frames.jsonl) already logs body.fl; fl % 10 == 4 at v84-v90 = suspect 1 confirmed without a new capture.
## Done (cloud)
- pie_capture_move.py logs per frame foot_l/_r world rotation, ball_r, curves contact_l/_r, GSTN_FootSlide(_r), enable_warping.
- runstart_turn_foot_metrics.py <batch>: per run, pop Smoothing frames of R, contact_r frames, shown spin (foot_r yaw - hips) v84-90.
## Next (desktop or laptop, ~10 min, one Play session, _ue59 lock)
  Tools/AnimPipeline/cards/runstart/runstart_batch_captures.ps1 -SavedDir TurnFoot -Views game,az90 -Runs `
   'base|28|AS_GSTN_RunStart_Generic_w2t180Ls|mv_turn_180a|88,132|a.GSTN.FootPopGuard 1;a.AnimNode.FootPlacement.Enable 1',
   'nopop|28|AS_GSTN_RunStart_Generic_w2t180Ls|mv_turn_180a|88,132|a.GSTN.FootPopGuard 0;a.AnimNode.FootPlacement.Enable 1',
   'nopin|28|AS_GSTN_RunStart_Generic_w2t180Ls|mv_turn_180a|88,132|a.GSTN.FootPopGuard 1;a.AnimNode.FootPlacement.Enable 0'
  then python runstart_turn_foot_metrics.py Saved/TurnFoot/batch. Whichever run shows the spin at v84-v88 names the cause.
## Planned fix per cause (attempt 1; the Unreal Assistant question is asked in the same editor session, rule 14)
- POP: the turnaround clips get GSTN_FootSlide_r (pivot window, curve already honoured by the guard and by clipcheck): no new code,
  divergence row only if the Assistant says stock handles ball pivots another way. Generic alternative: guard ignores a foot whose
  jump is explained by the component/root yaw that frame (code, cvar a.GSTN.FootPopGuard stays the switch).
- PIN: contact_r -> 0 over the pivot frames in the turn clips (stock rule: a foot that must move is not pinned, foot-lock-and-slides.md).
- BLEND: shorter entry only on the turn rows (+10 siblings, RSH_BLEND) since f0 is the stock idle pose; never on the forward starts.
## Owner note (2026-11-23) for the w3 turnaround thread
At gstn.RunStart.Variant 29 (w3) the row scripts add NO turnaround rows (runstart_w3_ue.py has none; chooser not opened): the stock reface plays (its planted pause is the "downtime").
The w3 180 L (cheap skill) must start the rev-up on the pivot foot's toe-off, no idle beat between turn and run.
SUBAGENTI: 0
COSTO: ~2 $ | 0 min desktop | 0 token review
