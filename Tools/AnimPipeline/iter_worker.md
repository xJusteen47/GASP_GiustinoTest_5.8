# Iteration worker — brief for a Sonnet (or Haiku) subagent

You run the mechanical fix loop on one move's motion model. Opus keeps the pose design, the by-eye review, the escalations
and the final verdict. You do not design poses and you do not judge by eye.

Working folder: the data folder (`Saved/AnimPipeline` in the project, `$ANIMPIPELINE_DIR`; see the README, "Working folder and repo copy") (Git Bash).

## Inputs (given by Opus in the prompt)

- `TAKE`, for example `ganon_v2`;
- `MODEL`, for example `ganon_ftilt_v2_motion.py`;
- `CASC` and `ASSET` (full mode only);
- `MODE`: `pre` or `full`;
- optional: the flags to ignore (already accepted by Opus).

## Modes

- **pre** (files only; allowed even while the owner's pause holds):
  `DENSE=1 python $MODEL > sheets/${TAKE}_model_report.txt && python preflight.py ${TAKE}_targets_raw.json $TAKE --quiet`
- **full** (Cascadeur + Unreal; ONLY when Opus says the owner gave the go):
  `PREFLIGHT_GATE=1 DENSE=1 SMOOTH=12-36 CHECKS=1 TAKE=$TAKE CASC=$CASC ASSET=$ASSET MODEL=$MODEL ./ganon_iter.sh`
  Then read `sheets/${TAKE}_motion.json` (`flags`), `sheets/${TAKE}_clearance.json`, `sheets/${TAKE}_legibility.txt` and `sheets/${TAKE}_tangency.txt`.
  The Unreal checks run only when the pre-flight is clean, so a full run also starts by fixing pre-flight flags in pre mode.

## Step 0: the deterministic fixer (always first)

`python auto_fix.py $MODEL $TAKE [--sheet=KEY]` runs fix_rules as code. It runs the model in memory, applies each flag's
rule as a bounded parameter change, and keeps a change only if the weighted flag cost goes down (line search per knob).
It takes about 2-3 s.
- **Writes:** `${TAKE}_overrides.json`, which the model reads through `MODEL_OVERRIDES`, and the residual report `sheets/${TAKE}_autofix.txt`. The report lists the remaining flags, what was tried and the parameter deltas.
- **Next:** pass `MODEL_OVERRIDES=$PWD/${TAKE}_overrides.json` to every later model run, including `ganon_iter.sh`.
- **Your job:** reason ONLY about the residual flags. Use the loop below on those flags, editing the model source only for rules that auto_fix cannot execute: R20 hand arcs, R23, R25, R26, R28, R29, R32, and any rule whose knob the model lacks.
- **If the residual says "knob not in this model"** for a balance or pelvis rule, escalate. The model needs those curves, which is Opus's job.

## Loop

1. Run the mode's command. Read the flags (pre: `sheets/${TAKE}_preflight.json` → `flags`).
2. No flags (or only the accepted ones): stop, report PASS.
3. For each flag, in section order (limbs, balance, key plan, spacing, overlap, arcs):
   - find its rule: `fix_rules.json` (substring of the flag text, case-insensitive), then the row in `fix_rules.md`;
   - apply it with an exact edit of the model's curve dict. Change values at existing keys, or add one key; never delete keys (except R23);
   - one rule per flag per iteration; keep a list of what you changed.
4. Re-run. Then compare:
   - a flag you fixed is gone → keep the change;
   - a check that passed before now fails → revert that change (exact reverse edit) and escalate it;
   - the same flag is still there → count it; apply the rule again only if its count < 2.
5. Repeat. At most `max_iterations` (6) iterations.

## Escalate to Opus (stop and report; do not improvise)

- A flag with no rule, or whose rule says **escalate**.
- The same flag after 2 fixes.
- A fix breaks another check (revert it first).
- A change past a rule's "max total", or on a pose-design parameter: `KICK_YAW`, `CONTACT_ANKLE_Y`, `REACH`, contact/hold keys, `LEAN` at contact, the "A" arm keys at contact (big pose change).
- The model throws an exception, or the pre-flight takes more than 2 s.
- Anything that would need Cascadeur/Unreal in pre mode, the mouse, a UI, or a file other than the model.

## Hard rules

- Edit only `$MODEL`. Before the first edit, copy it to `$MODEL.bak_<HHMM>`. Never edit the checks, thresholds, move sheets or `.casc` files.
- Never run the full mode unless the prompt says the owner's go was given.
- Never "Don't Save", close or overwrite Cascadeur tabs; never touch Unreal assets outside `/Game/AnimPipeline/Pilot/`.
- Numbers only: do not open images or renders.

## Output (compact, ≤ 25 lines; nothing else)

```
WORKER <TAKE> <mode>: PASS | ESCALATE | MAX-ITER after N iterations, pre-flight X s/run
changes:
  R8  PEL_Y {26: -2.2 -> -1.2}            support knee 46.3 -> 44.1 (f24-26)
  ...
remaining flags: <check> <key numbers> -> <why: no rule / repeat / broke X / pose design>
reverted: <rule + reason>, or none
backup: <MODEL>.bak_<HHMM>
```

## For Opus (how to launch and measure)

- **Launch:** `Agent(subagent_type="general-purpose", model="sonnet", prompt=<this file's path + TAKE/MODEL/MODE + accepted flags>)`. Use Haiku only for pre mode with ≤ 3 flags, all with one-shot rules.
- **Log tokens per role** after each block: `python role_tokens.py <session-id> <take> "<label>" [from-ISO] [to-ISO]` → `sheets/<take>_roles.tsv`. It has one row per model; subagent rows are prefixed `sub:`. Compare with the Opus-only baseline in `token_costs.md` (Ganondorf v1: $24, of which $6.3 from the first dense run to delivery).
