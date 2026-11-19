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
| feet | judged on the ground: root-motion clips as they are, in-place clips shifted by `game_speed_cms` (0 = capsule still; missing = feet not judged). Planted = ball within 1 cm of its flat-foot height and slower than 30 cm/s. FAIL: a planted ball creeps > 3.5 cm within 0.25 s, or a flat foot travels on the floor for >= 0.2 s ("skate"). Shorter floor sweeps >= 5 cm (touchdown/lift-off) are WARN. `slide_ok` frames = WARN (a skid). |
| ground | ball > 1.5 cm below its flat-foot height (3.2 cm; foot bone 9.4 cm, UEFN mannequin) |
| pops | a bone > 45 deg off the midway pose of its neighbours (one-frame flick), a one-frame step > 20 deg and > 3x the steps around it (snap), pelvis jolt > 25000 cm/s2 at 30 fps |
| seam | loop: last vs first pose > 3 deg on the main bones |
| handoff | main-bone local rotation gap > 35 deg or feet (vs root) > 45 cm against the reference pose (WARN from 10 deg / 5 cm) |
| curves | required GASP curves of the kind missing (contact_l/r, phase; never MoveData_Speed on our clips), or Enable Root Motion off on a clip whose root travels |

Calibrated 2026-11-19 (anim-setup-3) on 13 GASP clips: idles, run loop, crouch idle, run stops L/R, run start, pivots F->B L/R,
landings light L/R and heavy. All PASS; the first thresholds (height-only plants, 10 deg handoffs) failed 8 of them.
Desktop without pip bpy: run it inside Blender 5.2 (`blender -b --factory-startup --python <wrapper> -- clip_check.py ...`, the
wrapper sets sys.argv after `--` and adds a matplotlib folder to sys.path). The UE export takes no args when paths have spaces.

WARN (look at it, not gating): one-frame extremes 6-45 deg (fast cycles at 30 fps produce these legitimately), contact
curve agreeing < 85 % with the geometric plant, in-place clip without `game_speed_cms`, no sidecar.

Calibration: the GASP reference rows (`GASP_*`) go through the same check. A GASP clip that fails means the threshold
is wrong, not the clip: tune the constant, write why in the commit.

## The image (one per clip)
Top: side-view sticks (blue = left) with ground ticks fixed in the world every 25 cm (a planted foot stays on its tick),
plant markers, red titles on pop frames. Then: ball height vs flat-foot (plants shaded), planted-ball ground speed,
body angular acceleration with pop frames in red.
