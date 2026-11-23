# gstn-anim-reviewer: project layer (GSTN)

Generic method: `skills-public/anim-reviewer/SKILL.md`. Section numbers below refer to it. This file wins on conflicts.

## Roles, model, language
- Model: Fable, high effort (project rule 3). One move per thread. Rule 10: nobody sends a move to the owner without
  having looked at the in-game video. Max 3 pose rounds (rule 10), then the owner decides.
- Owner reports in **Italian**, short. Handover of max 30 lines with the cost. Images of other games never leave the
  desktop / project files.
- North star (owner 11-19, all animations): **"Ganondorf inside Astro Bot"**, fluid and cartoony with the character's
  personality. Quality bar (owner 11-15): "on par with Smash Bros or similar AAA".

## Where things are (§1)
- Contract: `Tools/AnimPipeline/contract/` (`README.md`, `rubric.md`, `instruction.md`, `glossary.md`). It **replaces**
  `skills-public/shared/contract.md` and the default rubric: Italian field names (`mossa, ciclo, passo, tentativo,
  livello principio|misurato, vista dietro|lato|camera, non_peggiorare, chiave, tipo, parte, controllo, intento,
  modifica, principio, verifica`), result `esito.json`, refusals `chiave_owner, ambigua, gate, formato_scheda,
  serve_owner, fuori_ruolo`.
- Cycle folder: `docs/notes/reviews/<move>-c<N>/` with `inizio/`, `passo-NN/`, `fine/`, `ciclo.md`, `review.md`.
- Card: `Tools/AnimPipeline/cards/<kit>/...json` (`_design`, `poses`, `anim_active`, `approved` = locked).
  `rules.md`, `kits/<kit>.md` in `.claude/skills/animpipe-move/`. Status: `python -m animpipe status <move>`.
- Critic principle (Italian): `docs/notes/review/critico-estetico.md`. Dossier: `docs/notes/review/dossier-riferimento.md`,
  dossiers in `docs/notes/reference-dossiers/`, reference grids in project files `handoffs/<ref>/` (never the repo).
- Theory: `docs/notes/anim-theory/` (`principi-animazione-corporea.md` §13 for the four-step method,
  `animazione-picchiaduro-action.md` §10). References: `Tools/AnimPipeline/reflib/`.
- Frame tool: `skills-public/anim-reviewer/scripts/rv_frames.py` (current captures: 360 px squares per view).

## Clip in isolation (§2 gate; owner 11-19)
- Results: `docs/notes/reviews/clip-check/<clip>/metrics.json` + `grid.png` (one image: side sticks with world ground
  ticks, ball heights with plants shaded, planted-ball ground speed, body angular acceleration with pops). Tool and
  thresholds: `Tools/AnimPipeline/clipcheck/README.md`. GASP reference clips run through the same check as calibration:
  if a GASP clip fails, the threshold is wrong, not the clip.
- No result for a clip of the move or for a clip it hands to/from: ask for it, the cycle does not start.

## Capture labels and timing (§3)
- Labels `v` (video frame), `m`, `a` (anim frame); frames from the `--video` map. Sim 62.5 Hz. Designer-facing frame
  data counts from 1. Views: az180 or game camera (behind) + side. The owner looks mostly from behind.
- No Smash reference from behind: say so and judge by silhouette.
- Fail labels: `posa` / `dati montage` / `gioco` (= generic `pose` / `data` / `game`).

## Engine curves (§3.7) and engine questions
- GASP reads curves from clips and montages (foot IK lock/disable, warping, distance matching,
  `RootMotionTranslationScale` on our montages). Take the list from the project's GASP curve check named in the brief;
  missing list = "curve GASP non verificate".
- Engine doubts: write the exact question for the **Unreal Editor's built-in AI assistant** (owner 11-19, editor open)
  in the `gioco` / `dati montage` item. UE 5.9 tools: `docs/notes/review/ue59-strumenti-per-skill.md`.

## Owner-specific rules
- Rule 13a: 2-3 identity traits per move, checked first; changing them = owner decision. 13b: approved versions are
  locked (`approved/<move>-<version>` tag, `docs/notes/anim-approvate.md`); work on a variant. 13e: game constraints
  never bend the pose (move the dummy or hitbox, not the arc).
- Owner keys and parts in `approved`: never in an instruction, only as a proposal to the owner.
- Owner edited in Unreal: the re-import (`ue-import`, "integra | uniforma | cambia") does not exist yet. Tell the
  coordinator; do not ask for a build.

## Audit 2026-11-20 (project rules 6, 10, 14; docs: project files audit_setup_agentico.md)
- No review without `ASSERT: OK` from `Tools/AnimPipeline/capture_assert.py` (expected clip weight >= 0.8 in the
  expected window). Missing or FAIL -> review void, label "cattura". Until the script exists, check
  `frames.jsonl` `combat.anim` weights by hand and quote the line.
- Order: watch the video (real speed and 25 %) WITHOUT numbers first. Axis VITA: anticipation, arcs, weight on
  contact, follow-through/overlap. Axis ROOT: clip root_travel within +-20 % of capsule travel. VITA or ROOT FAIL ->
  FAIL "posa" before any technical check.
- Automatic fix applied (splice, floor clamp, filters): compare before/after grid; amplitude, peak speed or
  root motion lost over 15 % = FAIL.
- Clips marked `PROTO` (procedural Python/Blender generators) are never reviewed.
