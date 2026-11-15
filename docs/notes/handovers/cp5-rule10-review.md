# Handover: rule-10 cloud review of the cp5 captures (2026-11-15, cloud, Fable, ~$18 est., not measured)
Branch `claude/project-thread-c9tzis` = `claude/integ-cp3` 3d9567e5 + docs only (4 `review.md`, 9 `rv_*.jpg`, `cp5-data/tools/`).
Pushed here, not on integ-cp3: this session may push only to its own branch. Fast-forward merge.
Nothing goes to the owner as PASS. 15 grids looked at (jab 4, dropkick 4, down-air 5, hit cue 2). Double-shot not reviewed (spawn bug).

**Verdicts** (each `docs/notes/reviews/cp5-*/review.md`)
- `cp5-jab-chain` FAIL: gioco end squat v83-v126 (known); gioco 30 Hz startup; posa jab 2 contact reads like the jab 1 hold from behind.
  Jab 1 poses, jab 2 first frame (fixed since r1) and acting at FAF (v58 → v59) are clean. Pose rounds used on jab 2: this is round 2 of 3.
- `cp5-dropkick` FAIL, game side only (no pose round): landing drop v102 → v103 (known 53 cm), no get-up v129 → v132 (known),
  30 Hz through the whole air hold. The air part passes all four pose questions.
- `cp5-dair-v9` FAIL: gioco leg jump after the air montage ends (v88 in one run, v79 in the other); dati montage: landing during the
  move keeps playing the air tail on the ground v102-v120; gioco 30 Hz. **Base vs variant is void**: same asset by values
  (`anim-finish-4.md` line 30), poses identical; the two folders are two runs of v9.
- `cp5-hit-cue` FAIL: dati montage: KIT_Example Attack hits at timeline frame 5-6, the clip's push arrives about 20 frames later, and the
  attacker's first frames do not change pose; gioco: victim shows a wrong pose for one frame on the hit frame (v26; also dair hit v91);
  gioco tuning: victim shake is about 1 px, not visible; no spark (no VFX assets, known).

**New game-side items for the code queue** (not pose rounds)
1. Montage shown at 30 Hz: position goes `0 2 2 4 4 6 6 …` for the first 6-10 frames of every move, for the whole dropkick hold loop and
   for one whole down-air run. Count: `cd docs/notes/reviews/cp5-data && python tools/count_repeats.py` (jab 12/59, dair_base land 16/32,
   dropkick 5/49 outside the loop). Pixels confirm dead frames: `python tools/rv_diff.py ../cp5-jab-chain/jab_jab1_jab2_faf.mp4 14 side`
   (v23 0, v25 0). Suspect: the montage presentation clock snaps to integer sim frames that arrive two at a time. Highest priority: it is in every move.
2. Victim one-frame pose on the hit frame. 3. Down-air: fall pose arrives at a different frame in two runs of the same asset;
   land-cancel for aerials missing. 4. Game camera cuts the body after the dropkick lands.

**Applied choices (rule 6)**: down-air judged as two runs instead of stopping on the void comparison; grids and four small scripts
committed so the numbers can be redone; reviews in Italian as the reviewer skill template asks.
**Owner decision proposed (1)**: landing during an aerial. A) a landing section per aerial, as in Smash (recommended); B) cut the montage
on touchdown and use the Anim BP landing. Until decided nothing changes.
**Next**: code thread for item 1, then recapture; one pose instruction for jab 2 (hand at hip height at a10-a15, visible on the right
from behind); fix the example attack timing in the CP3/CP5 setup script; a real down-air variant before any new comparison.
**Limits**: jab and dropkick are whiff only (contact, hitstop not judged); no jab base; hit-cue centroid numbers measured inline.
