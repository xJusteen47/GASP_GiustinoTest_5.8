# Bug pass 1 handover (cloud, Opus, 2026-11-18)
Branch claude/project-thread-k9sj5a from claude/project-thread-98dml5 3e88c938. NOT compiled (cloud). _ue59 not touched.
- Report: docs/notes/bugpass-1.md (7 findings: 1 P1 fixed, 1 P2 decision, 5 P3 open; scope reviewed and not reviewed listed there).
- Fix B1: run start scramble lean spring diverged at <= 30 fps (GSTN_LocomotionLean.cpp, sub-steps <= 1/120 s like the lean solver).
  New test GSTNCombat.NP.RunStartSkid.ScrambleLowFps. Evidence: Python port of the loop (numbers in the report).
- B2 applied A (coordinator, rule 6): Skid.MinStage display name "Min Stage Index (0 = Run)", no data change. Owner may still ask for B.
- Also fixed B5 (skid entry speed reset on back-to-back skid), B7 (capsule lift along gravity up). Open: B3 (net harness), B4, B6.
- Next: desktop thread runs "Desktop checks" 1-2 in the report (build + 1 test, then 30/20 fps run start look) and merges into 98dml5.
- Next bug-pass checkpoints (cloud): GSTNDefence + GSTNLink; CombatSimComponent/CombatTick_Victim/snapshot; GSTNInput(NP) + GSTNCamera.
Cost estimate: ~5 $.
