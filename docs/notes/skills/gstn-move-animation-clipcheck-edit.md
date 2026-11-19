# Proposed edit to the account skill `gstn-move-animation` (2026-11-19, clip-in-isolation gate)

For the coordinator to propose with `propose_skills` (kind improvement, target gstn-move-animation). Insert as a new
step right BEFORE the step that imports the clip into Unreal / captures the in-game video (in the Checks section if
the skill has one).

```markdown
### Clip in isolation: hard gate before Unreal (owner 2026-11-19, all clips)
A clip that fails on its own never passes in the full setup, however much the setup is iterated, and it can break
clips that already work. Before importing, routing or capturing in game:
1. Add the clip (and the clips it hands to / takes over from) to `Tools/AnimPipeline/clipcheck/clips.json`.
   Editor-only clips: export first with `Tools/AnimPipeline/clipcheck/export_clips_ue.py` (headless, editor closed).
2. `python Tools/AnimPipeline/clipcheck/clip_check.py <clips.json> docs/notes/reviews/clip-check <clip>`
   (offline, `pip install bpy numpy matplotlib`, cloud OK). One grid + metrics.json per clip.
3. Any FAIL (planted foot slide, feet through the floor, one-frame flips/snaps, loop seam, handoff pose gap, missing
   GASP curves) is fixed in the clip or in the tool that generated it, never with a per-clip patch in the game setup.
   Only then go to Unreal. Warnings ("one-frame extremes") go to the reviewer to look at.
Thresholds, the image and calibration on GASP clips: `Tools/AnimPipeline/clipcheck/README.md`.
```
