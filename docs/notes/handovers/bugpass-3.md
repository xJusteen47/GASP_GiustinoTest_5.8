# Handover: bug pass 3, core combat sim (cloud, Opus), 2026-11-18

Branch claude/project-thread-i8mmmj, based on claude/project-thread-016vb8 (305251a4). Full report: docs/notes/bugpass-3.md.

Done
- Reviewed hit resolution, knockback, balloon, impacts/bounces/bump, hitstop, hitstun and recovered-tumble timers (list in the report).
- 6 findings: C1 P1? (bump drag outlives the tumble, ~4 s slow walk after a wall bump), C2 P2 latent (balloon move beats newer launches,
  Mover tie goes to the older move), C3-C6 P3.
- FIXED C3 only (bTumbleActionable cleared on splat / health KO / zone KO) + 1 assertion block in
  GSTNCombat.NP.Hits.Victim.TumbleAirRecovery. 2 files, 13 lines. Not compiled.

Not done (rule: timing changes go to the desktop)
- C1, C2, C5 change simulation timing: desktop check first (report section "Desktop checks"), then a small code thread.
- C4, C6 are design questions for the owner (with C1).

Next
- Desktop: build + `Run-Timed.ps1 -Preset Test -Tests GSTNCombat.NP.Hits.Victim`, then `-Tests GSTNCombat`; check C1 in PIE.
- Next bug pass checkpoint: snapshot / lag compensation, CombatTableSet, rest of CombatSimComponent; GSTNInput(NP) + GSTNCamera.

Proof: findings are code reads (file:line in the report); no counts claimed. Verify C3 with `git diff 305251a4..HEAD -- Plugins`.
