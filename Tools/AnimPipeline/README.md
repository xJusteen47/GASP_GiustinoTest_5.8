# AnimPipeline — move animation pilot (Cascadeur → Unreal)

Working folder for the combat-move animation pilot: Cascadeur scenes, motion models, the Unreal round trip, the checks and
the review pages. The code and settings are versioned here (`Tools/AnimPipeline/`); the data is not. Start here; the current state is in `handoff.md` and the move logs in the working folder (not in git):
- `jab_log.md` and `ganon_ftilt_log.md` are in Italian, for the owner;
- this README and the code docstrings are in English.

Coordinates:
- **Cascadeur**: X = character's left, Y up, Z forward.
- **Unreal component space**: X left, Y forward, Z up.
- Positions convert by swapping Y and Z.
- Clips are 60 fps; the game ticks at 62.5 Hz, so anim frame = round(game frame × 0.96).

## Working folder and repo copy

- **This folder (`Tools/AnimPipeline/` in the repo) is the source of truth** for scripts, rules, thresholds, move sheets and these docs.
- The scripts run in a **working folder** that also holds the data (Cascadeur `.casc`, FBX, `sheets/`, `refs/`, the handover and logs). By default that is `Saved/AnimPipeline/` in the project, which git ignores.
- Scripts find the data next to themselves, or in `$ANIMPIPELINE_DIR` when it is set (the scripts that run inside Cascadeur or Unreal use it too). Set it if you run the scripts straight from the repo folder.
- `sync.sh push` copies the tracked files repo -> working folder; `sync.sh pull` does the reverse for a quick edit made there (then review `git diff`). Data files are never copied.
- Not in git, on purpose: `.casc`/`.fbx`, renders and videos, `refs/ufd` (third-party GIFs and frame data: fetch them with `ufd_fetch.py`), logs and per-take data, the handover and token data.
- Requirements: Python 3 with numpy and matplotlib for the checks; Cascadeur (script server) and Unreal 5.8 (editor Python) for the steps that drive them; the idle clip `M_Neutral_Stand_Idle_Loop` of the UEFN mannequin.
- `role_tokens.py` reads Claude transcripts from `$CLAUDE_PROJECT_TRANSCRIPTS` (default `~/.claude/projects/<project slug>`).

## How to extend

- **New move:** add an entry to `move_sheets.json` (class, contact, FAF, effector, hold window, strike limb); copy `ganon_ftilt_v2_motion.py` as the model and `ganon_iter.sh` as the driver (`TAKE`, `CASC`, `ASSET`, `MODEL`).
- **New check:** a module with a `check(frames, sheet, TH, ...)` returning flags, called from `motion_check.run`; put its thresholds in `motion_thresholds.json` and add the rule to the table above and to `fix_rules.md`/`.json` (parameter to change, step, limits, escalation).
- **New fix rule:** `fix_rules.json` plus the matching knob in `auto_fix.py`; the model must read it through `MODEL_OVERRIDES`.
- **Thresholds** are calibrated on jab v11 and Ganondorf ftilt v1: re-run both after any change; results must not move unless the change is meant to.

## Owner's standing rules (details: memory `anim-review-rules.md`, skill `gstn-move-animation`)

- **New version, new file.** Never overwrite the owner's `.casc` or an earlier version: every version is a new `<move>_vN.casc`.
- **Unreal assets** go only under `/Game/AnimPipeline/Pilot/`.
- **Entry and exit.** A move is entered FROM the upright standing idle (`M_Neutral_Stand_Idle_Loop`, rig points in `idle_points.json`).
  - Frame 0 already shows a strong change into the move's stance: guard, load or chamber.
  - The recovery rises back toward the upright idle by the FAF.
  - Exceptions: crouched and aerial moves.
- **Six views.** Every key pose is reviewed from six views (front, right, back, left, top, bottom), plus 3/4, the game cameras, videos and elbow close-ups.
- **Exaggeration.** Joints must look natural; an out-of-range joint is accepted only as an exaggeration inside the action frames and along the motion.
- **Big pose changes.** Before a relatively big pose change beyond what was asked, describe it and wait for approval (waived for jab v11 and Ganondorf ftilt).
- **References first.** Fetch UFD references first (`ufd_fetch.py`); GIF downloads are pre-approved.
- **FAF.** FAF = the UFD total read as a 0-based game frame (jab: anim 18; Ganondorf ftilt: anim 37).

## One pass of a move (Ganondorf ftilt is the template)

```
ganon_ftilt_v2_motion.py   model: rig points per frame + key plan       -> <take>_targets_raw.json
preflight.py               the checks on the model's points, < 0.1 s     -> sheets/<take>_preflight.txt/.json
sparse_apply.py            Cascadeur: keys, interpolation, weights       (TAB, SAVE_AS, TARGETS)
fingers_fist.py            Cascadeur: closed fists on the key frames      (TAB, FRAMES)
export_current.py          Cascadeur: baked FBX next to the .casc
import_take.py             Unreal: import as /Game/AnimPipeline/Pilot/<asset>
legibility.sample()        Unreal: bone samples -> sheets/raw/<take>_legbones.json
wrist_fix.py               fists in line with the forearms -> <take>_targets.json, then a second pass
clearance.py               self-penetration (physics-asset capsules)
tangency.py                tangencies per key pose and view (report)      -> sheets/<take>_tangency.json/.txt
motion_check.py            weight, spacing, hold, overlap, arcs, joints, arms, balance, posing rules
legibility.py measure      body-part legibility over the camera rings
```

- **Driver:** `ganon_iter.sh`, with env vars `TAKE CASC ASSET MODEL DENSE=1 SMOOTH=12-36 CHECKS=1`.
- **Timings:** each step's wall-clock time goes to `sheets/<take>_timings.tsv`. A pass is about 16 s, legibility about 130 s.
- **Final renders:**
  - `view_sheets.py <take> "f:label,..." [a-b] [elbows=f,..]`: six views, hands, elbows;
  - `legibility_shots.py`: game cameras, mesh and parts side by side;
  - `traj_overlay.py`: motion paths;
  - `visual_check.py`: silhouettes and videos.
- **Review page:** `ganon_page.py` / `legibility_page.py` build `sheets/*.html`, published as Artifacts.

Keys:
- every layer is keyed on the same frames (the rig's IK chains span layers);
- overlap lives in the model's curves;
- `DENSE=1` keys every frame (what Unreal receives anyway);
- `sparse_apply.py` also supports sparse plans (per-layer keys, lags, interpolation, weights), with the caveats in `handoff.md`.

## Checks, where each rule lives, and its thresholds

| Module | What it judges | Thresholds |
|---|---|---|
| `preflight.py` (before any pass) | On the model's rig points, approximate: limb bend from lengths (reach, elbow direction), balance/lift-off/support knee/kick weight, key plan (same keys and lag on every layer of an IK chain, Bezier overshoot and drift, clamped mid-motion keys), strike spacing table, recovery fast-out/slow-in, linear motion (constant speed over 4 frames), landing absorption (kicks: pelvis dips >= 1 cm after the foot lands), overlap peak order and simultaneous arrivals, arcs (straight paths, reversal clusters, one-frame pops, twinning, head, foot slide). Floor = the idle's foot heights. Calibrated on Ganondorf v1 (catches all known errors; knee 93–99° vs Unreal 89–99°) and jab v11 (1 marginal flag) | `motion_thresholds.json` + constants in the script |
| `tangency.py` (report) | Tangencies (Solid Drawing): two parts' outlines within 1.5 cm (screen-scaled), within 15 deg, over > 25% of a limb's length; joint pairs excluded (shoulder, elbow, wrist, hip, knee, ankle, neck). Six ortho views, 3/4s and game rings, at the key poses. Offline on the cached bones, 30-45 s | constants in the script |
| `spacing_rules.py` (from preflight and motion_check) | Recovery: speed peak in the first 60% of hold end -> arrival (fast out), last approach step <= 0.6 x the largest of the last 4 (slow in to the guard) | constants in the script |
| `clearance.py` | Gap between capsules (≥ 2 cm) | in the script |
| `legibility.py` | Attacking part distinct at contact; rear arm / arms hiding it; rear elbow; fists closed; attacking limb readable from the Follow, rail and PvP cameras; exaggeration out of the body. `configure(take)` switches to the kick mode (foot as the attacking part) from the move sheet | `legibility_thresholds.json` |
| `motion_check.py` | Weight (punch: centre of mass into the strike; kick: hip drive from the wind-up), impact spacing, hold/recoil, overlap cascades, arcs and zig-zags (snaps, holds and the wind-up reversal are beats), attack line | `motion_thresholds.json` |
| `joints.py` (from motion_check) | Wrists, knees over the toes, ankles, foot roll/floor, support knee on one foot, airborne kicking foot | `motion_thresholds.json` (`joint_*`) |
| `arm_check.py` (from motion_check) | Elbow direction (from 40° of upper-arm elevation), flare, chicken wing, reverse bend, forearm/hand roll and roll jumps | `motion_thresholds.json` (`elbow_*`, `twist_*`) |
| `balance_check.py` (from motion_check) | Centre of mass inside the support base (flat foot or toes), 2 cm tolerance; kicks: upper body leans back relative to the hips | `motion_thresholds.json` (`balance_*`, `counterbalance_*`) |
| `pose_rules.py` (from motion_check) | Weight shifted before lift-off, support knee 10–45° on one foot, kick kinetic chain, no twinning, head toward the target, planted-foot slide | `motion_thresholds.json` |
| `wrist_check.py` | Wrist flexion/deviation and frozen fists | `motion_thresholds.json` (`wrist_*`, `frozen_*`) |

Per move (`move_sheets.json`):
- `move` class (jab/tilt/smash: hold frames, recoil);
- `contact`, `faf`, `effector`, `hold_window`;
- `strike_limb` (`leg_l` for kicks), `strike_side`, `crouch`;
- `balance_exempt_frames` (declared lunges, falls, airborne frames).

## Light-model iteration (owner, 31/10)

- `fix_rules.md` / `fix_rules.json`: for every flag, the model parameter to change, the direction and step, the limits, the re-check, and when to escalate.
- `iter_worker.md`: the brief for a Sonnet/Haiku subagent that runs the fix loop. In `pre` mode it is files only (model + pre-flight). In `full` mode it runs `ganon_iter.sh` with `PREFLIGHT_GATE=1`, only after the owner's go. It escalates to Opus on no rule, a repeat, a broken check or a pose-design change, and returns a summary of at most 25 lines.
- `auto_fix.py <model> <take> [--sheet=KEY] [--max-evals=N]`: fix_rules as code. It runs the model in memory, makes bounded knob changes (PEL_Y, BAL_S/W, LEAN, HEEL, ANK_ADJ, lags, CHEST_YAW, arm reach, elbow poles R4), and keeps a change only if the weighted flag cost drops. The contact, snap, hold and FAF are protected. Output: `<take>_overrides.json` (the model reads it through `MODEL_OVERRIDES`) and the residual `sheets/<take>_autofix.txt`. Test: a broken v2 (10 flags) recovers to 2 (hand arcs) in 2.5 s. Ganondorf v1 stays at 6, because its model lacks the pelvis and balance knobs and its knee needs a redesign.
- **Parallel checks:** `legibility.py measure` and `tangency.py` spread the frames over processes, leaving 6 cores free (`LEG_WORKERS=1` forces single-core). Results are identical. Legibility: v11 96→31 s, v1 123→37 s. Tangency: v11 14→4.6 s, v1 21→6.4 s.
- `role_tokens.py <session> <take> <label> [from] [to]`: tokens per model (subagents marked `sub:`) -> `sheets/<take>_roles.tsv`, to compare with the Opus-only baseline in `token_costs.md`.

## Other tools

- **References:** `ufd_fetch.py` (frame data and hitbox GIFs from ultimateframedata, cached), `ufd_measure.py` (extension, spacing, hold, recoil from a GIF), `refs/ufd/`.
- **animationsight** (external, owner-installed, second opinion only): `bvh_from_bones.py` in the animationsight output folder writes a sampled clip as BVH. Then run `animationsight inspect <bvh> --unit cm --up y --kind oneshot` with `HOME`/`USERPROFILE` set to the venv's `home` folder (never `--show`). Its side-view frames show the centre of mass over the feet clearly.
- **Idle pose:** `idle_points.py` turns an Unreal pose (bones) into Cascadeur rig points through each point's bone, calibrated on jab v11.
- **Unreal helpers (editor-side, via `ue.py`):**
  - `capture_ue.py`: captures;
  - `ue_subframe.py`: sub-frame sampling of a clip, for slow-motion behaviour;
  - `ue_tangent_test.py`: Sequencer per-key tangents from Python;
  - `ue_save_*.py`: save one asset;
  - `ue_find_idle.py`, `ue_export_idle.py`.
- **Cascadeur probes and tests:**
  - `tangent_test.py`: sparse keys, easing weights, sub-frames;
  - `phys_*.py`, `snap_dialog_*.py`, `toggle_*.py`: physics tools;
  - `inspect_scenes.py`, `metrics.py`.
  - MCP Plus (`cascadeur-plus` tools) covers scenes, layers, keys, physics (`enable`, `priority_frame`, `snap`), actions and UI input.
- **Cascadeur script server:** Scripts → MCP → Start script server. It does not autostart; MCP Plus autostart is not installed (it needs admin rights).

## Legacy (jab passes, kept for the record)

- `jab_v1_build.py` … `jab_v6_build.py`, `v6_*.py`, `v7_motion.py` / `v7_iter.sh`, `v8_*.py` / `v8_iter.sh`, `v9_*.py` / `v9_iter.sh`, `v10_*.py`, `v11_*.py`.
- `v10_iter.sh` is the generic jab pipeline (`TAKE=v11`).
- `ganon_ftilt_motion.py` is Ganondorf v1.
- Throwaway scenes: `tangent_test.casc`, `ganon_ftilt_v1_unbake.casc`, `ganon_ftilt_v1_autophys.casc`, `jab_*test*.casc`.
