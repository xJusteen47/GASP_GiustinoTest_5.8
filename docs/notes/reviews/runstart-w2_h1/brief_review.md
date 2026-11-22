# Review brief: generic run start w2 = approved w1 with the drive foot slide fixed (one move, rule 10, polish round 1)
Branch claude/runstart-generic-desktop. Files here: grid_az90.png, grid_az0.png, grid_game.png (top row w1_h1 = the APPROVED w1 =
base, bottom row w2, SAME capture frames v84-v180 step 4, 60 fps, same kit/sequence), runstart_w2_h1.mp4 (mp4 frame n = capture
frame v(n+60), label burnt in; tiles: top az0 | az45 | az90 | game, bottom az180 | az270 | az315), strip_az45_v86-120.png and
strip_az90_v86-120.png (every 2nd video frame v86..v120, row 1 = v86..v102, row 2 = v104..v120). Same strips of w1:
../runstart-w1_h1/strip_az45_v86-120.png, strip_az90_v86-120.png. The curves sheet of w1: ../runstart-w1_h1/curves-sheet.md.

## What changed (owner approved w1: "nuovo standard. Rifinisci lo scivolamento")
Cause found: in game the capsule ran 2-3x ahead of the clip's root during the drive, so the planted drive feet were dragged.
w2 = w1 resampled on the timing the game showed for w1, with the clip root on the capsule up to the end of the rev (clip f19), the
drive contacts pinned in the world, and the pelvis drop held 1 frame longer (released f13-22 instead of f12-19) so the pelvis does
not rise and fall back before the real rise (w1 watch A). Nothing else: same poses, lean, arms, rate.

## Identity traits (check FIRST, rule 13a) - must be the SAME as w1
1. Sprinter weight transfer: falls down-and-forward in the first ~4 frames, low through two quick drive steps, rises on the long stride.
2. Frantic limbs from frame 1, no planted pause. 3. No clash idle -> start -> loop (no pop, no hitch, no wrong foot). Knees never backward.

## Judge in this order (rule 10)
1. Life: does w2 look like w1 (strips side by side, v86-v120)? Any loss of energy, timing change, pop or hitch, especially around
   v118-v124 (the clip root goes from capsule speed back to loop speed there) and in the hand-off v136-v180 (mp4 frames 76-120)?
   Is the pelvis now low through the drive and rising once (no rise-dip-rise around v100-v110)?
2. Feet (runs with foot z < 13 cm, f80-240, horizontal path per run; one-off on metrics_*_h1.json):
   w1 L 14.3 / 20.5 / 25.6 / 28.3 / 29.3, R 1.1 / 11.3 / 33.9 / 28.8 / 24.7 cm.
   w2 L 14.6 / 5.0 / 28.7 / 28.6 / 29.3, R 1.1 / 2.7 / 20.0 / 29.5 / 24.7 cm. (L first = idle + push foot under the idle blend, gioco.)
3. Technical: ASSERT OK (w2 weight >= 0.8 on 100 % of f97-140, min 1.00). Travel f0-200 1017.0 cm (= w1).
A fix that reduces amplitude, speed or root motion more than 15 % is FAIL. Verdict PASS/FAIL, FAIL items labelled posa / dati
montage / gioco with frames, w2 vs w1 in one line. Max 8 extra images. Write review_fable.md here (max 40 lines).
