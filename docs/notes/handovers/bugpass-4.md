# Handover: bug pass 4, input and camera (cloud, Opus), 2026-11-18

Branch claude/project-thread-3f1vdx, based on claude/project-thread-pg3fen (e137330c, newest camera code). Report: docs/notes/bugpass-4.md.
Desktop list for CP1-4: docs/notes/bugpass-desktop-checklist.md.

Done
- Reviewed camera anchor + air hold (jump-stable camera), playtest camera (framing, telephoto, shift, tilt-shift), input simulator and
  source (held/pressed, mapping contexts), slot resolver and input history (buffer, consume, re-base). 11 findings, no P1/P2.
- FIXED K1 (screen band ignored the shift offset) and K2 (teleport test vs held camera; no change at default values), each with a test.
- ADDED test I4 for the held re-press rule (owner's held dodge bug, cb8163ab had none). 5 files. Not compiled.
- Owner answer 1A (C4) recorded in the desktop checklist next to C1 as the intended behaviour.

Not done
- K3 (landing catch up on uneven ground) changes camera timing: desktop feel check first. K4-K7, I1, I3: notes / asset checks.
- GSTNCamera plugin not reviewed (inactive). Snapshot / lag compensation / CombatTableSet / rest of CombatSimComponent still unreviewed
  (CP3 listed them as next; not in this checkpoint's scope).

Next
- Desktop thread runs docs/notes/bugpass-desktop-checklist.md (merge, build, tests, PIE checks C1 first).

Proof: no counts claimed. Verify with `git diff e137330c..HEAD -- Source Plugins` (3 source files + 2 test files).
