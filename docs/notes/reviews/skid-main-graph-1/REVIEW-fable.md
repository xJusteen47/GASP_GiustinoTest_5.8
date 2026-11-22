# Skid in main graph (state "Skid", SS_180_Turn_Root180) - review fable: FAIL
Sources: the 6 grids in this folder (az90, variant "main" vs "base"), each looked at once. Visual only: no frames.jsonl numbers this round, so sizes are estimates from the sheets.

## Identity traits
1. Low body, weight back, braking leg extended: FAIL (gioco, maybe dati montage). Variant v62-70 is a narrow crouch, torso upright, feet close together. The wide low lunge of base v59-64 never appears. Pelvis drops less than base.
2. Turned toward the new direction while sliding, dust at the foot: PASS, partial. v68-79 the body turns slowly to back-to-camera (about 90 deg of the 180) while the dust is at the braking foot (v66-80). It never gets past 90 deg inside the Skid state.
3. Held, then explosive push and full run in ~15 frames: hold PASS (v72-79 pose almost still, reads as held). Push and return FAIL (gioco): v80-94 is a wide straddle seen from behind, never a run pose facing left. Base is in a readable left run from v76-78.

## Other checks
- Facing: one turn only, no double 180, no backward run. But the turn is NOT finished: at v94 the character still shows its back to the camera (about 90 deg off the travel direction), while base faces left from v74-76. FAIL (gioco): after the 0.12 s exit the locomotion plays what looks like a sideways/strafe stride, so the actor facing or the pose chosen does not match the new direction.
- Pelvis under the floor: PASS, never close.
- Entry pop (base v58->59): FIXED. Variant v59->62 lowers progressively, no one-frame jump of head or feet.
- Entry timing: FAIL minor (gioco). Variant keeps the run stride v54-59 (v56-59 nearly the same pose, 4 frames) and starts lowering only at v60; base starts at v55. About 5 frames (0.08 s) later, the brake reads as late.
- Exit v79->80: visible one-frame step (crouch with bent legs -> upright wide stance, arms out). Smaller than the old base pop, but seen. FAIL minor (gioco, 0.12 s blend to a very different pose).
- Restart slide (base v76-88): KEPT, looks worse. v82-91 the straddle pose is frozen for ~10 frames while the dust balls drift right in the frame (the body glides left). No foot plants. FAIL (gioco).

## Variant vs base
Better: no pop at entry, hold is clean and stable, dust timing matches the hold.
Worse: brake pose is weaker (no lunge), skid starts ~5 frames later, the 180 is not completed, the return to run is not a run within v80-94.

Smash Bros / AAA parity: no. The state entry is cleaner than base, but the move loses its brake silhouette and ends in a gliding sideways stance instead of an explosive run the other way.
To check next: actor yaw vs mesh yaw at state exit (does the Root180 rotation reach the actor?), and which clip the chooser picks at v80-94.
