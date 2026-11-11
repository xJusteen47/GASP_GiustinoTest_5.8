# Antigravity animation workflow (state on 2026-11-11)

Source: an interview of Antigravity through the `agy-bridge` MCP (read-only, 2 calls), cross-checked by reading the files it
named. The work lives in the Antigravity worktree
`C:\Users\jacop\.gemini\antigravity\worktrees\GASP_Giustino2_feature\animate_3d_moves_unreal`
(branch `experiment/3d-silhouette-anim`, head `fbc03811`, many files uncommitted: card moves into `cards/overlord/{dair,dropkick,ftilt}/`,
edits to `animpipe/*.py`, `ue.py`, jab builders). Nothing there was modified.

## 1. Facts

### Rules and instructions
- The process doc is `docs/notes/anim-process.md` (backup `anim-process.backup.md`), in Italian, commit `fbc03811`. It says it replaces older
  briefs and must not change without the owner's OK. It splits roles: Antigravity authors animations, Claude integrates them (GAS, NP, hitboxes).
- Antigravity claimed project rules in `.gemini/rules/`: that folder does not exist. Its skills under `~/.gemini/antigravity-cli/builtin/skills/`
  are generic (automation, plugin...), none about animation. Its project knowledge is the process doc plus the cards' `_design` text.
- The tooling itself (`Tools/AnimPipeline/animpipe`, verbs, `locks.py`, the `animpipe-move` skill) predates Antigravity (commit `03081026`);
  Antigravity uses it and added cards, the 3-module dropkick, `assemble_dropkick_montage.py`, jab builders.

### Steps, brief to screen
1. Move card JSON: `Tools/AnimPipeline/cards/<kit>/<move>.json` (poses per frame: pelvis loc/rot, spine, head, arm hand+pole, leg foot+pole+foot_rot;
   `spacing` presets; `part_delays`; `planted`; `exemptions`).
2. `python -m animpipe author <card>`: solves poses, gate "65 bones within 0.1 cm".
3. `python -m animpipe build <card>`: `ue_rig.py` builds `LS_<Move>` with a Control Rig track, keys only the pose frames on rig controls, shapes
   in-betweens with weighted tangents per spacing profile (`RCIM_CUBIC`, user/flat tangents on pose hits, smart-auto elsewhere), lags parts by
   `part_delays`, then bakes to `AS_<Move>`.
4. `python -m animpipe check <card>`: clearance, planted feet, balance.
5. `python -m animpipe review <card>` / `ingame_judge.py <card> <hitstop>`: creates `AM_<Move>`, scrubs frame by frame in `L_PilotCapture` with the
   Duel side camera, repeats the first active frame for hitstop, writes `sheets/ingame/<move>_ingame.mp4`, `_whiff.mp4`, `_compare.mp4`, `_strip.png`.
6. Aerial moves with ground impact: 3 clips `AS_<Move>_Startup/_Loop/_Land` assembled into `AM_<Move>` by Unreal Python (dropkick only so far).

### Keying
- Sparse: v7 dair keys 10 pose frames `[0,3,8,14,15,17,20,31,37,42]` at 60 fps on the `LS_` rig controls; curves are editable there.
- The `AS_` sequence is a bake, one key per frame. The editable source is `LS_<Move>` + the card, not the `AS_`.
- Easing comes from named spacing presets (`charge_long`, `hold_recoil`, `limb_led_recovery`) and optional per-key `ease_out`, not hand-edited curves.
- Edits on the `LS_` (additive section) are read back and rewritten on rebuild (`ue_rig.py` ~l.174-233), so owner tweaks there survive.

### Overlord down-air versions (cards in `cards/overlord/dair/`, assets in `Content/AnimPipeline/Rig/Overlord/DAir/`)
| Ver | Pose frames | What changed (Antigravity's account, frames checked) |
|---|---|---|
| v1 | 0,6,12,14,16,26,34,42 | Ganondorf timing (active 14-16, FAF 42), knees tucked, Bison crossed arms |
| v2 | +13 | owner's Cascadeur f15 tuck / f23 stomp imported at anim f12/f14, forearm clearance |
| v3 | +10 | progressive trunk curl 0-6-10-12, head lag zeroed |
| v4 | 0,3,6,8,11,14,15,17,31,37,42 | owner timing: open stance, charge 5-11, release 12-14, stomp 14, settle to 30, FAF 42; variants VarA-M |
| v5 | 0,3,8,14,15,17,31,37,42 | fewer keys (not described by Antigravity) |
| v6 | +20 | extra settle key at 20 (not described) |
| v7 | same as v6 | `part_delays` chest 1, head 2, forearm 2, hand 3 (whip and settle); source anim still V6 |
- Latest is **v7** (`AS_/LS_OverlordDAirV7_Build`, 2026-11-08). **No v8 exists** in cards or Content, in any worktree checked.

### Self-check and blind spots
- Checks: bone-length read-back, `check` numbers, contact sheets / trails / legibility sheets (`Tools/AnimPipeline/sheets/`), the scrubbed video above.
- It never records a real PIE video of the move inside the ability: the capture scrubs a preview mesh in `L_PilotCapture`, no Mover motion,
  no ability timing, no gameplay camera, no network.

### What the owner corrects (Antigravity's list)
Frame 0 already contracted (wants an open stance that sweeps into the coil); parts moving as one block or snapping late; dead holds (wants a
living charge, release spread over ~3 frames); rubbery secondary motion (wants one heavy rebound and stop); details: locked Cascadeur poses,
toe point, wrist angle, jab rear-arm amplitude.

### Slow or fragile (Antigravity's list)
Unreal modal popups (overwrite, source control) block the bridge; frame-by-frame capture with sleeps is slow; Cascadeur round-trip and
`locks.py` vs. owner poses.

## 2. What could be improved (my reading)
- The deliverable the owner judges is a scrubbed preview, not gameplay: add a PIE capture of the move through its ability (the harness can
  already run headless) so review sees real timing, Mover motion and camera.
- Commit the uncommitted Antigravity work (cards moved, verbs edited) on its branch; today a review cannot pin a version to a commit.
- The process doc claims things not done for dair (3-module split applies to dropkick only); state per move which pattern applies.
- Easing is preset-only: allow per-control curve overrides in the card so a correction can target one control without a new preset.
- Version notes live only in chat: add a short `changes` field per card version (v5/v6 have no recorded reason).
- Retarget to UE 5.9 is untouched: the Control Rig bake and `ue_rig.py` APIs need a check there.

## 3. What Claude as reviewer needs
- Per variant: the in-game mp4 (PIE, through the ability) and the same-frame base video, committed under `docs/notes/reviews/<variant>/`,
  plus the card path and commit, the hit frame and FAF.
- Correction handoff format Antigravity accepts: `docs/notes/handovers/<YYYY-MM-DD>_<move>_review.md`:
  ```
  # Review: <move> (v<N>)  Verdict: REVISE | APPROVE
  - Frame <F> | part `<part>`: rot [rx,ry,rz], loc [x,y,z]   (card axes: +X char left, +Y forward, +Z up; +rx leans back)
  - part_delays: {"<part>": <frames>} | spacing: <preset>
  - Exemptions: contact <bone_a>|<bone_b> frames <a>-<b> (reason)
  ```
  Keep corrections at card level (pose frame, part, value), never per baked frame, so keys stay sparse.
