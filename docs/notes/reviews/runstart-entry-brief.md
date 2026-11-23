# Review brief: run start ENTRY fix, round runstart-entry (one move per review, rule 10)
Branch claude/runstart-entry-desktop. One folder per variant: docs/notes/reviews/runstart-entry-<v>h/
- entry/grid_az90.png, entry/grid_game.png, entry/grid_az0.png: top row = <v> BEFORE both fixes (old 0.3 s entry), bottom = <v>h NOW,
  SAME frames v80-v103, every frame (the entry).
- grid_az90.png / grid_game.png / grid_az0.png: same layout, v84-v144 step 4 (rev-up and hand-off into the run loop).
- runstart_<v>h.mp4 (v60-v240). Capture ASSERT: Saved/RunStartEntry/<v>h/assert.txt = OK.
- Previous verdict (round runstart-phase, 0.05 s entry): ../runstart-<v>h/verdict.md. Identity traits: ../runstart-<v>/brief_review.md
  (v4r7) or the v4r8 brief it points to.

## What changed since the previous round (only this)
- Entry blend idle -> start: 0.1 s (6 frames) Linear, was 0.05 s CircularOut (read as a one-frame cut). Measured (frames.jsonl,
  Tools/AnimPipeline/cards/runstart/runstart_entry_metrics.py): biggest one-frame stance-foot jump at entry 12.0 -> 5.5 cm (v4r7),
  12.0 -> 5.7 (a), 7.3 -> 4.3 (c), b <= 4.4 both rounds. The start still enters on its first frame at v84 (first moving frame v82).
- Diagnosis (per-frame ABP Gait/StateName in the capture): NO re-pick of the start after v84 (same state, same Gait, clip time
  continuous). The logged clip "weight" sag v88-v98 follows max(w, 1-w) of the stock lean additive's ramp (BS_Relaxed_*_Leans
  weight w goes 0 -> 1 over ~0.27 s after the move starts): most likely one cached Blend Stack pose feeding two branches, logged
  at the larger branch weight (hypothesis, not verified in the graph). So: do NOT judge the weight numbers; judge what you SEE in v88-v98 (previous round: torso upright
  v91-v95 in b, twitch v89 in a). If you still see it, label it "gioco" and describe it frame by frame.

## Judge in this order
1. Identity traits, then life on the mp4/grids without numbers: entry from idle (6 frames: smooth but fast, no pop, no visible
   slide of the planted foot), rev-up visible from the first frames, arc, weight, torso v88-v98; then the hand-off into the loop
   (fixed last round: check it did not regress).
2. Compare with the previous verdict: each earlier FAIL item fixed / not fixed / new.
3. Technical last. Verdict PASS/FAIL; each FAIL item labelled posa / dati montage / gioco. Write verdict.md in the variant folder
   (max 25 lines), last line "What the owner will notice: ...".
