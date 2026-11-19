# clipcheck: judge one clip on its own, before any in-game work

Owner 2026-11-19 (scope: every animation clip): a clip that fails on its own never passes in the full setup, however
much the setup is iterated, and it can break clips that already work. So this check is a **hard gate** before any
capture, routing or blend work. A fail is fixed in the clip (or in the reusable tool that made it), never with a
per-clip hack in the game setup.

## Run
1. Editor assets only (crouch, run stop, skid, GASP references...): export them, editor CLOSED on that checkout,
   read-only, nothing is saved:
   `UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="Tools/AnimPipeline/clipcheck/export_clips_ue.py Tools/AnimPipeline/clipcheck/clips.json Saved/ClipCheck/export" -unattended -nullrhi`
   writes `<clip>.fbx` + `<clip>.meta.json` (curves sampled per frame, notifies, sync markers, root motion flags,
   bone track count) and `export.log`. First run: the notify-time API may need a small fix (max 2 tries, rule 5).
2. Anywhere (cloud OK, `pip install bpy numpy matplotlib`):
   `python Tools/AnimPipeline/clipcheck/clip_check.py Tools/AnimPipeline/clipcheck/clips.json docs/notes/reviews/clip-check [clip ...]`
   writes `<clip>/metrics.json`, `<clip>/grid.png`, `summary.json`; prints max 5 lines.

New clip: add a row to `clips.json` (fields documented at the top of `clip_check.py`): `kind`, `root_motion`, `loop`,
`air`, `start_ref`/`end_ref` (the clip it takes over from / hands to; `"frame": "nearest"` for loops),
`game_speed_cms` for in-place clips.

## What fails (60 fps thresholds, constants at the top of clip_check.py)
| Check | FAIL when |
|---|---|
| tracks | no animated bone (the pose comes from elsewhere) |
| slide, root-motion clip | planted ball travels > 2 cm in the world during one plant |
| slide, in-place clip | planted ball speed off the capsule speed by > 20 %, or sideways drift > 2 cm |
| ground | ball > 1.5 cm below its flat-foot height (3.2 cm; foot bone 9.4 cm, UEFN mannequin) |
| pops | a bone > 45 deg off the midway pose of its neighbours (one-frame flick), or a one-frame step > 20 deg and > 3x the steps around it (snap) |
| seam | loop: last vs first pose > 3 deg on the main bones |
| handoff | main-bone local rotation gap > 10 deg or feet (vs root) > 5 cm against the reference pose |
| curves | required GASP curves of the kind missing (docs/notes/gasp-curves.md), root motion flag != the game's |

WARN (look at it, not gating): one-frame extremes 6-45 deg (fast cycles at 30 fps produce these legitimately), contact
curve agreeing < 85 % with the geometric plant, in-place clip without `game_speed_cms`, no sidecar.

Calibration: the GASP reference rows (`GASP_*`) go through the same check. A GASP clip that fails means the threshold
is wrong, not the clip: tune the constant, write why in the commit.

## The image (one per clip)
Top: side-view sticks (blue = left) with ground ticks fixed in the world every 25 cm (a planted foot stays on its tick),
plant markers, red titles on pop frames. Then: ball height vs flat-foot (plants shaded), planted-ball ground speed,
body angular acceleration with pop frames in red.
