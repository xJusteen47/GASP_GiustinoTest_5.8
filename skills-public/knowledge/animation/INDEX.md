# Animation knowledge index (level 1)

## Read this first
- **Authoring or generating:** [06-holds-and-life.md](06-holds-and-life.md) (what "lifeless" looks like and how it is measured), then [checklists.md](checklists.md).
- **Reviewing:** [review-rubric.md](review-rubric.md); open a family file only to cite or to reason about a criterion.
- **Measuring:** [09-measurement.md](09-measurement.md); `python ../../tools/animkb/kb_motion.py metrics <clip.bvh>`.

Frames are at 60 fps and counted from 1. Numbers are indicative ranges with a reason, not targets.

| Family | File | One line |
|---|---|---|
| P-TIME, P-SPACE | [01-timing-spacing.md](01-timing-spacing.md) | contrast of fast and slow; spacing follows force; strikes accelerate into contact; no 50% breakdowns; strike poses readable; no spikes at joins; camera-space spacing; spacing by mass |
| P-ARC | [02-arcs.md](02-arcs.md) | extremities on arcs; no zig-zag; FK vs IK paths; parabola in the air; fixed horizontal velocity in the air |
| P-WEIGHT, P-CONTACT | [03-weight-contact.md](03-weight-contact.md) | balance, shift before lift, load before push, absorb after contact; planted feet do not slide; re-plant one foot at a time |
| P-ANTIC, P-SETTLE | [04-anticipation-action-settle.md](04-anticipation-action-settle.md) | prepare, compress (not delete) for games, frame 1 is a new pose; anticipation opposes the line; designed recovery |
| P-OVERLAP | [05-overlap-secondary.md](05-overlap-secondary.md) | parts never start and stop together; lead and lag along the chain; secondary has a cause; every part arrives from where it was |
| P-HOLD, P-LIFE | [06-holds-and-life.md](06-holds-and-life.md) | no dead frames, moving holds; seven measurable signatures of interpolated animation |
| P-POSE, P-STRETCH | [07-pose-silhouette.md](07-pose-silhouette.md) | silhouette, line of action, every game view, no twinning, line of action reverses through a strike; volume, stretch on speed |
| P-GAME | [08-game-clips.md](08-game-clips.md) | isolation first; root motion matches the body; loops; responsiveness; hand-off poses; hit-pause; engine data; clean export; yaw-only root; attack frame data; strength tiers |
| P-MEASURE | [09-measurement.md](09-measurement.md) | four-step judgement; isolation report fields and calibrated ranges; figure list |
| Checklists | [checklists.md](checklists.md) | start, stop, skid, pivot, loop, jump, landing, return to idle, attack, hit reaction, idle |
| Rubric | [review-rubric.md](review-rubric.md) | ten axes; LIFE, WEIGHT and ROOT are FAIL axes; verdict format |
| Sources | [sources.md](sources.md) | books, courses (as general references), datasets |

## Figures (`figures/`)
`A-*` = measurements on free CMU motion capture vs a lifeless rebuild of the same clip (keys every 0.2 s, smoothstep).
`D-*` = synthetic diagrams. List and criteria in [09-measurement.md](09-measurement.md#figures-in-this-folder).
