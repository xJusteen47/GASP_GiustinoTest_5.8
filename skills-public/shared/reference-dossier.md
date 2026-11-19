# The reference dossier (animation and VFX)

Scope: moves and effects that have a **reference video** (a clip recorded by the team, or footage of another game).
Without a reference video there is no dossier: judge by principles. Used by `anim-reviewer` (§1, §3.6, §3b) and
`vfx-reviewer` (§1, §3.5, §3b). The fill-in model is `dossier-template.md`. Inspired by the idea behind
"video-decompiler" style tools: take the reference apart once, in writing.

## Why
"Far from the reference", "looks like a flat PNG": a reference watched from memory gives vague verdicts, and the builder
does not know what to measure. Taking the reference apart **once, in writing**, before building gives the builder
concrete targets (how many frames, how big relative to the character, when it disappears) and gives the critic a
yardstick that does not depend on what the builder meant to do.

## Principles (not recipes)
1. **Before the work, once per reference.** Written before the build and reused for every round and for moves or
   effects of the same class. Written by an agent that does not build (usually the reviewer on the first round, or a
   subagent). If it exists, just read it.
2. **Write what is seen, measured as far as the video allows.** Frames counted from 1 at the first frame that moves,
   in game frames (write the video fps, the game's simulation rate and the conversion). Sizes relative to the
   character (body height or width), never in pixels. What the video cannot measure is written as "not measurable"
   with the reason, never eyeballed as if it were a number.
3. **Only the parts that reference needs**: timing, shapes and silhouette, sizes, motion and curves, light and
   matter, value and colour, opacity and life (VFX), identity traits. The template is a list of things to look at,
   not a form to fill.
4. **Say what NOT to copy.** A section "where the reference is weak or does not apply to us" (different camera,
   character of a different weight, limits of a phone recording, a style that contradicts a project rule).
5. **Key frames with ffmpeg, outside the repo.** Reference grids are made with the same `rv_frames.py` as the reviews.
   Images of other games stay outside the repository; only the dossier text goes in.
6. **The critic judges against the dossier, blind.** It gets dossier, grids, identity traits and project rules; not
   the full brief, not the builder's notes, not the judge's verdict. Each deviation it cites names the dossier field
   and the frame.
7. **The dossier is not law.** A deliberate deviation (a project rule, a heavier character, the project's north star)
   is not a flaw: the judge rejects it citing the rule. Changing an identity trait stays the approver's decision.
8. **Small cost.** One dossier = one read of the reference (max ~8 images), max ~80 lines.

## Where it lives
The project layer says where dossiers and reference grids are kept. The move or effect brief cites the dossier; the
reviewer reads it first.
