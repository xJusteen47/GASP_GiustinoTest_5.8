# <Project> style profile (level 2) — template

One file per project, kept in the project's repository. It **tunes** level-1 ranges and states taste; it does not
restate principles. Every rule carries its **scope** (all animation, one character, one move type, one phase, VFX
only...) and its origin (who decided, when). Swap this file and the level-1 base works for another game.

## 1. North star
One sentence and 2-3 reference works with what to take from each (behaviour, timing, palette...).

## 2. Animation
| Item | Value | Scope | Tunes |
|---|---|---|---|
| Timing scale vs realistic | e.g. 20-40% shorter, longer holds | all | P-TIME-02 |
| Mass class of each character | e.g. medium-heavy | character | P-TIME-02, P-WEIGHT-03/04 |
| Exaggeration / stretch amount | | all | P-STRETCH-02/03 |
| Responsiveness budget | e.g. pose change on frame 1 for attacks | input actions | P-GAME-05 |
| Review views | e.g. game camera (behind) + side | all | P-POSE-03, P-GAME-08 |
| Interpolation space rules | | authoring | P-ARC-03 |
| Frame-count convention | e.g. from 1 | all | — |

## 3. VFX
| Item | Value | Scope | Tunes |
|---|---|---|---|
| Behaviour vs palette choice | | all VFX | V-MATTER, theory §7 |
| Palette, saturation ceiling | | all VFX | V-READ-05/06 |
| Reference observations | what was seen in which reference | | — |

## 4. Owner rules with scope
`<rule>` — scope: `<...>` — origin: `<who, date>`.

## 5. Approved references
Approved versions (locked) and the reference clips reviews compare against.
