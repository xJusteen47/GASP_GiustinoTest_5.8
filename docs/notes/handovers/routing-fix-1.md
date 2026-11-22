# Routing fix 1: our ABP states reach the visible pose (desktop _ue59, Opus, 2026-11-19) - DONE for Skid / Run Stop, crouch clips empty
Branch `claude/crouch-veloce-2` (on top of 3443c8b0). Brief: coordinator note (owner "sistema tutto quello che puoi"), handover crouch-veloce-2.

## Cause (read from the ABP, Saved/Routing/dump*.txt, Tools/GAS/routing/routing_dump.py + graph_dump.py)
- `DDCvar.LocomotionSetupMover` defaults to 1 (Config/DefaultEngine.ini) = BlendListByInt_0 child 1 = TwoWayBlend (Alpha 1, always update children).
  GASP's own comment: the "State Controller" machine "is used as a purely logical state machine, and outputs no pose"; the Blend Stack
  is the visible pose, written ONLY by each state's On State Entry -> SetBlendStackAnimFromChooser(State Name) -> root chooser
  CHT_GSTN_LocomotionAnimations (column StateName). Our states (Skid, Run Stop, Crouch *) held Sequence Evaluators and had no entry function.

## Fix (option A, owner GASP ANIM rule: raw clips as chooser rows, switching in the state machine)
- C++ `UGSTN_LocomotionStateLibrary::SetBlendStackFromStateName` (GSTNCombatNP/Presentation): one On State Entry binding for all our states;
  reads the entered state's name from the machine node and calls the ABP's SetBlendStackAnimFromChooser with it ("Run Stop" -> "Run Stop L/R"
  by the planted foot). Must use `Context.GetBaseContext()`: GetContext() is null on state entry (cap2 log). Cvar `gstn.LocoState.Log` 1/2.
- `Tools/GAS/routing/route_states.py` (idempotent, chained at the end of crouch_states.py and run_stop_states.py): empties our states, binds the
  entry, new chooser `CHT_GSTN_OwnStates` (StateName + output struct: start 0, blend = old cross-fades) with clips read from
  DA_AnimSet_Locomotion_Default, root rows key -> Evaluate Chooser CHT_GSTN_OwnStates. A new state (Run Start) = its name in OUR_STATES + a row.
- Backups: Saved/Backups/Routing/. Builds: Saved/Routing/build4.log (`grep -c "Result: Succeeded"` = 1). GSTN tests not run (new file only).

## Proof (docs/notes/reviews/routing-1/, capture Saved/Routing/cap4, KIT_Test_Overlord, cvar gstn.RunStop.Mode 1)
- pose_match.txt (`python -I Tools/GAS/routing/pose_match.py <frames.jsonl> Saved/Routing/clip_tracks.json <clip> M_Neutral_Stand_Idle_Loop a b`):
  Skid vs SS_180_Turn_Root180 RMS 1.4 cm (idle 22.6); Run Stop vs AS_RunStop_Ganon_L_V1 RMS 4.0 cm (idle 11.5). Sheets looked at: lunge brake
  and the 180 turn are on screen. Entry log: `grep -c "LocoState enter" Saved/Routing/cap4/run.log.ue.log`.
- Crouch: routed (cap3 log "state=Crouch Idle key=Crouch Idle"), pose changed, BUT AS_GSTN_Crouch_{Enter,Idle,Exit} have 0 bone tracks
  (Saved/Routing/clipcheck3.txt): crouch_fk_build wrote empty clips, so the crouch shows a stiff standing pose. Fix in the crouch checkpoint.

## Open / next (each its own thread)
1. Crouch checkpoint: rebuild the clips with bone tracks + the air pitch sign fix (hanging bones), not done here (would change clips blind).
2. The Blend Stack plays clips at their own rate: the C++ clip-time getters (skid sections, run stop speed match) no longer drive the pose.
   If the review wants speed-matched timing, pass a start time / play rate through the chooser output (decision applied: real rate, as in 5.7).
3. Root chooser fallback is SS_180_Turn_Root180 (any state without a row plays the skid clip): found, left as is.
4. Run start: add "Run Start" rows after its integration; Fable video review of Skid / Run Stop poses still owed.
Lock _ue59 released. Cost: ~1 h 30 desktop, Opus, ~120k context, no subagents.
