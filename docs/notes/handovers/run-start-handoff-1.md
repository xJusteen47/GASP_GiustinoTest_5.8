# Run start hand-off 1 (desktop _ue59, Opus, 2026-11-19 21:00-21:35 UTC) - partial fix, then owner: restart from stock GASP Mover
Branch claude/anim-setup-3 (continued from cb72849c). Build OK (Saved/Logs/ci/build_rsh1.log, exit 0). Lock released.
1. Measure (Saved/Captures/setup4_runstop/mv_run_stop/frames.jsonl, scratch rs_measure.py: loc delta x 60, combat.anim weights):
   stamp 21 frames = 0.336 s under 35 cm/s (f22-42), capsule at 450 cm/s by f63; AS_GSTN_RunStart_Frantic (in place) kept full weight
   until clip t 0.80 (f72), then GASP M_Relaxed_Transition_Sprint_to_Run_Lfoot. = "stays in place too long".
2. Fix (3a624e42, d357bfe3): UGSTN_RunPresentationComponent::UpdateRunStartHandOff + GetAnimOwnerRunStartHandOff; data in
   FGSTN_RunStartScrambleSettings (anim set) "Run Start|Hand-off": first contact_l/r rising edge after the stamp, max 0.15 s after.
   run_start_states.py v3 exit rule (Saved/RunStart/run_start_states.txt: clip curves contact_l, contact_r, phase; rule compiled).
   Result (Saved/Captures/rsh1/mv_run_stop, same scratch script): blend out starts f61 at clip t 0.617 (was 0.80): only ~0.15 s
   earlier, still into a GASP transition (Walk_to_Run_Rfoot). Not reviewed, not for the owner.
3. Root motion (clip-check metrics.json root_travel_cm): RunStart_Frantic 0, RunStop L/R 125 vs capsule ~360 cm in 0.92 s
   (setup4_runstop capture), Skid_180 45. Owner rule: root motion must match the sim (ambito: all locomotion clips).
4. Review criteria: rubric.md `vita` and `root` FAIL axes + locomotion section; critico-estetico.md updated.
5. Unreal Assistant q5-q6 (docs/notes/assistant/): exit = stamp end + foot plant, clip time only as fallback; clips need root motion
   (stride warping / distance matching divide by it); on re-entry force-interrupt MM continuing pose.
6. Architecture draft docs/notes/abp-architecture.md: Mover setup = State Controller + chooser Blend Stack (MM node off, MM only
   refines start frame by pose history). Our rows: fixed start time, no pose match. Owner's dedicated table:
   ExperimentalStateMachineData/CHT_MoverCharacterAnimations (his edited 5.7 copy in GaspMoverProject, 2026-09-29).
7. Why stiff (git): run start r3 = Blender Python (author_runstart_r3.py, 27 keys @30 fps, root locked); run stop V4/V5 =
   run_stop_gen.py from card poses; fix_clips_ue splice eases first/last 0.2 s of every bone to the neighbour pose (~half the run
   start); captures before anim-setup-4 v2 showed GASP's stop (no gstn.RunStop.Mode 1).
Owner decision pending (sent 21:2x): 1 base (A new stock copy ABP_GSTN_Locomotion2 / B revert in place), 2 clips (A GASP mocap
restyled + pose-match rows / B Cascadeur with sim root motion), 3 sim (A keep run stages, align curves / B off). Recommended 1A 2A 3A.
Queue after the base: skid trailing foot slides + no blend in/out (owner 11-19), GASP stop after skid, skid exit leg snap, capsule
12 cm float, dust lump, skid uphill, air crouch landing, refactor 4-7, camera P0.
Uncommitted assets from anim-setup-4 left as found (clips, L_Test_Hub, PM_*). Cost: ~35 min desktop, Opus, no subagents.
