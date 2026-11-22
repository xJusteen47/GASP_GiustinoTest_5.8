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

## Intended slides (owner 2026-11-19, scope: every clip)
Some moves slide a foot on purpose (skid brake, run stop slide). Mark it ON THE CLIP, not in the game setup: add the float
curve `GSTN_FootSlide` (both feet) or `GSTN_FootSlide_l` / `GSTN_FootSlide_r` (one foot), value 1 over the slide frames, 0
elsewhere (Animation Sequence editor, Curves, Add Curve; a montage inherits it from its clip). Effects:
- clip_check: a drift or skate inside the window is WARN "(slide_ok)", not FAIL (the old per-row `slide_ok` still works).
- fixers: `floor_clamp` only lifts feet vertically, so a slide keeps its travel; `splice` touches only the first/last 0.1 s.
- game: the foot lock's pop guard (GSTN_AnimNode_BlendOutFootLock, property SlideCurve) never fires on a marked foot.
Pending: the Unreal Assistant's answer on GASP's own foot-lock curves (docs/notes/abp-architecture.md); if GASP already has
an unlock curve, the marker becomes that curve and this name an alias.

## Generic fixes (fix_clips_ue.py, rows with "fix")
| Fix | What it does |
|---|---|
| filters | removes the pops the check fails (deflick, ease_snaps) |
| splice | first/last `seconds` (0.1) of every bone track eased to the neighbour clip's pose (`"splice": {"start": {"asset", "frame"}, "end": ...}`); root and ik bones untouched |
| floor_clamp | ball/heel below flat-foot height: the foot is lifted straight up by leg IK, foot keeps its world rotation |
| ik_feet | ik_foot_l/r re-baked from the feet (always last) |
Agents save through `Tools/CI/Save-Assets.ps1 -Script Tools\AnimPipeline\clipcheck\fix_clips_ue.py [-Only rows] [-Steps fixes]`.

## Conservation guard (project rule 10, 2026-11-20)
A fixer optimises its own metric and can flatten the motion while every check passes. So each step of `fix_clips_ue.py`
(filters, splice, floor_clamp) is measured before/after by `conserve.py`: per bone of the arm, leg and spine chains the swing
(largest angle between two of its poses) and the peak turn speed (over 2 frames), the pelvis movement and the root travel.
One-frame flicks are ignored (median of 3), so a deflick is not a loss; a held pose or a whip is. A step that loses > 15 % of any
of them (on a bone that moves at least 10 deg / 90 deg/s, pelvis 2 cm, root 5 cm) FAILS: nothing of it is written, the clip's
next steps are skipped, the asset is not saved, `fix_report.json` says `guard_fail` and which bone. With `CLIPFIX_WRITE=1` the
steps are also judged together against the original ("total"). Dry runs report the per-step verdict too.
Offline on two exports: `python clip_check.py --conserve <before.fbx> <after.fbx> [report.json]` prints `CONSERVE: OK/FAIL`.
A guard FAIL means the fix is wrong for that clip: fix the clip at its source or the generic fixer, never MAX_LOSS for one clip.
First real case (dash r3, 2026-11-20): `ease_snaps` turns the right toe bend 0 -> 62 deg (f4-f9, released in one frame at f10)
into a 12 deg bump: ball_r swing -77 %. The guard refuses it; the toe release belongs in the clip.

## Return to idle (owner 2026-11-19, scope: every move that hands to an idle)
FAIL when both balls are > 2 cm up on the same frame in the last 0.5 s of a clip whose `end_ref` is an idle (or
`"returns_idle": true`). In game the blend-out foot lock steps one foot at a time (SecondStepStart >= 1).
