# Verdict run start v4r8 (cloud review, 2026-11-20) : FAIL (feet)
Material: runstart_v4r8.mp4, grid_az90.png, metrics_summary.json, review_feet_az90_v92-115.png (az90 crop of the mp4, every frame v92-v115).

## Life (video, no numbers)
- Identity 1 (frantic start): OK. v92-v100 is a fast high-knee scramble with a forward lean, then real strides from v104. Reads as a burst, not a jitter. Shorter rev-up matches the Luigi ask.
- Identity 2 (arms at rest): OK, same chest-height arms as v4r7, no high-to-low drop.
- Identity 3 (lean, no stomp): OK.
- Cut (v99-v103): no knee or foot pop visible; legs go scramble -> scissor -> stride in one arc. Hand-off to the loop (v116-v130) is smooth; the body is a bit more upright earlier than v4r7, expected since the clip ends sooner.

## Feet (FAIL)
- The stance foot is DOWN v95-v101 (clip f0-f3) but moves forward against the world grid cross by about a third of a grid cell while planted (review_feet_az90_v92-115.png, compare foot vs the cross in each frame). That is why the contact detector found no contact in the rev-up: the foot is on the ground but moving. Corroborates planted-feet travel f80-240 46.6/45.0 cm vs v4r7 36.7/33.3 (metrics_summary.json).
- It reads as a skate, not as cartoon in-place steps: the foot is flat on the floor, not lifting. No designer marker authorises it.
- Label: posa (clip): the 7-frame warp with the root speed ramp moves the root faster than the planted foot. Not game: ASSERT OK, clip weight 1.0 in window.

## Next
Brief step 6 on desktop: no cut (CUT 0,0), TARGET_S 0.27 (Luigi turning start), same capture, re-review. v4r7 stays the approved base.
COSTO: ~2 $ | 0 min desktop | ~45k token review
