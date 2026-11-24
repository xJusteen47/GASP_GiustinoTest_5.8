# gstn-anim-reviewer: project layer (GSTN)

Generic method: `skills-public/anim-reviewer/SKILL.md`. Section numbers below refer to it. This file wins on conflicts.

## INVARIANTI (non toccare per costo)
Owner 2026-11-24 (scope: whole animation skill and review): quality equal to the OG skill, no compromise on false
positives, savings only from automatic tools. Source with the reason for each item: `Tools/AnimPipeline/contract/invarianti-review.md`.
Only the owner changes them, with a written decision; whoever slims this skill keeps this section as it is.
`Tools/AnimPipeline/tests/test_review_invariants.py` fails if an id or a pack step goes missing.
Level 1 (0-token gates, silhouette, trails, previews) only DISCARDS a round. Level 2 (the final review) always runs on every
variant that can reach the owner, on the pack of `anim_review_pack` (`final/review_pack.json`, `complete: true`, else no review).
- INV-01 identity traits first (rule 13a); a changed trait = FAIL until the owner decides.
- INV-02 life before numbers, on the moving video: real + slow4x mp4 of base and variant before any number.
- INV-03 slow motion of the WHOLE move (startup, actives, recovery, settle), 1 tick per cell, az90 + az180 + game, no frame uncovered.
- INV-04 velocity sheets with stops and trails for EVERY clip (startup, loop, settle, landing).
- INV-05 large images of every key pose per view, base beside variant, looked at before the numbers; bone paths; OG pose checks.
- INV-06 before/after against the base on the same montage-aligned frames, inherited parts included.
- INV-07 blind critic: independent subagent, never the editor, gets only pack, criteria, brief (`final/critico_brief.md`).
- INV-08 level 1 may discard a round, never promote it: PASS comes only from level 2.
- INV-09 no image cap on level 2: the cap is "no frame uncovered"; cost caps (rule 4) apply to level 1 only.
- INV-10 a check that gives false alarms is FIXED (measure corrected, recalibrated on an approved version with proof), never
  softened or switched off; a block failing identically on the approved base still BLOCKS (only way out: the owner's exemption
  row in `docs/notes/anim-approvate.md`, section "Esenzioni dei controlli").
  A NEW check starts as a measure and blocks only after its calibration proof (owner 2A 2026-11-24; life metrics:
  `Tools/AnimPipeline/life_gate.json`, written by `life_calibrate.py --write-gate`; the others show as `WOULD FAIL`).
- INV-11 OG life metrics (impact spacing, hold + recoil, cascade, arc jitter, stops) and pose checks are part of level 2.
- INV-12 holds (owner 1A 2026-11-24): a LIVE hold (pelvis or head keep moving) is a normal tool, declared WITH a written reason
  (card `life_holds` / `why` / `hold_reason` / `hold_reasons`, capture `no_stop.hold_reasons`) and judged on the video; the
  defects are DEAD holds and stop-and-go; 2 holds / 6 frames are guides for the eye, not blocks.
- INV-13 principles are CONSIDERED one by one, never applied like an automaton (owner 2026-11-24): every entry of
  `Tools/AnimPipeline/contract/principi-review.md` (body, fighting-game and Sakurai/OG principles, each with its document) is
  written in `final/principi.md` with the four steps of principi-animazione-corporea.md §13 (intent, prediction, observed frames
  + view, deviation and whether it is intended) or closed as `non pertinente` with a reason; an intended, reasoned deviation
  stays accepted. No PASS (reviewer or blind critic) without `PRINCIPI: OK` from `gstn_anim_mcp/principi_review.py check`.

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
  `animazione-picchiaduro-action.md` §10). The level-2 principles to consider (INV-13), each with its document, are in
  `Tools/AnimPipeline/contract/principi-review.md`: open the cited section of every principle you are considering, not
  only the one you quote. Index of every research document (body, fighting games, Sakurai, feel, VFX, camera), with what
  it holds and who uses it: `Tools/AnimPipeline/contract/indice-ricerca.md`. References: `Tools/AnimPipeline/reflib/`.
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

## After K8 round 1 (2026-11-23; scope: combat-move reviews)
- Before judging: a review video or grid with hitbox shapes, or a whiff with the dummy in frame (also from behind), is
  void, label `cattura`. Hitboxes live only in `<out>/hitbox/` and never go to the critic.
- Read the `warn` lines of `<move>_check.txt`: `pose_coherence` (pelvis tilted, legs not re-keyed: look at the shins
  under the pelvis on those poses) and `arm_jump` (an arm crossing in one frame: look for the pop at 25 %). They are
  leads for the eye, not verdicts.
- Blocks marked inherited in `gate_report.json` fail identically on the approved version. Since 2026-11-24 (INV-10) they
  BLOCK: no capture happens unless the owner wrote an exemption row for that exact line in `anim-approvate.md`. Judge the
  inherited parts on the video like the rest (INV-06); a defect shared with the base is still a FAIL `posa` for the variant
  (the approved version stays locked, the fix goes in the variant). Report exempted blocks once to the owner.

## Life and speed (2026-11-23, skill audit option A; owner: same quality as the OG, speed comes from tools, never from cutting the review)
- Nothing of the final review is shortened: watch the movement, life before numbers, identity traits, comparison with the
  base, all as above. The tools below are added to it, they replace nothing.
- Look in this order: (1) the **slow4x mp4** of base and variant (`pack.json` `slowmo_mp4`), or, if you cannot read video,
  the **startup slowmo tile** `slowmo_<seq>_az90.png` (one tick per cell) as the frame-by-frame sequence; (2) the two
  **velocity sheets** `velocity_<variant>_az90/az180.png`: arrows per frame, red dots and red bands = whole body STOP, the
  speed strip (black = whole body, dashed = stop threshold) shows stops AND pops; (3) every grid; (4) real speed.
- **Blocking** (rubric `statua`, `carica_viva`): the whole body still like a statue (a DEAD hold, declared or not) or
  stop-and-go, in ANY clip (startup, loop, settle, landing), inherited clips included. A live declared hold is normal
  (owner 1A): judge on video that it reads as wanted. Life measures marked `WOULD FAIL` (no calibration proof yet) are
  not blocks but you look at those frames and cite them. A still frame in a grid is not a "good held pose"
  until the speed strip says it moves. `no_stop` failing in `gate_report.json` means no capture happened: report it.
- Inherited stops are judged again and FAIL the variant (label `posa`); the approved version stays locked, the fix goes in a
  variant. Do not file them as "ereditati dalla base approvata".
- `pack.json` `over_budget: true`: the pack was kept whole on purpose; review all of it.
