# gstn-anim-executor: project layer (GSTN)

Generic method: `skills-public/anim-executor/SKILL.md`. Section numbers below refer to it. This file wins on conflicts.

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
- **Deterministic zero-token steps:** run `runstart_pivot_gates.py` (incl. the contralateral arm–leg phase MEASURE `phase_arm_leg`, not blocking until calibrated), `clipcheck`, headless UE import (`Save-Assets`), `runstart_batch_captures.ps1` / `Run-Timed.ps1`, grid extraction and `capture_assert.py` as deterministic scripts before calling any reviewer model.

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
3. **Gate: inherited blocks BLOCK (changed 2026-11-24, INV-10; the 2026-11-23 rule that let them pass is revoked).**
   A failing `animpipe check` block that fails with the SAME line on the approved base (K8 r1: legibility, contact_faf,
   hitstop_snap 14->15, identical on K7) is reported as inherited in `gate_report.json` and still blocks the capture.
   Only two ways out: a) the check measures wrong: fix it in `animpipe/checks.py` (or the script it runs), recalibrate on
   the approved version and write the proof (log + command) in the handover; b) the base really has the defect: fix it in
   the variant, or the owner writes an exemption row in `docs/notes/anim-approvate.md`, section `## Esenzioni dei controlli`
   (`| <base asset> | <block> | <exact failing line> | owner <date>: "<words>" |`; a thread never writes it). The
   hitbox-sync file must still name the edited clip.
4. **Arm jump warning.** `animpipe check` warns `arm_jump <bone> fA->fB` when a hand or elbow (chest frame) moves
   >= 20 cm in one frame and >= 2.5x its neighbours, outside the hitbox-active frames (K8 r1: goal-post closing in one
   frame). Spread it over 2+ frames or add a breakdown; an intended snap goes in the card's `exemptions` (`arm_jump`).
5. **Hitboxes never in review videos.** Every run sets `gstn.Combat.Debug.Hitboxes` itself (0 review, 1 `hitbox_view`),
   since cvars are never undone and whiffs of a hitbox run now precede hits of review runs; otherwise
   `anim_capture_move` refuses (`stage: hitbox_cvars`). The pack fails if a review grid or video carries hitboxes;
   hitbox videos only in `<out>/hitbox/` (owner only).

## Life and speed (2026-11-23, skill audit option A; owner: same quality as the OG, speed comes from tools, never from cutting the review)
6. **Anti-stop gate on every clip, blocking.** List EVERY clip the montage plays in the run's `clips` (startup, loop,
   settle, landing). `anim_capture_move` runs `gstn_anim_mcp/no_stop_gate.py` on each joints dump (`clip_joints_dump_ue.py`
   first; a missing dump blocks): no run of more than 3 steps with the whole body under 3.5 cm/frame outside the holds,
   and with `no_stop.startup {clip: "a-b"}` no still step and largest step <= 4x the median. Thresholds are parameters
   (`no_stop.stop_cm/still_run/max_ratio`, rule 15c); `no_stop.holds` only for holds the owner wants. A clip shared with
   the approved base is judged again and blocks ("ereditato"): the approved version stays locked, fix it in a variant
   (first case: K6 settle, still ~6 frames on the ground, v158-163). There is no switch to turn it off: a false alarm is
   fixed in `animpipe/curves.py` with its cause written down. `animpipe check` `stop_and_go` now blocks on every card
   (tangent builds too) and is never inherited. Manual run: `python Tools/AnimPipeline/gstn_anim_mcp/no_stop_gate.py
   Saved/AnimPipeline/runstart/joints_<clip>.json [--startup=a-b] [--holds=a-b]`.
7. **Life images in every pack.** `anim_capture_move` writes, for every move: `slowmo_<seq>_az90.png` (the startup one
   tick per cell, base band over variant band), `velocity_<variant>_az90.png` and `_az180.png` (trails of hands, feet,
   head, pelvis with a velocity arrow per frame, red STOP dots, and the per-frame SPEED STRIP under the trails), and
   `pack.json` with the reviewer's order (slowmo, velocity sheets, every grid) plus the `slow4x` mp4 paths. Grids are never
   dropped to fit 8 images: `over_budget: true` goes in the handover. Commit them with the out_dir.
8. **Before/after for an instruction:** keep motion trails on the touched keys AND add the velocity sheet of the whole clip
   (`anim_motion_overlay`, holds from the card), so a stop created outside the touched keys is seen.
