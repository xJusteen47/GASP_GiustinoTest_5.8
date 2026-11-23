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
- **Deterministic zero-token steps:** run `runstart_pivot_gates.py` (incl. contralateral arm–leg phase check on start/additive clips), `clipcheck`, headless UE import (`Save-Assets`), `runstart_batch_captures.ps1` / `Run-Timed.ps1`, grid extraction and `capture_assert.py` as deterministic scripts before calling any reviewer model.

## After K8 round 1 (2026-11-23; owner OK 22:28; scope: combat-move cards and `anim_capture_move` captures)
1. **Pelvis tilt = re-key the legs.** Legs are authored in the PELVIS frame (`animpipe/author.py`): turning `pelvis.rot`
   swings knees and ankles with it (K8 r1: ball +16 deg at a11, shins fell under the pelvis; K7 shows the same drop).
   Give a variant card `"base_card": "cards/<kit>/.../<approved>.json"`: `animpipe check` then warns
   `pose_coherence aN` (pelvis >= 3 deg off the base, leg foot/pole unchanged) and also flags, inside one card,
   consecutive poses whose pelvis turns >= 9 deg with identical leg targets. Answer it by re-keying the leg targets where
   the feet must BE, or write why they ride along in `_changes`. Warning only, never blocking.
2. **Whiffs first, clean stage.** `pie_capture_move` (PIE_CAPTURE_RUNS) now runs EVERY whiff of every run before ANY hit
   (stable order inside a run; a run met again redoes kit, cvars and spawn reset); before each whiff the dummies are
   parked off stage and a dummy still within 15 m writes `<run>/<seq>/dummy_in_view.txt`, which makes
   `anim_capture_move` refuse the review pack (`stage: scene`). `PIE_CAPTURE_KEEP_ORDER=1` = old order (debug only).
   Untried in the editor until the K8 round 2 capture.
3. **Gate: inherited blocks.** `anim_capture_move` no longer refuses a card whose failing `animpipe check` blocks ALL
   fail with the SAME line on the approved base (K8 r1: legibility, contact_faf, hitstop_snap 14->15 of the snap, identical
   on K7). Conditions: the base asset is listed in `docs/notes/anim-approvate.md`, and the base card's check JSON exists
   (base run `"card"` or variant `"approved_base_card"`; run `animpipe check` on the base once). A new failure, or an
   inherited one whose numbers changed, still blocks. `gate_report.json` lists the inherited blocks: copy them into the
   handover. The hitbox-sync file must still name the edited clip (K8 r1's other gate failure; not changed).
4. **Arm jump warning.** `animpipe check` warns `arm_jump <bone> fA->fB` when a hand or elbow (chest frame) moves
   >= 20 cm in one frame and >= 2.5x its neighbours, outside the hitbox-active frames (K8 r1: goal-post closing in one
   frame). Spread it over 2+ frames or add a breakdown; an intended snap goes in the card's `exemptions` (`arm_jump`).
5. **Hitboxes never in review videos.** Every run sets `gstn.Combat.Debug.Hitboxes` itself (0 review, 1 `hitbox_view`),
   since cvars are never undone and whiffs of a hitbox run now precede hits of review runs; otherwise
   `anim_capture_move` refuses (`stage: hitbox_cvars`). The pack fails if a review grid or video carries hitboxes;
   hitbox videos only in `<out>/hitbox/` (owner only).
