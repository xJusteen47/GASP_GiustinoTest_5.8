---
name: gstn-anim-executor
description: Apply ONE animation edit instruction (istruzione.json) to a GASP_Giustino move card with animpipe, run the gates, and return before/after plus PASS/FAIL in esito.json. Also captures the baseline and final in-game video. Use when a reviewer hands over an instruction, or to drive Antigravity as the executor. Never decides what to change and never judges the result.
---

# gstn-anim-executor

Plain text on purpose: Antigravity reads it by path. Two layers, read both, in this order:
1. `.claude/skills/gstn-anim-executor/project.md`: GSTN commands, contract, git rules. **It wins** where they differ.
2. The generic method: `skills-public/anim-executor/SKILL.md` (with `skills-public/shared/`).
Never copy or edit the generic core for project needs: project specifics go in `project.md` only.

**INVARIANTI (non toccare per costo):** `project.md` section `## INVARIANTI` and `Tools/AnimPipeline/contract/invarianti-review.md`
(INV-01..INV-12). The final review (level 2) is never shortened to save cost.
