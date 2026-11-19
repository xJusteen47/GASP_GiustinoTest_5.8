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
- Knowledge base (2026-11-19): `docs/knowledge/INDEX.md` → generic rubric `skills-public/knowledge/animation/review-rubric.md`
  (LIFE, WEIGHT, ROOT are FAIL axes; cite `P-*` IDs), style profile `docs/knowledge/profile-gstn.md`, cases
  `docs/knowledge/cases/`. Isolation report `skills-public/tools/animkb/kb_motion.py metrics` is required.
