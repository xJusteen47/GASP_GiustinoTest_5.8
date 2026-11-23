# Dash Ganon 1: L3 boost dash clip authored offline (cloud, Opus, 2026-11-19) - r3 ready to import; Fable offline PASS on r2
Branch claude/project-thread-8hhy0v (from claude/locomotion-finish-3 78e9a005). Brief: coordinator note (owner: L3 = Smash dash, skips
the frantic start; reference handoffs/dash-ganon-ref/). _ue59 not touched. Cost: ~9 $ estimate (Opus thread + 2 Fable reviews ~190k tokens).

## Done
- Dossier: docs/notes/reference-dossiers/dash-ganon.md (traits: compact load + dust; held lunge 45-55 deg, wide split, rear leg
  straight, fists low forward; heavy settle). Straight dash from idle (the reference dashes are turnarounds; turn variant = later).
- Cloud authoring works: `pip install bpy` (Blender 5.3 module). Skeleton = docs/notes/reviews/run-start-frantic/r3/AS_RunStart_Frantic_r3.fbx.
  Generator Tools/AnimPipeline/cards/dash/dash_ganon_gen.py (all keys as tables at the top), stick renders dash_ganon_plot.py.
  `python dash_ganon_gen.py <r3.fbx> <out> --variant rN` then `python dash_ganon_plot.py <out>`.
- Clip AS_GSTN_Dash_Ganon_r3.fbx: 25 frames 60 fps, root motion = the sim's boost (610 cm/s in ~3.3 f, 2.27 m). Numbers in
  docs/notes/reviews/dash-ganon/r3/metrics.json (summary): planted slide 0.0 / 1.3 cm, ball min 2.6 cm, IK stretch 0.1 cm.
- Reviews (offline stick renders, NOT in game): r1 FAIL (r1/REVIEW-fable.md), r2 PASS (r2/REVIEW-fable.md), r3 = r2 + its 4
  non-gating fixes (not re-reviewed; r2 FBX kept as fallback, DASH_ROUND=r2).
- Curves sheet (rule 13c): docs/notes/reviews/dash-ganon/curves-sheet.md. VFX puff spec only: docs/notes/reviews/dash-ganon/vfx-puff-spec.md.
- Import (desktop, headless, _ue59 lock): Tools/GAS/dash/dash_states.py = FBX import to /Game/GSTN/Animation/Dash/AS_GSTN_Dash_Ganon
  (root motion on, root lock ref pose), GASP curves via curves_author.py (kind start), state "Dash" + alias from idle/moving/Run Start
  on GetAnimOwnerBoostDashTime in [0, 0.10) s, exit at clip end - 0.12 s or < 0; route_states.py row "Dash" (added, 0.03 s blend).

## Next (desktop thread 'Sync landing pose with dust')
1. Merge this branch; check setup_run_start_stamp.py ran (BoostDash 6 f x8 on). Run dash_states.py; check its log line "root travel"
   (~227 cm forward) and Saved/Routing/curves_author.txt. NOT run anywhere yet: first run may need small API fixes (max 2 tries).
2. Capture base (L3 with no Dash state = run) vs dash, L3 from idle, real speed + x4, side + game camera; Fable in-game review vs
   the dossier (rule 10). Only PASS to the owner.
3. Game-side (Fable r1/r2 "gioco"): the run loop must be entered ~2 f before its right-foot contact (end pose: right foot ahead in
   the air); dust burst f1-6 + trail f10-12 cover the load slide (puff spec).
## Open
- Dash ledge rule (owner 00:09Z) = code thread: brief docs/notes/reviews/run-start-frantic/brief.md lines 62-65 (ProbeFloorAhead +
  LedgeGuard during the dash window). Dash window length still open.
- Found, not fixed: the run start r3 clip's RIGHT foot points ~47 deg outward on its flat frames (twisted idle source).
- No mirrored (right-lead) clip and no turnaround variant yet.
