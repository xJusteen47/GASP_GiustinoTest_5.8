# Handover: review turnaround 180 L variant 27, round 1 (cloud, Fable)

Brief: coordinator note 2026-11-21 21:51Z (rules 10, 15a). Source `claude/runstart-generic-desktop` @ b568cf61.
Result: **FAIL**, not sent to the owner. Review: `docs/notes/reviews/turn3-v27/180a/review.md` (+ 3 sheets).

- GOOD v85-v88: upright pivot step with the L foot planted about 90 deg turned, arms tucked. Better than v26.
- FAIL v89-v97: body stalls side-on about 8 frames, torso hunched 30-40 deg, R leg hanging, no travel. Turn reads
  v85-v102 in two beats.
- Items: 1 gioco (in-game yaw lags the authored yaw after 90 deg; trace actor yaw, root offset, clip time v84-v104 and
  ask the Unreal Assistant, question in the review); 2 posa (spine drop only after frame H); 3 posa (R foot in the air
  v91-v101, re-judge after item 1); 4 posa (arms open with hanging hand v92-v97); 5 cattura (ASSERT window f97-f140
  does not cover the pivot).
- Builder's questions: (a) confirmed; (b) lean after v102 is fine, lean before it is item 2; (c) knee zigzag not visible.

Applied choices (rule 6): item 1 goes to the code queue first and does not count as a pose round; the other three
directions stay unbuilt until 180 L passes.
Next: desktop thread traces the yaw chain (item 1), then pose round 2 with items 2-4 and a capture whose ASSERT covers v85-v96.
Evidence for the numbers: frames extracted with `ffmpeg -i ab_26_vs_27_<view>_real.mp4`, v = 80 + index (label on
frame); authored yaw from FRAC in `Tools/AnimPipeline/cards/runstart/runstart_pivot_compose.py` line 30.

COSTO: ~4 $ | 0 min desktop | ~70000 token review
