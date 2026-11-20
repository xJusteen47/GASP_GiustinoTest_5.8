# Knowledge index

General craft knowledge for agents that create or review game **animation** and **VFX**. Two separate bases, one
small file for the points where they meet. Nothing here is specific to a game: each project adds its own style profile
(template: [PROFILE-TEMPLATE.md](PROFILE-TEMPLATE.md)).

## Read this first
1. Making or editing an animation clip (any tool, script, solver or model): [animation/06-holds-and-life.md](animation/06-holds-and-life.md) and [animation/10-reference.md](animation/10-reference.md),
   then the checklist of your move type in [animation/checklists.md](animation/checklists.md), then measure with
   `tools/animkb/kb_motion.py metrics` **before** anyone looks at the clip.
2. Reviewing an animation: [animation/review-rubric.md](animation/review-rubric.md). A review that checks only cleanliness cannot pass a clip.
3. Making or reviewing a VFX: [vfx/vfx-criteria.md](vfx/vfx-criteria.md), with [vfx/vfx-theory.md](vfx/vfx-theory.md) for the reasons.
4. Effect synced to a move (impact frame, hit-pause, dust on contact): [anim-vfx-links.md](anim-vfx-links.md).
5. Always load the project's style profile after these; it tunes ranges, it never cancels a FAIL without saying why.

## Levels
- **Level 1 (here):** universal principles with stable IDs, measurable criteria, figures from neutral sources.
- **Level 2 (per project):** a style profile file in the project repository.
- **Level 3 (per project):** worked cases applying 1 and 2 to move types. Examples, not rules.

## Map
| Topic | File | One line |
|---|---|---|
| Animation index | [animation/INDEX.md](animation/INDEX.md) | criteria families P-*, figures, tools |
| VFX index | [vfx/INDEX.md](vfx/INDEX.md) | criteria families V-*, theory |
| Animation ↔ VFX | [anim-vfx-links.md](anim-vfx-links.md) | the shared points X-01..10 |
| Profile template | [PROFILE-TEMPLATE.md](PROFILE-TEMPLATE.md) | what a project's level-2 file contains |
| Tools | [../tools/animkb/](../tools/animkb/) | life metrics, figures, FBX→BVH, diagrams |

## Citing
`<ID> <PASS|FAIL|WARN> f<from>-<to> <view>: <what is seen>` — e.g. `P-OVERLAP-01 FAIL f14-22 side: shoulder, elbow and hand stop together`.
IDs are stable; retired ones stay with `RETIRED: <why>`.
