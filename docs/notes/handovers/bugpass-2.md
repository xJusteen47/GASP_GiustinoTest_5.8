# Handover: bug pass 2 (Defence + Link), 2026-11-18

- Machine: cloud only. Branch claude/project-thread-016vb8 from claude/project-thread-k9sj5a ab2746ac. Docs only, no code or asset change.
- Report: docs/notes/bugpass-2.md (6 findings D1-D6, no P1, no fix applied, desktop checks).
- Method: full read of the Defence/Link source diff since feature (git diff origin/feature/gas-npp-5.8..HEAD -- Plugins/GSTNDefence/Source
  Plugins/GSTNLink/Source ':!*Tests*'), pattern sweep of the older M6 code, full read of defence hit steps, outcome applier, link tick extension
  request/accept/partner-move paths. 208 automation tests in the two plugins (grep IMPLEMENT_*_AUTOMATION_TEST).
- Rule-6 choices (coordinator note allowed them): none needed. Chose NOT to fix D1 (neutral get-up missed when the downed end falls inside
  the bounce clip) from the cloud: it changes sim timing, so it goes to a code thread with the Landing tests and a capture.
- Most useful next step: desktop check 1 in the report (D1). If it reproduces, a small Opus-medium code thread.
- Next checkpoints of the pass (unchanged from bug pass 1): CombatSimComponent/Victim/snapshot; GSTNInput(NP) + GSTNCamera.
- To verify this handover (rule 11): `git diff --stat ab2746ac..HEAD` shows only the two docs files.
