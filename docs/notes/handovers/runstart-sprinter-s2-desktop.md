# runstart sprinter s2 + footstep foley + debug menu (desktop, 2026-11-21): s2 built, ASSERT OK, Fable PASS -> owner
Branch claude/runstart-generic-desktop (continues 808b2b7e). Lock _ue59 13:42-13:51Z; owner closed and I reopened the editor.
DA_Locomotion_Default (owner's uncommitted change) untouched. Re-check (rule 11): g3/s1 "ASSERT OK" confirmed (assert.txt of each).
## Done
- s2 = Variant 9/19 with the brief recipe (runstart_generic_compose.py, TAG=s2 ...). Torso 42-46 deg f4-f14 (s1 fell to 32 by f14),
  pelvis ~80 cm (one-off on generic_s2_pose.npz, neck_01 vs pelvis_MainPoint). 63 f. Casc RunStart_Generic_s2.casc (copy of s1, keys
  60-62 added, range 0-62); 0 backward knee frames, min 6.4 cm ahead. g3 (7) and s1 (8) untouched as clips.
- Foley: runstart_generic_g_ue.py add_foley() copies the stock notifies of M_Relaxed_Run_Loop_F (BP_AnimNotify_FoleyEvent_Run_L/_R,
  tracks Footstep Left/Right, notify properties + event settings, threshold 1e-5 like stock) at each contact 0->1 frame with the foot
  < 15 cm. RSGG_FOLEY_ONLY=1 adds them without re-import (used for g3). s2: L 4/15/37/60 R 10/26/48; g3: L 3/11/26/49 R 7/17/37/60
  (runstart_foley_check_ue.py -> Saved/AnimPipeline/runstart/foley_check.json, same class/side/tag Foley.Event.Run as stock).
- In-game firing: derived from the capture's per-frame clip times (frames.jsonl, crossing of each notify time with weight > 1e-5):
  s2 fires L4 v91 (w .58), R10 v102, L15 v110, R26 v128, L37 v146, R48 v163; then the loop BS every ~17 f. One per plant, none extra
  in the churn. ONE double: loop R at v160 (w .18) + s2 R48 at v163 = same plant at the start->loop crossfade (stock threshold 1e-5;
  stock clips do the same in any blend). Not an engine notify log (no cheap hook): proxy only.
- Debug menu (owner ask): SGSTN_DebugOptionsPanel now also lists gstn.RunStart.* and gstn.Loco.Base; the Variant row shows a name
  ("Variant: sprinter s2"). Cvar help lists 9. Real build OK (19 s, both modules compiled, Saved/Logs/ci/build_s2.log).
- Capture s2: ASSERT OK, min 1.00 f97-140 (Saved/RunStartGeneric/s2_h1/assert.txt). Travel f0-200 1017.0 = base. Per-contact foot
  travel (z<13, f80-240, metrics_s2_h1.json): L max 32.4 (s1 28.6), R max 31.5 (= s1); the long ones are loop-side contacts.
- Divergence row 21b updated (s2; foley is stock, no divergence).
## Review (Fable, life first; docs/notes/reviews/runstart-s2_h1/review_fable.md) + my look at az45 v86-v120
- PASS, all 3 traits. Push readable ~v88-v104, ~0.13-0.15 s per "ta" (s1 ~0.1), elbows back at v92-98. s2 better than s1.
- Notes (non-blocking): gioco v86-88 fast fold under the idle blend (known code-queue item); posa v92-98 drive plateau a bit static
  in az90; dati montage loop-side contacts 31-32 cm.
## Open / to the owner
- Pick: g3, s1 or s2 (or keep several). Foley double at the hand-off: options A) leave stock (recommended, rule 14), B) threshold 0.5
  on our clips + stock loop (divergence), C) per-foot 0.15 s debounce in the foley receiver (Unreal Assistant suggestion).
- s1 has no foley (kept untouched); RSGG_TAG=s1 RSGG_FOLEY_ONLY=1 adds it in 10 s if the owner keeps s1.
COSTO: ~6 $ | ~9 min desktop | ~97k token review
