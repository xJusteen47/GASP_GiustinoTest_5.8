# Animation and VFX knowledge base

A reference for any agent (or person) that creates, edits or reviews character animation and visual effects for a
real-time game. It exists so that nobody ships a clip that is technically clean but lifeless.

## Three levels, never mixed

| Level | Folder | What it holds | Who may change it |
|---|---|---|---|
| **1. General principles** | `L1-animation/`, `L1-vfx/` | Universal craft: definitions, failure signatures, measurable criteria, figures from neutral sources. No game, character, engine asset or owner taste. | Only with a source (book, course, measurement). |
| **2. Project style profile** | `L2-profile/` | One swappable file per game: north star, timing scale, exaggeration, palette, camera, rules the owner set *with their scope*. | The project owner's decisions. |
| **3. Worked cases** | `L3-cases/` | Start, stop, skid/pivot, landing, attacks, return to idle: how levels 1 and 2 *apply* to a move type. Examples, not rules. | Anyone, citing the level-1 IDs they apply. |

Rule of precedence: a level-2 profile may **tune** a level-1 range (e.g. "our anticipations are 30% shorter") but may not
**cancel** a level-1 FAIL criterion without saying so explicitly with a reason. A level-3 case never creates a rule:
if a case teaches something general, it is promoted to level 1 with a source, or to level 2 as an owner decision.

A generic skill or public repository ships **only level 1** (plus `checklists/` and `Tools/AnimKB/`), with the level-2
file replaced by `L2-profile/PROFILE-TEMPLATE.md`.

## Stable IDs: how to cite

Every criterion has an ID of the form `<family>-<nn>`. Reviews cite **ID + frame + view**, for example
`P-OVERLAP-02 FAIL at f14-22 (side): hand peaks with the shoulder, zero lag`. IDs are never renumbered; a retired
criterion stays with the note `RETIRED: <why>`.

| Family | Topic | File |
|---|---|---|
| `P-TIME` | timing, rhythm, contrast | [L1-animation/01-timing-spacing.md](L1-animation/01-timing-spacing.md) |
| `P-SPACE` | spacing, easing, breakdowns, favouring | same |
| `P-ARC` | arcs and paths | [L1-animation/02-arcs.md](L1-animation/02-arcs.md) |
| `P-WEIGHT` | centre of mass, balance, weight shift | [L1-animation/03-weight-contact.md](L1-animation/03-weight-contact.md) |
| `P-CONTACT` | contacts, planting, foot roll, slide | same |
| `P-ANTIC` | anticipation | [L1-animation/04-anticipation-action-settle.md](L1-animation/04-anticipation-action-settle.md) |
| `P-SETTLE` | overshoot, recovery, settle | same |
| `P-OVERLAP` | overlap, drag, follow-through, successive breaking | [L1-animation/05-overlap-secondary.md](L1-animation/05-overlap-secondary.md) |
| `P-HOLD` | held poses, moving holds, dead frames | [L1-animation/06-holds-and-life.md](L1-animation/06-holds-and-life.md) |
| `P-LIFE` | failure signatures of generated / interpolated animation | same |
| `P-POSE` | silhouette, line of action, contrapposto, twinning | [L1-animation/07-pose-silhouette.md](L1-animation/07-pose-silhouette.md) |
| `P-STRETCH` | squash and stretch, exaggeration | same |
| `P-GAME` | root motion, loops, transitions, responsiveness, readability, hit-pause | [L1-animation/08-game-clips.md](L1-animation/08-game-clips.md) |
| `P-MEASURE` | how to measure, tools, calibrated numbers | [L1-animation/09-measurement.md](L1-animation/09-measurement.md) |
| `V-*` | VFX: information, shape, timing, layers, value, matter, impact, tech | [L1-vfx/vfx-principles.md](L1-vfx/vfx-principles.md) |

## Reading order by task (agent-optimised)

| You are about to... | Read first | Then |
|---|---|---|
| **Author or edit a clip** (any tool, any model) | `L1-animation/06-holds-and-life.md` (the failure signatures), the checklist of the move type in `checklists/move-types.md` | the families the checklist cites, the project profile in `L2-profile/` |
| **Review a clip** | `checklists/review-rubric.md` | the IDs it cites when you need the reasoning |
| **Generate clips with a script or a solver** | `P-LIFE` (all of it) and `P-MEASURE` | run `Tools/AnimKB/kb_motion.py metrics` on every output before anyone looks at it |
| **Author or review a VFX** | `L1-vfx/vfx-principles.md` §0 and its checklist | the project profile's VFX section |
| **Add knowledge** (course notes, a book, a measurement) | `ingestion/course-ingestion.md` | the "promote to level 1" rule above |

## What each criterion contains

`Rule` (one sentence) · `Why` · `Measure` (what to look at or compute; indicative numbers at 60 fps, frames counted from 1)
· `Broken looks like` · `Severity` (FAIL = cannot pass review; WARN = must be justified) · `Exceptions` · `Figure`.

Numbers are **indicative ranges with a reason**, not targets. A clip inside every range can still be dead; a clip outside
one can be right if the reason in the criterion does not apply. Judgement goes: intent of the move -> what the principle
predicts -> what the frames show -> where they differ and why (`P-MEASURE-01`).

## Figures

`figures/` holds two kinds of image, both regenerable by script:
- `D-*.png`: synthetic diagrams (`Tools/AnimKB/kb_diagrams.py`).
- `A-*.png`: measurements on free motion-capture clips (CMU Graphics Lab Motion Capture Database, BVH conversion by
  B. Hahne; free to use, see `figures/README.md`), each paired with a **lifeless** rebuild of the same clip from key
  poses every 0.2 s eased with smoothstep. The pair is the visual definition of what "interpolated a few poses" does.

## Sources

[sources.md](sources.md). Level-1 claims cite a source or a measurement in this folder.
