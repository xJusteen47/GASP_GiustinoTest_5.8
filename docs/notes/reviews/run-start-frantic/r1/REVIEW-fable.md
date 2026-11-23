# Review r1, frantic run start (Fable, 2026-11-19): FAIL, go to r2
Judged from grid_side.png, grid_q34.png (stills every 2nd frame, looked once) and metrics.json. No motion seen: cadence and pops are read from the numbers.

## Identity traits (checked first)
1. FRANTIC LIMBS: **FAIL**
   - posa: back swing is a chicken wing (f7, f13, f15, f25, f27: upper arm horizontal at shoulder height, forearm hanging).
   - posa: elbow collapses to 32-55 deg mid-swing (f5, f6, f8, f9, f12, f16-18, f22-24) and the arm crosses front to back in 2 frames (f4 to f6), then holds: a pop, not a pump.
   - posa: run part is a long bounding stride, not a scramble: feet 0.57-0.65 m apart in flight (f17, f23), contact 0.28-0.30 m ahead of the pelvis.
   - posa: open hands. In-place knees (f1-f9) are good: keep them.
2. HEAVY: **FAIL**
   - posa: torso reads 10-15 deg, chest proud and head up at f11 and f21; no hunch in silhouette.
   - posa: pelvis bobs 4 cm every 3 frames (10 Hz) in f1-f10: that is vibration, not mass.
   - dati: lean is not in metrics.json, so keyed vs real cannot be compared.
3. SHORT AND CLEAN: start **PASS** (run pose from f1, 0.9 s, timing B respected); hand-over **FAIL**
   - dati: phase, pelvis height and lean of M_Neutral_Run_Loop_F are unknown, f27 is a guess.
   - gioco: blend-out and foot slide with warping can only be judged in the capture.

## r2 pose spec (30 fps, frames from 1; angles in the side plane unless said)
1. Arms in FK, not hand targets: key upper-arm swing S (deg from hanging down, + forward) and elbow flexion E. Sine ease between extremes, no holds.
   Abduction <= 12 deg always; elbow joint stays >= 12 cm below the shoulder joint on every frame (add this to metrics).
2. Arm extremes, f1-f10: forward S +45, E 100, fist at chin height 10 cm ahead of the sternum; back S -40, E 85, fist beside the hip. Total swing 85 deg.
   f11-f27: forward S +55, E 100; back S -60, E 80. Total 115 deg. E never leaves 75-105 anywhere in the clip.
3. Arm timing: extremes 1 frame after each contact (f4, f7, f10, f14, f20, f26); wrist lags 1 more frame with 8 deg extra bend at the extreme.
4. Fists on all 27 frames: fingers curl 75 / 90 / 60 deg (base / middle / tip), thumb 30 / 40 across the index.
5. Lean L = angle of the pelvis-to-neck_01 line from vertical; write it per frame in metrics, measured within 3 deg of keyed.
   L: f1 20, f3 24, f6 29, f9 34 (ramp, the body tips while the feet spin), f11 32, f13 30, f19 24, f25 17, f26 15, f27 16 (one small undershoot).
   Split: pelvis pitch 30% of L, spine_01-05 share 70% evenly. Neck counter 0, head counter 15% of L only (chin leads, eyes ahead).
6. Hunch on all frames: clavicles 12 deg up and 10 deg forward, spine_04-05 extra 8 deg flexion, head 4 cm ahead of the chest line.
7. Pelvis height f1-f10, slow load instead of a bob: f1 -5 cm vs idle (0.856), f3 -8, f6 -10, f9 -12 (0.786), ease-in overall;
   per-stamp ripple only -1.5 cm on the contact frame, back in 1 frame. f10 -8, f11 -3 (0.876, release, ease-out).
8. In-place legs: keep contacts R f3, L f6, R f9, flat feet, lifted ankle 0.30-0.34 m; feet stay within 8 cm of the start spot. Hips yaw +-6 as r1.
9. Run legs f11-f27, short choppy steps: contacts L f13, R f19, L f25 with the ankle 0.18 m ahead of the pelvis (r1 0.28-0.30); toe-off 0.15 m behind;
   feet never more than 0.40 m apart front to back. Stance foot slides back 0.33 m in 3 frames, linear (constant ground speed).
   Recovery: heel kick ankle 0.38 m high, passing under the pelvis; driving knee: thigh 70 deg from vertical at f11, f16-17, f22-23.
10. Pelvis height on run contacts: f13 -7.5 cm (0.831), f19 -7, f25 -6; mid-flight -3 (0.876). Ease-in into contact, ease-out after.
11. Follow-through: chest (spine_04-05) nods +3 deg 1 frame after each run contact, head +4 deg 2 frames after; both back in 3 frames.
12. Hand-over (needs the editor first): read from the run loop the frame with left foot flat and right knee passing, plus its pelvis height and L.
    Key f27 to those values (L = loop + 2 deg) and ease f24-f27 toward them. Until read, keep r1's f27 and mark the hand-over unverified.
13. Rules kept: root untouched and in place; ankle z >= 0.094 and ball z >= 0 on every frame. With the pelvis at -12 cm check the knee does not lock or flip.
14. Capture round (gioco): check foot slide with stride warping at low speed (the 0.33 m slide is sized for it) and the blend-out pop at f27.
