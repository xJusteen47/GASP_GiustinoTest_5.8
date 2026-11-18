# Bug pass 1: C++ movement/combat (cloud, read-only review + 1 fix), 2026-11-18

Scope (checkpoint 1 of "bug fix e test su tutto"): branch claude/project-thread-98dml5 (3e88c938, contains feature/gas-npp-5.8), C++ in
Plugins/GSTNCombat (Core), GSTNCombatNP. Reviewed in depth the code added since feature (git diff feature..98dml5, ~19.5k lines, 266 commits),
newest first: run stages, run skid (logic + layered move + combat half), air momentum, jump designer, locomotion lean + run start scramble,
run presentation component, body capsule modifier, wait tasks, pushbox/balloon/analog walk/gravity frame math (division guards).
Not reviewed (next checkpoints): GSTNDefence, GSTNLink, GSTNInput(NP), GSTNCamera, GSTNAnimation, CombatSimComponent core (693-line diff),
CombatTick_Victim, snapshot/lag compensation, editor plugins. Nothing compiled here (cloud): see "Desktop checks".

Tests: last full run 1783 Success / 0 Fail (docs/notes/handovers/merge-skid-camera-jump-1.md, Saved/Logs/ci/merge3-test1/-test2). Test names:
1656 IMPLEMENT_*_AUTOMATION_TEST in Plugins/GSTN* + Source, 0 duplicate class or test names (grep over the macros, this thread).

## Findings (severity: P1 visible/likely, P2 wrong but rare or data trap, P3 minor)

| # | Sev | Where | Bug | Evidence | State |
|---|---|---|---|---|---|
| B1 | P1 | GSTNCombatNP/.../Presentation/GSTN_LocomotionLean.cpp:153 (FGSTN_RunStartScrambleState::Step) | Run start extra lean spring: one explicit Euler step per render frame. With the default 6 Hz / damping 0.55 it diverges at 30 fps and below (omega*dt 1.26, past the stable bound); 10-15 Hz (allowed by ClampMax 15) diverge at 30 fps. LeanDeg grows to inf/NaN: layer weight stuck at 1 or NaN in the additive lean = torso locked bent / broken pose after the first run start on a slow PC (owner target: toaster). The run lean solver right above already sub-steps for this reason. | Python port of the loop: 6 Hz, 30 fps -> 1e21 after 200 frames; 60 fps -> 10.5 (bounded). With sub-steps <= 1/120 s: 9-11 deg at 20/30/60 fps. | FIXED (sub-steps like the lean solver) + test GSTNCombat.NP.RunStartSkid.ScrambleLowFps (6 and 15 Hz at 30 and 20 fps). Existing Scramble test re-checked in the Python port: still passes (max 8.93, rebound -0.23, settles to 0). |
| B2 | P2 | GSTNCombatCore/.../Movement/GSTN_RunSkidTypes.h:49 vs GSTN_RunStageTypes.h:158 | Same DA (DA_Locomotion_Default > Run Stages) counts stages two ways: Skid.MinStage is 0-based (1 = sprint 1), BoostTargetStage is 1-based (2 = sprint 1). This already bit the owner (skid never fired at base run, memory "SKID GOTCHA"). | header comments | FIXED, decision A (coordinator, rule 6, 17:47Z): display name "Min Stage Index (0 = Run)" + tooltip, stored values unchanged. |
| B3 | P3 | GSTNCombatNP/.../Movement/GSTN_RunStageSyncState.cpp:44 | ShouldReconcile ignores RampFrames/RampStage/PrevSpeed, which drive acceleration and the bump rule. A client that mispredicts only these keeps the wrong ramp until something else reconciles. | code read | OPEN, needs the PIE net harness to judge (adding them may raise reconcile rate). |
| B4 | P3 | GSTN_RunPresentationComponent.cpp:151 ApplyMainAnimClass | A profile that later has no MainAnimClass never restores the original anim class; a class that fails to load is never retried (path cached first). | code read | OPEN (presentation only). |
| B5 | P3 | GSTN_RunPresentationComponent.cpp:106 | SkidEntrySpeed not reset when a new skid starts on the frame the previous one ends (source stays RunSkid): the hold section uses the older, larger entry speed. | code read | FIXED: reset on a new skid's first enter frame too. |
| B6 | P3 | GSTN_LayeredMove_RunSkid.cpp:206-214 | Ledge stop with ExitFrames = 0 still pushes one frame at ExitSpeed; the normal path with ExitFrames = 0 has no exit at all. | PhaseAt vs ledge branch | OPEN, data edge case. |
| B7 | P3 | GSTN_BodyCapsuleModifier.cpp:213 SweptHorizontalMove | The "lift" retry is world Z (0,0,0.5), not the gravity up used elsewhere since the gravity audit. Only matters with custom gravity. | code read | FIXED: lift along MoverComp up. |

Checked and fine (no change): RunStageLogic compact serialization ranges, RunSkid FramesToRest / life cap, AirMomentum channel split and wall clamp,
JumpDesigner closed form + secant, WaitTasks zigzag packing, all divisions in Pushbox/Balloon/AnalogWalk/GravityFrame are guarded, HasAuthority uses
are all outside the simulation (event dispatch, breaks, respawn).

## Desktop / laptop checks (later thread with editor)
1. Build + `Run-Timed.ps1 -Preset Test -Tests GSTNCombat.NP.RunStartSkid`, then full `-Tests GSTN` (expect 1783 + 1 new, 0 Fail).
2. In-game: run start from rest with `t.MaxFPS 30` and `t.MaxFPS 20`, KIT_Test_Playtest: the extra lean comes in and settles (before the fix it
   should lock bent). Same at 60 fps: feel unchanged.
3. Skid at base run after the MinStage 0 data step (B2), and a skid toward a ledge with ExitFrames 0 (B6).
4. PIE net harness (dedicated + 2 clients): run start/stop/boost with packet lag, count reconciles (B3).
5. Profile swap (skin/kit) while running: anim class stays right (B4).
