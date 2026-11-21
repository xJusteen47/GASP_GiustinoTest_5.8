# Handover: review run start variants v4r8 a / b / c (cloud, 2026-11-21)
Material: claude/runstart-v8c-desktop 82b2c4d6. One reviewer subagent per variant (brief + grids + mp4 sheet + metrics only).
Verdicts: docs/notes/reviews/runstart-v4r8{a,b,c}/verdict.md (also project files reviews/runstart-v4r8{a,b,c}/).

- a (one high-knee pump, cvar 2): PASS. Rev-up ~0.27 s (v88-v104), plants hold (3.9 / 7.5 cm), arms at rest, root = v4r7.
  Note (dati montage, inherited from v4r7): skipped L step at blend-out to loop v124-v134, more visible since it comes earlier.
- b (cartoon wheel, cvar 3): FAIL, dati montage x2: wheel f1-f7 plays during the 0.3 s blend-in (weight 0 until v90) so
  it reads as one floaty hop; blend-out v118-v130 out of phase -> stutter step. Minor posa: R landing skim v112-v116.
- c (loop sped up, cvar 1): FAIL, dati montage x2: same blend-in hides the 3.5x rev-up (no visible scramble);
  blend-out v116-v129 out of phase -> floating half step v122-v134. Root motion identical to v4r7.
Common cause b/c (and a's note): stock 0.3 s blend-in over a short rev-up, and early blend-out without phase match.
Fix direction (no amplitude cut): shorter blend-in (~0.05-0.08 s) or rev-up after blend, phase-matched exit (sync marker / exit phase).
ASSERT: reported OK by capture thread; assert.txt not in review folder, not re-checked.
COSTO: ~6 $ | 0 min desktop | ~231k token review (3 x ~77k)
