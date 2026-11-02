# Spin and drill handovers (per-thread file, from 2026-11-02)

Earlier entries: HANDOVER 63-66, 70-75 and 78 in `docs/notes/takeover-state.md`. Numbering here: SD-01, SD-02...

## SD-01 (laptop, 2026-11-02): state after D4-D11, next is D9+D10

- feature/gas-npp-5.8 = ae928e8 (fast-forward, owner OK): D4 ground/air spin, D5 opposite cancel, D6/D7 core seams (S1 re-hit
  interval, D-1 outbox event key), D8 dive, D11 minimal presentation, Handover 78. Backup: claude/project-thread-vfuzsj-rebased.
  claude/project-thread-vfuzsj is the stale pre-rebase copy: ignore it.
- Gate run: build with GSTNSpinDrill enabled + RunTests GSTNInput+GSTNCombat+GSTNSpinDrill+GSTNDefence+GSTNLink+GSTNParts = 943/0
  (Saved/D4/headless-rebase2.log on the laptop). NOT run: plugin-off build (no plugin file changed outside GSTNSpinDrill except
  GSTNCombatCore/NP, compiled in the on-build), and no harness scenario (SUITE_SPIN in Tools/GASNPPTest/spin_drill_scenarios.py,
  needs the editor and Input.Button.Crouch in IC_GASNPP_Test).
- Design decisions and deviations: docs/notes/spin-drill-doc-deltas.md sections D4, D5, D6/D7, D8, D11.
- Laptop build recipe: back up the LF .uproject (Saved/D4/uproject.local.bak, SHA256 E48D6E45DC87...), insert a GSTNSpinDrill entry
  after the GSTNLink one with an LF needle (a CRLF needle silently skips it), Build.bat ... -BytesPerUnityCPP=1, then UnrealEditor-Cmd
  RunTests; check the log has GSTNSpinDrill paths; restore the .uproject and its hash. Wait until no UnrealBuildTool / UnrealEditor
  process of another thread runs (one build per machine). Write new headers BEFORE starting UBT.

## Next: D9+D10 (latch and bore), C++ first

- Wait for M7's generic GSTNLink commit "relationship filter in the probe" (owner-approved plan); GSTNLink stays read-only for spin.
  GSTNCombat: one open change at a time (M7 G5 seam, then G3): no core edit planned for D9.
- Pieces: the drill tip hit carries LinkType Combat.Link.Type.Drill (declare type + roles Driller/Drilled in GSTNSpinDrillCore);
  UGSTN_LinkTypeDefinition for it (asset via a setup script when the editor is free; owner did not answer the asset question: C++ part
  first); latch rule per type (check FGSTN_LatchRules is not hard-wired to Type.Latch, plan stop condition); driller role parked,
  bActsWhileLinked true, YawLimitDeg 0 (M7 G2 default 360 turns it with the stick).
- In GSTNSpinDrill: ReadDiveOutcome turns a latched link into the Latch request (D8 bounces every hit today); UGSTN_GA_DrillBore adopted at
  latch, its loop held by a plugin track hook stall (defence hold-window pattern) while crouch is held, tip hitbox with
  RehitIntervalFrames = FGSTN_BoreTuning::RehitIntervalFrames (S1 counts held frames); end rules already in UGSTN_DrillMath::ResolveEnd;
  launch = UGSTN_DrillMath::ComputeLaunchVelocity via an instant effect; the drill presentation wobble already reads Seating.
- Harness for D9/D10 per plan: drill_latch_static_prop_multi_hit, drill_release_early_pop, drill_launch_after_bore, drill_target_destroyed,
  drill_latch_moving_enemy, drill_hit_while_latched_detaches.

## Cost of this thread (estimate)

One long Opus session (D4 through D11 plus two rebases): very high, far past the new 150k-context rule; the next thread should stop
after one checkpoint.

## SD-02 (laptop, 2026-11-02): triage of the flaky latch check, D9 part 1 (drill latch as a GSTNLink type)

- Commit on feature/gas-npp-5.8 (on d05d767): "Spin and drill D9 part 1: the drill latch as a GSTNLink type". Deltas:
  docs/notes/spin-drill-doc-deltas.md sections "D9, part 1" and "Triage".
- Triage (M7 OPEN 7, `latch_big_enemy_host_latches_client`): NOT the D6/D7 seams (with RehitIntervalFrames 0 the ring key is the group,
  byte for byte; no other spin commit touches a file outside GSTNSpinDrill, which those runs do not enable). The check was at its limit
  before the rebase (3, 4, 4 counter steps; limit 4) and is 3-5 after: a margin flake. No code change. Lead to decide: per-hit count or a
  10-run distribution before touching the bound.
- D9 part 1 (C++, owner's asset question unanswered: code first, as the mount): tags `Combat.Link.Type.Drill`, `Combat.Link.Role.Driller/
  .Drilled`, `Combat.Immunity.Drill` (29 tags); `UGSTN_LinkRule_Drill` (Core); `FGSTN_DrillLinkType` (GAS, registered at PostEngineInit:
  driller parked, acts, yaw 0, predicts, hit break on the driller, leash 300); the dive tip's hit carries the drill link type; the phase
  machine reads `FGSTN_DrillLinkView` (Pending -> AwaitLatch, Active -> Latch, refused -> Bounce; latched: link broken by a hit -> detach,
  otherwise target gone -> launch; hitstun/KO detach; crouch held = dive timeline running) and releases its own link through GSTNLink's
  `MarkPendingBreak` when its bore ends. New plugin edge GSTNSpinDrill -> GSTNLink (public API only; GSTNLink and GSTNCombat untouched).
- Gates (laptop, Saved/SD02): build with GSTNSpinDrill on `build-on2.out` "Result: Succeeded" (attempt 1 failed on a C4459 shadowed name in a
  test, fixed); RunTests GSTNInput+GSTNCombat+GSTNSpinDrill+GSTNDefence+GSTNLink+GSTNParts `headless-on2.log` 950/0 (943 at SD-01 + 4 new +
  M7's link tests), no ensure; the 4 new tests pass; .uproject restored (SHA256 E48D6E45DC87...); check_modules.py 0 problems. Plugin-off build not run (no file outside GSTNSpinDrill changed). Harness not run (needs the bore and
  `Input.Button.Crouch` in IC_GASNPP_Test).
- OPEN for the owner (one message): (1) driller placement is a fixed offset (0, 0, 100) over the target's root: a per-latch anchor (tip
  where it hit) needs a GSTNLink seam (a per-link offset in the link state); OK to ask M7/lead for it? (2) a link type asset (DA_LinkType_Drill
  via a setup script) instead of code: keep code? (3) "not drillable" is the tag `Combat.Immunity.Drill` until D12 (penetration vs hardness).
- Next thread (D9 part 2, then D10): `UGSTN_GA_DrillBore` adopted at the latch (track hook stall while crouch is held, tip re-hit interval
  = `FGSTN_BoreTuning::RehitIntervalFrames`, `HitsLanded`, damage scale), launch and pop velocities (`ComputeLaunchVelocity`, instant effect),
  outbox events, Launch refresh; harness scenarios of the plan; D10 moving target (corrections per latch, stop condition > 3).
- Cost of this thread (estimate): one Opus session, about 140k context, two builds of ~20 min; within the 150k rule.
