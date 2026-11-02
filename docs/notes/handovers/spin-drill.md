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
