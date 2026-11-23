# Land v2: review

Inputs: `grid_az90.png`, `grid_az45.png`, `grid_game.png` (frames 100-196, one sample every 6 frames, 60 fps), `recovery.txt`, `v2_assert.txt`. Three images, each looked at once.

**Not blind.** The grids carry the row labels `BASE` and `V2`, so I knew which row was the variant. The two rows are also shot from slightly different camera positions (marker and HUD differ), so small pose differences between rows are not reliable.

**Winner: row V2. Verdict on the variant: PASS, goes to the owner**, with the open points below.

## 1. Identity

| Item | Result | Frames |
|---|---|---|
| Near-hard cut into the land | PASS | V2 is airborne at f100 and already at full crouch depth at f106. BASE is only half way down at f106 and reaches the low at f112. |
| Impact readable | PASS | V2 f106-f112: deep squat, torso folded over the knees, hands near the floor. Reads in az90 and az45. |
| Weight | PASS | V2 holds the low f106-f124 (about 300 ms). Heavy, in character. |
| Fast recovery | PASS | V2 rises f124-f142 and is upright at f148 (about 0.73 s after the land at f104). BASE is still hunched at f166 and upright only at f172-f178 (about 1.2 s). |

## 2. Life

| Item | Result | Frames |
|---|---|---|
| Anticipation | PASS (weak) | f100 air pose, legs reaching down, same in both rows. One sample only. |
| Arc / rise | PASS | V2 f124 -> f130 -> f136 -> f142: continuous unfold, hips first, then the chest. No pop at this sampling. |
| Rushed or mechanical | PASS | The rise takes about 300-400 ms after a 300 ms hold. It does not read as a sped-up clip. |
| Follow-through / settle | FAIL, **posa** | V2 f148-f196: eight samples in a row with the same upright pose, arms at the sides. No overshoot, no settle of chest, arms or head after the rise. BASE keeps moving until f178. This is the least "fluid, cartoony" part of the variant. |
| Root motion vs capsule | Not judged | The character is a few pixels tall in `grid_game.png`. No visible drift f100-f196 in either row, but this grid cannot prove it. |
| Foot skate | PASS (low confidence) | az45 f106-f196: the feet stay on the same floor marks in V2. Sampling every 6 frames at this size would miss a small slide. |

## 3. Technical: the assert window

- The pre-chosen window f110-f150 failed (83%, f111 weight 0.25). The passing window f118-f175 was chosen after seeing the data, so the `ASSERT: OK` line is not evidence on its own.
- From the frames: V2 f106, f112 and f118 show the same deep crouch in az90 and az45. There is no visible flash of another clip and no upright or walking pose in the dip f104-f117. **For the player this is not a visible problem on the sampled frames.**
- Limits of that statement: only f106 and f112 fall inside the dip; f111 itself is not in the grids.
- The explanation in `v2_assert.txt` (both states hold the same clip, the log follows the state weight) is marked "most likely" and is not verified. The failed gate names `BS_Relaxed_Walk_Leans` as owner at f111, which is a different asset, so the "same clip in both states" story is not the whole picture.
- Result: visibility PASS; gate FAIL, **gioco** (capture/gate logic, not the animation).

## Other points from the data

- `recovery.txt`: in V2 the idle loop owns the pose only at f187 (1383 ms), while the character is already upright at f148. About 650 ms of the land clip is a still upright tail. If anything waits on the land clip ending, the recovery is slower in play than it looks. **dati montage**, not verified here.
- `recovery.txt`: `ld_jump_fwd` picks a different clip in V2 (`Land_Run_Light_Rfoot` instead of `Land_Walk_Light_Lfoot`, 100 ms vs 83 ms). No grid covers the forward landing, so that change is unreviewed.

## To fix (none blocks the hand-off to the owner)

1. **posa**: add a settle after f142 (small chest and arm overshoot, 8-12 frames) so f148-f187 is not a frozen pose.
2. **dati montage**: trim or shorten the blend out of the upright tail f148-f187, or confirm that it is interruptible from f148.
3. **gioco**: make the gate sum the weight per asset across both states' Blend Stacks, or declare the window f118+ before the capture, then re-run. Log f104-f117 frame by frame once to confirm or reject the `BS_Relaxed_Walk_Leans` 0.25 reading.
4. Capture a grid for `ld_jump_fwd` before calling the forward landing unchanged.
