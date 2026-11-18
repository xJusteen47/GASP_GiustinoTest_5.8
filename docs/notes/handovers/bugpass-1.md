# Bug pass 1 handover (cloud, Opus, 2026-11-18)
Branch claude/project-thread-k9sj5a from claude/project-thread-98dml5 3e88c938. NOT compiled (cloud). _ue59 not touched.
- Report: docs/notes/bugpass-1.md (7 findings: 1 P1 fixed, 1 P2 decision, 5 P3 open; scope reviewed and not reviewed listed there).
- Fix B1: run start scramble lean spring diverged at <= 30 fps (GSTN_LocomotionLean.cpp, sub-steps <= 1/120 s like the lean solver).
  New test GSTNCombat.NP.RunStartSkid.ScrambleLowFps. Evidence: Python port of the loop (numbers in the report).
- Decision for the owner (via coordinator): B2 Skid.MinStage 0-based vs BoostTargetStage 1-based in the same DA.
  A) rename the field's display name only (recommended, no data change)  B) make it 1-based + migrate DA values on desktop.
- Next: desktop thread runs "Desktop checks" 1-2 in the report (build + 1 test, then 30/20 fps run start look) and merges into 98dml5.
- Next bug-pass checkpoints (cloud): GSTNDefence + GSTNLink; CombatSimComponent/CombatTick_Victim/snapshot; GSTNInput(NP) + GSTNCamera.
Cost estimate: ~5 $.
