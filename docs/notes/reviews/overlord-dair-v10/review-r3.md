# Dair V10 r3, Fable review (cloud, 2026-11-17)
Verdict: POSE PASS on all three clips. Game-side items remain (not pose).
- HeavyTuckR3 PASS: low from the ball, feet slide out, no low-tall-low; rise v98-v103 progressive, standing v105 (g1-g3, frame v93). Polish (non-blocking): pelvis 17-19 cm reads near sitting from the back; hold ~25 cm (ball height) f3-f9.
- HeavyR2 from strike PASS (g4 v87-v101, g7 apex v90-v104).
- LightR3 PASS: f0 crouched contact (v86), clean rise g5 v87-v94.
- Whiff full move matches V9 (g8). Charge PASS since r1.
- Window f4-14 accepted (= touchdown f2-12 with 2 f latency); return to f4-12 once latency is fixed.
GAME (code queue, block shipping, not the pose):
1. v88 one-frame sink before the tucked clip (capsule-restore order).
2. After LightR3 the base-layer landing crouch returns from v95 for 12+ f: ABP must skip its land state after a landing-lag montage.
3. 2 f touchdown latency (tolerable). g7: a figure leans forward v114-v130, player or dummy unclear.
Unverified: late light window f32+, stomp twist/head turn at full resolution.
