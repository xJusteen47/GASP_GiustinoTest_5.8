# Run-start (frantic) — Mixamo clip survey

Read-only survey, 2026-11-19, desktop machine, Blender 5.2 in background mode. No Unreal, no _ue59.

**Source:** `Downloads\38833FF26BA1D.UnigramPreview_g9c9v27vpyspw!App` holds 2 zips
(`Male Locomotion Pack.zip`, `Sword and Shield Pack.zip`) = 61 FBX (2 are the X Bot skin).
Full list with sizes: `file_list.txt`. FBX files are not committed.

**Run-like clips (name has run/sprint/dash/rush/charge/start/jog): 3.** No sprint, dash or start clip exists;
all three are looping run cycles, so none is a run *start* as delivered.

| clip | frames @30 | s | steps/s | arm swing L/R (deg) | speed m/s |
|---|---|---|---|---|---|
| standard_run | 23 | 0.73 | 2.73 | 124 / 106 | 4.42 |
| sword_and_shield_run | 22 | 0.70 | 2.86 | 111 / 30 | 4.15 |
| sword_and_shield_run_2 | 17 | 0.53 | 3.75 | 20 / 46 | 4.39 |

Numbers: `run_clips.csv`, made by `survey.py` (command:
`blender -b --factory-startup --python survey.py -- <clip.fbx> <outdir> <tag> [--norender]`).

**Method / caveats**
- Contacts = first frame of each run of frames where the lower of foot/toe height is under 20% of that foot's range
  (cyclic, last frame dropped when it repeats the first). One cycle = 2 steps, so steps/s = 2 / cycle time.
- Arm swing = largest angle between any two upper-arm (LeftArm/RightArm) directions over the clip.
  `arm_fwd/back_max_deg` = sagittal pitch from straight down; back values are inflated when the arm is
  abducted (pitch is unstable near horizontal) — use the swing amplitude, not these.
- Clip FBXs have no mesh; grids drive the pack's X Bot skin with the clip action. Side view, ortho, camera
  follows the hips. 8 evenly spaced frames, 512 px cells (`*_grid.png`, built by `grid.py`).
- Only `sword_and_shield_run_grid.png` was looked at (renders correctly); the other two were not viewed.

**First read for the Overlord (not a decision):** `sword_and_shield_run_2` has the highest cadence (3.75 steps/s)
and the shortest cycle, the most "frantic" timing; its arms barely swing (shield/sword hold), which fits a
heavy armoured character but not a flailing start. `standard_run` has the biggest arm swing. A run start would
have to be built (e.g. first 1–2 steps of run_2 with a deeper lean), none exists in these packs.
