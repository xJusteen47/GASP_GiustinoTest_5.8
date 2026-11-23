# gstn-anim-executor: project layer (GSTN)

Generic method: `skills-public/anim-executor/SKILL.md`. Section numbers below refer to it. This file wins on conflicts.

## Who runs it
Antigravity through the `agy-bridge` MCP (`delegate`, then `follow_up` in the same session), started by a Claude session
on the desktop. Fallback: Sonnet medium for the card edit, Haiku low for build, checks and captures.
When driving Antigravity (§4), send: "read `.claude/skills/gstn-anim-executor/SKILL.md` and
`Tools/AnimPipeline/contract/instruction.md` and `glossary.md`; answer only with `esito.json` and the picture".
Antigravity cannot run git in its sandbox: the Claude session commits card and step folder on the brief's branch.

## Contract and source
- Contract: `Tools/AnimPipeline/contract/` (`instruction.md`, `glossary.md`); it replaces `skills-public/shared/contract.md`
  (Italian names: `istruzione.json`, `esito.json`, `passo-NN/prima_dopo.png`, `inizio/`, `fine/`; refusals
  `chiave_owner, ambigua, gate, formato_scheda, serve_owner, fuori_ruolo`).
- Source = the move card `Tools/AnimPipeline/cards/<kit>/...json`; `controls` list in `<move>_build_ue.json`; locked
  parts in `approved`. Rules: `.claude/skills/animpipe-move/` (`rules.md`, `kits/<kit>.md`); commands in the
  `animpipe-move` skill. `python -m animpipe status <move>` is the authority. Run from `Tools/AnimPipeline`; every
  command prints one json line.

## Commands per step (§2, §3)
- Snapshot `animpipe snapshot <move>`; restore `animpipe restore <move> <stamp>`.
- Gates: `author`, `build`, `check`, `fidelity`. Check: `animpipe verify <move> --req=<file>`; bone dump
  `sheets/raw/<take>_legbones.json` (X left, Y forward, Z up, cm).
- Before/after: `animpipe poses <card> --over=<take before>` and `animpipe trails`, or `clip_grid.py` for lit views.
- Clip in isolation (§3, before any capture; owner 11-19 hard gate): add the clip to
  `Tools/AnimPipeline/clipcheck/clips.json`; editor assets are exported first with `export_clips_ue.py` (headless,
  editor closed, read-only), then `python Tools/AnimPipeline/clipcheck/clip_check.py <clips.json> <out> <clip>`
  (pip `bpy numpy matplotlib`, cloud OK). Commit `<out>/<clip>/metrics.json` + `grid.png` under
  `docs/notes/reviews/clip-check/`. README next to the script.
- In-game video: `animpipe render <move> --standalone [--azs=...]`; labels `v`, `m`, `a`. Curves file `curve.txt`.
- Reference dossiers: `docs/notes/reference-dossiers/`.
- Per-control keys and per-key tangents do not exist in the card yet: refuse with `formato_scheda`.

## Project rules
- Never push to the feature branch, never force-push, never save maps (except L_Test_Hub).
- Owner edited in Unreal: no build (`serve_owner`); only edits on the "Manuale" layer survive, after `animpipe capture`.
- Never start, close or configure Unreal or Cascadeur unless the brief says the machine is free; exit 4 means ask.
- Engine questions: the **Unreal Editor's built-in AI assistant** (owner 11-19; editor open). UE 5.9 tools:
  `docs/notes/review/ue59-strumenti-per-skill.md`, only those marked as tried.
- Max two attempts (rule 5); scripts print at most five lines (rule 4).

## Audit 2026-11-20 (project rules 6, 10, 14; docs: project files audit_setup_agentico.md)
- Missing tool = stop. If Cascadeur, the editor, asset saves or the machine named in the brief are not available,
  stop at the first stable point and write what is needed. Never replace the authoring tool with a Python/Blender
  generator interpolating poses: such output is `PROTO` only and never goes to review.
- Every automatic fix step passes the conservation guard (amplitude per chain, peak speed, root_travel: max -15 %)
  and ships a before/after grid. Splice at most 0.1 s and at most 10 % of the clip.
- Every capture ends with `capture_assert.py` (`ASSERT: OK`); without it, no review.
- Any change that departs from stock GASP Mover: first a line in `docs/notes/gasp-divergenze.md` (rule 14).
