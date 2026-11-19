# Review Fable: frantic run start, in game 2 (2026-11-19)

**Verdict: FAIL** (2 gioco, 1 posa). Big step: the frantic clip r3 IS on screen now. Not for the owner yet.
Images used: side.png, game.png, r3 grid_side, plus crops of rs_start_v10.mp4 (az90 and az45, v42-v113, every frame).
Frame mapping used: first pose change at v43, so clip frame f = 1 + (v - 43) / 2. Clip = v43-v97, stamp = v43-v63, steps = v63-v97.

Open point on timing: the task says the stick is pushed at v20, but nothing moves until v43 (v20-v42 = plain idle).
I assumed the video numbering is ~22 frames ahead of the sequence (ingame1 had the push at ~v40, and the dust at v70 fits a
21-frame stamp from ~v43-v49). If the push really is at v20, add a FAIL gioco: 22 frames (0.37 s) of latency. Check with the log.

## Is our clip on screen? Yes
- v44 on: low crouch, both fists closed at chest, elbows ~90 deg, shoulders hunched, torso ~25 deg forward = r3, not idle, not GASP.
- v63-v96: three short crouched run steps with fists, torso still ~25 deg. No GASP sprinter push-off (ingame1 v70-v82 is gone).
- v98 on: the normal run (upright, open hands, long stride).

## Identity traits
1. FRANTIC LIMBS: **partial, FAIL on the stamp.**
   - Stamp v44-v62: foot lifts are small. Best one v48-v50: foot ~15-20 cm off the floor, thigh ~40 deg. r3 f5 has the thigh
     near horizontal and the foot 32 cm up. Lifts at v54-v56 and v58-v60 are heel taps (<10 cm). Reads as a crouched shuffle.
   - Game camera v44-v62: silhouette = crouch with elbows out, the legs do not read. No "scramble" in the first 0.33 s.
   - Steps v63-v96: good. Knee near hip height at v80-v84 and v92-v95, back leg kicks at v64-v68 and v86-v90,
     about 9-10 video frames per step = ~6 steps/s. Arms: fists, elbows ~90, near elbow clearly behind the back at v80-v86.
2. HEAVY (Ganon): **PASS, thin.** Pelvis ~15 cm low for the whole clip, hunched, head leading low, feet land flat (v52, v62, v78).
   Per-contact pelvis drop is hardly visible in the stamp (head stays level v44-v62); it comes back once the feet lift properly.
3. SHORT AND CLEAN: **FAIL at the hand-off.**
   - Entry v42-v44: idle to full crouch in 2 frames, pelvis ~15 cm down. Reads as a snap, acceptable for a burst (see minor).
   - Length 0.9 s, 3 steps: OK.
   - Hand-off v96-v101: torso goes from ~25 deg to ~5 deg and the pelvis rises ~12-15 cm in 5 frames. Foot phase matches
     (near leg back at v96 and at v101), no limb pop, but the body jerks upright. Brief: f27 = run lean + 1-2 deg.

## Foot slide: not measurable, probably present
The floor has no texture and both cameras follow the character. In az90 the planted far foot is fixed in camera space for
v78-v84 (7 frames) and v90-v96, while the character is moving (dust puffs and the red disc drift back). A planted foot should
drift back with the floor. Rough estimate from the disc parallax: 15-25 cm of slide per contact, low confidence.
Stamp v44-v62: no slide, feet stay put.

## FAIL items
- **gioco, v44-v62 (clip f1-f10): leg motion is cut to about half.** Upper body is at full weight from v44, so it is not the blend:
  something holds the feet down at zero speed (foot placement / foot lock / leg IK, or stride warping scaling the legs at speed ~0).
  Fix: foot placement and stride warping alpha 0 while run start time is 0-0.33 s. Gate: at v51 (f5) foot >= 28 cm off the floor
  and thigh >= 70 deg in az90. Log foot_l / foot_r Z against the clip to confirm.
- **gioco, v63-v97: foot slide, unmeasured.** The clip is in place, so stride warping has no reference speed. Fix options:
  give the clip a speed curve (step 0.38 m x 6 steps/s = ~2.3 m/s) so warping can scale, or cap sim speed at that value until
  the clip ends. Gate: slide <= 5 cm per contact. Next capture needs a checker floor or a foot world-X log.
- **posa, clip f21-f27 (v83-v97): lean and height do not land on the run.** Fix: ease the lean from 25 deg at f21 to run lean + 2
  (~7 deg) at f27, pelvis up to run height - 2 cm at f27, hands start opening from f25. Then blend out 0.15-0.2 s (now ~0.1 s).
- **dati montage: none.**

## Minor (not blocking)
- Entry blend v42-v44: widen to 3-4 video frames (0.05-0.07 s), no more, or the first stamp is eaten.
- Dust starts at v70, 27 frames after the first move. A puff on each stamp (v52, v62) would sell the scramble (Toad ref has dust
  during the frantic phase).
- Normal run after the hand-off (v102-v113) holds almost one pose for 12 frames: slow and floaty. Not this move, note for the run.

## Keep
- State entry timing: clip visible one frame after the first move.
- Fists, elbow angle, hunched shoulders, head low, 25 deg lean through the stamp and steps.
- The three frantic steps v63-v96: cadence, knee height, back kicks, arm swing.
- Flat foot landings, no foot through the floor, no slide during the stamp.
- Foot phase at the hand-off (near leg back at clip end = run phase).
