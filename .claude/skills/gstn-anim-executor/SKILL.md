---
name: gstn-anim-executor
description: Apply ONE animation edit instruction (istruzione.json) to a GASP_Giustino move card with animpipe, run the gates, and return before/after plus PASS/FAIL in esito.json. Also captures the baseline and final in-game video. Use when a reviewer hands over an instruction, or to drive Antigravity as the executor. Never decides what to change and never judges the result.
---

# gstn-anim-executor: you apply one instruction, you decide nothing

You are the executor of the cycle in `Tools/AnimPipeline/contract/README.md`. You receive **one** `istruzione.json`, apply
it on the move card, run the gates, and answer with `esito.json` and one before/after picture. You do not choose what to
fix, you do not say whether it looks good, you do not propose the next step. Anything unclear is a refusal, not a guess.

Who runs this: Antigravity through the `agy-bridge` MCP (`delegate`, then `follow_up` in the same session), started by a
Claude session on the desktop. Fallback: a cheap Claude (Sonnet medium for the card edit, Haiku low for build, checks and
captures). This file is plain text on purpose: Antigravity reads it by path.

## 1. Read only this
`Tools/AnimPipeline/contract/instruction.md` and `glossary.md`, the instruction, the card, `rules.md` and `kits/<kit>.md` in
`.claude/skills/animpipe-move/`. The commands are in the `animpipe-move` skill; `python -m animpipe status <move>` is the
authority on what exists. Not the rubric, not the theory (only the section named in `principio`, if you need it to
understand the words), not old threads.

## 2. One step
Run from `Tools/AnimPipeline`. Every command prints one json line; read the line, not the detail file.
1. **Validate.** Header and all eight fields present, `verifica` measurable, `controllo` a name from the `controls` list
   of `<move>_build_ue.json`. Otherwise `rifiuto: ambigua`, naming the field and the two readings.
2. **Map the frames** with `--video`. If the frame is an owner key, or the part is in `approved`: `rifiuto: chiave_owner`.
   If it asks to change startup, active frames or FAF, or to judge: `rifiuto: fuori_ruolo`.
3. **Snapshot** (`animpipe snapshot <move>`), and keep the stamp for the result.
4. **Apply on the card, and only there.** Use the tool named in `tipo`; at the `principio` level choose, in this order:
   curve (segment spacing profile, part delays), moving one of your keys in time, a tween between two neighbouring keys,
   a pose change on an existing key, and a new key last. A new key carries only the controls that need it and a written
   `why`. Touch nothing outside `parte` and `chiave.tratto`. Never a key on every frame; never bake a trajectory to force it.
   If the card cannot hold what is asked (per-control key, per-key tangent): `rifiuto: formato_scheda`.
5. **Gates:** `author`, `build`, `check`, `fidelity`. A blocking gate fails: one more try applying the same instruction
   more gently, inside the same part and span. Still failing: `animpipe restore <move> <stamp>` and `rifiuto: gate` with
   the failing line. Never fix a gate by changing another part: that is a decision.
6. **The instruction's check.** `req`: `animpipe verify <move> --req=<file>`. `misura`: compute it from the bone dump
   (`sheets/raw/<take>_legbones.json`, X left, Y forward, Z up, cm) and report the value. Same for each `non_peggiorare`
   line that has a number. A FAIL is reported as a FAIL: you do not retry on your own to turn it into a PASS.
7. **Before/after.** `animpipe poses <card> --over=<take before>` and `animpipe trails`, or `clip_grid.py` for lit views:
   same frames before and after, touched keys and neighbouring frames only, trails on. One picture,
   `passo-NN/prima_dopo.png`. No render while a blocking gate fails.
8. **Write `passo-NN/esito.json`** exactly as in `instruction.md`: what changed (control, tool, before, after, why), key
   count before and after, `segnali`, gate lines, PASS or FAIL with the measured value, the `ossa` numbers, the paths.
   Reply in at most five lines: applied or refused, PASS or FAIL, keys before and after, the paths. Then stop.

## 3. Start and end of a cycle: the in-game video
On request only. `animpipe render <move> --standalone [--azs=...]` after `build` and `check` pass. Put in `inizio/` (or
`fine/`) the mp4 files and grids of variant **and** base on the same frames: the version that hits, the one that whiffs,
and one sequence that acts at FAF. Frame labels (`v`, `m`, `a`) must be readable. Commit them: the review is done from
those files. Report the gate line and the paths, nothing about how it looks.
- **Curves for the reviewer** (GASP curve check, reviewer §3.7): next to the videos write `curve.txt` with the curve
  names on the clip and the montage of variant and base (editor MCP dump or import log). Facts only, no fix.
- **Reference video**: if the brief names a reference dossier (`docs/notes/reference-dossiers/`), capture the views it
  needs to be compared on (same side or rear view as the reference) besides the usual ones. You do not read or judge it.
- You never see the critic's items or the reviewer's verdict; you only receive the instruction that follows from them.

## 4. If you are a Claude session driving Antigravity
- Send the instruction file **verbatim**, plus: "read `.claude/skills/gstn-anim-executor/SKILL.md` and
  `Tools/AnimPipeline/contract/instruction.md` and `glossary.md`; answer only with `esito.json` and the picture".
  Do not rephrase, complete or soften the instruction. Do not add your own opinion of the move.
- When it answers, trust nothing unchecked: re-run `check`, `fidelity` and the `verifica` yourself (seconds), and look at
  `git status`: only the card, its outputs and the step folder may have changed. Anything else: restore and report.
- You do the git work (Antigravity cannot run git in its sandbox): commit the card and the step folder on the branch the
  brief names. Never push to the feature branch, never force-push, never save maps.
- Its `note` may contain opinions or proposals: drop them from what you pass on.

## 5. Hard rules
- One instruction, one change, one build. Max two attempts, then stop and write what is needed.
- Never move or edit an owner key, a locked part, or fingers on owner keys.
- Never write keys into a sequence in Unreal. The card is the only truth; the sequence is build output.
- If the owner says he edited the move in Unreal: do not build (`rifiuto: serve_owner`). Only edits on the "Manuale"
  layer survive, after `animpipe capture`.
- Never start, close or configure Unreal or Cascadeur unless the brief says the machine is free; exit 4 means ask.
- An engine question you cannot answer from the contract (why a gate or an import behaves as it does): ask the Unreal
  Editor's built-in AI assistant (owner 11-19; editor open) and paste its answer in `note`. If it does not settle it,
  refuse with the failing line; never guess a setting.
- Never delete a snapshot. Restore instead of repairing a card by hand.
- Scripts write to files and print at most five lines. No full logs, reports or diffs in the reply.
- No images, GIFs or videos of other games in the repo.
