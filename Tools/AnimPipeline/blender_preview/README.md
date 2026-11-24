# blender_preview: PROTO pose preview in seconds (owner choice 2B, 2026-11-24)

**PROTO only (rule 6).** It throws away wrong rounds and checks a pose before the in-game capture. It is never a review
and never goes to the owner: rule 10 judges only the in-game capture. Missing on purpose: Mover root motion and capsule,
hitstop pose, foot IK, the blend from the previous state, the hit on the dummy, hitboxes, the real mesh.

## Run (cloud or any machine, no Unreal, no Blender install)
```
pip install bpy numpy pillow          # bpy = Blender 5.3 as a Python module (py3.13); ffmpeg only for --video
. /opt/lavapipe/use-lavapipe.sh       # cloud containers: software Vulkan; on a PC with a GPU skip this
python -I Tools/AnimPipeline/blender_preview/pose_preview.py <clip.fbx> --out <dir> [--base <base.fbx>]
       [--frames 0,6,12 | v13,v21] [--every 4] [--views back,side | 180,90,45] [--at N --hitstop 8]
       [--video] [--size 480] [--silhouette] [--keep-frames]
```
- Input: the clip FBX that goes to Unreal (UEFN mannequin bones, e.g. what the authoring scripts write, or
  `clipcheck/export_clips_ue.py` for editor clips). Proxy body = capsules between the posed joints (left blue,
  right orange, `--silhouette` = flat black), rebuilt every frame, so it works for any clip on this skeleton.
- Views as in the in-game capture (`capture_ue.py`, `ingame_judge.py`): az 0 front, + toward the character's left,
  fov 55, 450 cm, elevation 12, camera following the pelvis horizontally; ground grid every 50 cm.
- Frames: anim (0 = first clip frame) or `v<N>` video frames, mapped through the hitstop like the in-game video
  (`animpipe/vframes.py`, same `--at`/`--hitstop`). `--video` writes every frame with the freeze, 60 fps.
- Output: `grid_az<N>.png` (one per view; `--base` puts base above variant, same frames), `preview_az<N>.mp4`,
  `preview.json`. Prints at most 5 lines.

## Speed (cloud, 4 CPU, software GL)
~0.5 s per image at 480 px: 9 frames x 2 views in 10 s; base + variant, 6 frames x 2 views + 2 videos in 10 s.

## Known limits
- Proxy proportions are generic (radii in cm on an 85 cm leg), not Overlord's mesh: judge lines of action, limb
  directions and silhouette, not contact or penetration.
- Keep the scene to few objects: with ~160 separate objects Blender crashed after 14 renders under lavapipe; the
  ground grid is one mesh for that reason.
- No Blender MCP: not needed (bpy module runs headless). With Blender on a PC the same script runs with
  `blender -b -P pose_preview.py -- ...` only after adapting argv; not tried.
