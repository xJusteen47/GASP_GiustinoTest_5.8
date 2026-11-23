# runstart-generic-desktop: generic frantic run start g1/g2 built, switchable, captured; both reviews FAIL (posa round 2 of 3) (desktop, 2026-11-21)
Branch claude/runstart-generic-desktop (from claude/runloop-desktop fbb7f5a5). Lock _ue59 taken 02:33Z, freed at the end. Owner editor
closed with Close-OwnerEditor.ps1 (first call "ESITO: ERRORE Canceled" but the editor did quit; process gone) and NOT reopened by me
(no reopen script in Tools/UE59: the owner reopens it). DA_Locomotion_Default (owner's uncommitted change) untouched.
Re-check (rule 11): runloop-desktop "base loop = M_Relaxed_Run_Loop_F 88 f" confirmed (Saved/RunLoop/export/Relaxed_RunLoop_F.meta.json).
## Done
- Composer runstart_generic_compose.py (env TAG R0 N_REV ENTRY S_MIN BOUNCE LEAN_DEG LEAN_IN ARM): stock loop (dump
  Saved/AnimPipeline/runloop/loop_relaxed_dump.npz) replayed at a play rate decaying R0 -> 1 over N_REV frames, 3-frame crossfade
  from stock start f0 (startL_stock_dump.npz, imported with FbxSettings.up_axis Z), root = stock start ramp, strides scaled to root
  speed, then the loop frame for frame (exact stock loop poses). Writer runstart_generic_write_casc.py (scene must have >= N keys:
  keys.set on the last frame first). Export export_joints, bake, up Z.
- UE runstart_generic_g_ue.py (RSGG_TAG): stock curve set (contact/phase from loop at warped time, movedata_speed = own root speed,
  enable_warping = stock start). Rows runstart_generic_rows_ue.py now takes RSG_ASSET/RSG_SRC/RSG_K/RSG_REPORT: siblings of BOTH
  stock rows (L and R; with only the Lfoot sibling the chooser never picked g1: capture 1 ASSERT FAIL, walk start + loop played).
- g1 = gstn.RunStart.Variant 5 (15 with Handoff 1): R0 3.2, N_REV 16. g2 = Variant 6/16: R0 2.6, N_REV 21, BOUNCE 0.3, LEAN 22,
  ARM 1.7, LEAN_IN 5. Divergence row 21 (covers g1; g2 is the same mechanism at 6/16 - add a line if kept).
- Captures runstart_generic_captures.ps1 -Variant/-Clip/-Name/-RefName (base = stock generic_h1): ASSERT OK both, 100 % min 1.00
  f97-140 (Saved/RunStartGeneric/g{1,2}_h1/assert.txt). Planted feet f80-240 L/R (metrics_summary.json): stock 43.3/93.5,
  g1 96.8/65.0, g2 102.9/78.6. Evidence docs/notes/reviews/runstart-g{1,2}_h1 (grids, mp4, casc sheets, brief, review_fable.md).
  runstart_v4_evidence.py now reports the run start clip actually seen (clip_weight was 0 for generic tags).
## Reviews (Fable, rule 10). Entry from idle and hand-off to the loop PASS in both (trait 3). Rev-up itself FAIL:
- g1: hop instead of drive (v104-112), arms not pumping -> fixed in g2.
- g2: rev front-loaded, dies by clip f12 (torso upright, = base from v112); entry drop in 2 frames. gioco: first step half blended
  with idle (clip weight 0.29/0.5/0.87 at v85/90/95; entry blend 0.1 s + lean layer) -> code queue. dati: per-contact L foot travel.
## Next (new thread, desktop, Opus high, ~10 $). Posa round 3 of 3 (then the owner decides):
1. g3: envelope full f5-f14, ease out f14-f21 (change g in the composer: plateau instead of ss(u/LEAN_IN)*ss(1-u/N_REV)),
   ENTRY 5 (pelvis leads, chest peaks f4-5), two clear high knees on steps 2-3; per-contact L foot travel (< 25 cm ok).
2. Owner queue: Antigravity approach (conversation dc6b56c3, "3d Platofmer run start animation", read via agy session
   ea3f2d1e): sprinter drive, f1-4 pelvis -10-15 % + torso 35-45 deg + elbows back, 2 short drive steps, then open stride, assets
   +15-20 % exaggerated. It says "no root motion": conflicts with rule 14 (stock root motion) -> build as its own variant WITH stock
   root. Its 0.10-0.15 s inertialization matches our 0.1 s Handoff rows.
COSTO: ~14 $ | ~20 min desktop | ~200k token review
