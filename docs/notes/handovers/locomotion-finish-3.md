# Locomotion finish 3 (desktop _ue59, Opus, 2026-11-19) - run stop V4 + skid 4 FAIL, 2 attempts each used; next thread
Branch `claude/locomotion-finish-3` from locomotion-finish-2 (68975a98). Reviews: docs/notes/reviews/locomotion-finish-3/.

## BLOCKER (from 14:42 local): every UnrealEditor-Cmd start exits after 9 s
- Saved/Logs/GASP_Giustino2.log: `LogPluginManager: Error: ... plugin 'Fab' ... modulo 'Fab'` non trovato.
- Engine/Plugins/Fab: Fab.uplugin + Binaries rewritten 19/11 14:41:48-51, Binaries/Win64/UnrealEditor.modules BuildId 60738873, engine
  BuildId 60351404 -> module rejected. Most likely the Epic Launcher is updating Fab / UE 5.9 (a 10-min headless run hung on UBT just
  before, Saved/Logs/ci/lf3_curves.log). Tried (2 attempts, rule 5): `-DisablePlugins=Fab`, `"Fab": Enabled false` in the uproject
  (reverted): still loaded. Owner: let the launcher finish / Verify UE 5.9, then any thread can continue.

## 1. Run stop V3 - Fable FAIL, close (review_fable_runstop.md). Pose round 3 -> owner decides (rule 10)
- run_stop_gen.py V3: back foot drags on its toes to STOP, steps STOP+5..+9; pelvis -11/-12 to STOP; front foot steps back STOP+9..+18.
  Clips AS_RunStop_Ganon_{L,R}_V3 (DA + ABP rows point at V3, Mode still Off). Dust: new NS_GSTN_RunStopDust (half size/life, no grains).
- PASS parts: wide low stance whole slide, back foot timing, no pop into idle, dust small and feet readable.
- FAIL (posa): brake v196-216 a frozen sliding pose; rise lands all at once v238 + 10 still frames; v223-230 shuffle light.
  dati montage: clip ends with feet 33 cm apart, game idle ~10 -> 9 cm creep v249-279. Next: overlap pass (chest push at STOP, arms/head
  5-6 f late with overshoot, end on the game idle stance, cut the frozen tail). Capture: stick value per frame not logged.
## 2. Skid attempt 3 - Fable FAIL gioco (review_fable_skid.md); attempt 4 written, NOT applied (editor blocker)
- New state "Skid Exit Run" (Tools/GAS/skid_exit_run.py + route_states.py row, run loop M_Neutral_Run_Loop_F from t=2.117 matched
  headless, json Tools/GAS/routing/skid_exit_run.json), rule speed >= 200 via new C++ GetAnimOwnerGroundSpeed (built). Correct first
  foot, no pop. Still: freeze f164-175 at the hand-back to motion matching (left foot slides 111 cm), 42 cm drag in the 0.15 s blend.
- Attempt 4 = HOLD 0.75 s in skid_exit_run.py (committed, run it + capture mv_skid). Log state/asset/time in frames.jsonl first.
## 3. Owner's note on GASP curves: offline name-table diff (no editor): our V3 stop and run start clips carry no curves and no notifies;
  GASP stops have Foley notifies + PoseSearchBlockTransition/BranchIn notify states. Check the curve list in the editor (or ask the
  editor's AI assistant, owner rule) before more clip work: it may explain the run-to-run pose differences of the run stop.
## UPDATE 16:20 local: editor fixed (owner: UE 5.9 update + Verify + RiderLink reinstall; project rebuilt, 458 s).
- Run stop V4 (owner 2A, follow-through) in UE (DA + rows V4, Mode Off). Fable FAIL review_fable_runstop_v4.md: brake breathes, stop push
  reads, feet end 10 cm apart, no creep. BUG (mine): numbers(f,"R") in run_stop_gen.py blends ALL channels to idle by STOP+22, so the
  late trunk/head/arm keys never play in the R clip (20 still frames v239-258). Fix: blend each channel to idle only after its own
  last key (or true idle + mirror(action - idle)), ease in the push (STOP->STOP+3 "io"), cut the tail to 2-3 frames; capture L and R.
- Skid attempt 4 (hold 0.75 s) in UE. Fable FAIL review_fable_skid_a4.md: first plant slide 111 -> 13 cm (fixed) but the stick release
  (capture f193) lands in the hand-back blend: 20 frozen frames, 84 cm slide. Fix: Skid Exit Run leaves at once on release/reverse;
  start the loop ~4 frames later (41 cm exit drag); capture held 1.5 s and released early; log state/asset/time in frames.jsonl.
- Capture driver used: scratch lf3_all.py = run_stop2_write (RUNSTOP_VERSION) + run_stop_states + skid_exit_run + route_states.
## Not started: L3 dash, run start stomp height, air crouch landing. Stray empty files for the owner to delete by hand:
  _ue59 Tools/AnimPipeline/run_stop_gen.py and root IN-USO.txt. Lock released. Cost ~1 h 30 desktop, 1 build, 6 headless runs
  (2 failed on Fab), 2 captures, 2 Fable reviews (~230k subagent tokens).
