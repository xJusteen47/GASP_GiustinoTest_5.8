# Review Fable: frantic run start, in game 1 (2026-11-19)

**Verdict: FAIL (gioco).** The frantic clip r3 is NOT what is on screen. Poses are not judged: nothing of r3 is visible.
Images used: 7 of 8 (r3 grid_side, ig1 side grid, ig1 game grid, az90 f048 / f056 / f064 / f072).

## Is the clip on screen? No
- r3 from f1: fists at chest, elbows ~90 deg, one knee at hip height, pelvis 5-10 cm low, torso 18-28 deg forward, feet stamping.
- In game v042-v058 (stick pushed at ~v040): upright idle, arms hanging, open hands, BOTH feet flat and planted, knees slightly
  bent only. No foot leaves the ground for ~20 frames = the whole stamp (21 sim frames) shows idle.
- v060-v066: slow lean and small forearm lift, feet still planted. v070-v082: low crouched push-off with long strides,
  torso ~40 deg, open hands = a plain GASP run start, then the normal run from ~v086.
- Dust puffs at v070-v082: the sim/VFX side of the start runs, only the animation is missing.
- This agrees with the pose match (clip RMS 17.2 cm, worse than idle 11.9).

## Identity traits (brief)
1. FRANTIC LIMBS: **FAIL.** 0 steps in v040-v066 (target ~5 steps/s, 3 stamps in place); no arm pump, no fists, no high knee.
2. HEAVY (Ganon): **FAIL / not judgeable.** No stomps, no pelvis drops; the visible start (v070-v082) is an athletic sprinter push-off.
3. SHORT AND CLEAN: **FAIL.** Frame 1 is idle, not a run pose; the player sees ~0.35 s of nothing, then a generic start.
   Handover to the run at v086-v094 has no pop (that is GASP's own blend, not ours).
Game camera (v042-v082): the character is ~25 px tall and the silhouette does not change until v070; no "scramble" read at all.

## FAIL items
- **gioco, v040-v066: RunStart state not shown.** Likely the known routing issue: a state in ABP_GSTN_Locomotion shows only through the
  Blend Stack + chooser row. Check, in this order: (a) row for AS_RunStart_Frantic_r3 in CHT_GSTN_LocomotionAnimations and the state
  name passed to SetBlendStackFromStateName; (b) GetAnimOwnerRunStartTime >= 0 on the anim thread from the push frame (log it v040-v062);
  (c) the idle -> RunStart alias fires on the stick, not on speed (speed is held ~0 by the stamp, so a speed rule keeps idle);
  (d) blend in <= 0.07 s (2 clip frames), else the stamps are eaten.
- **gioco, v066-v086: GASP start plays after the stamp.** While run start time >= 0 the motion-matching start must be excluded;
  exit only on clip end (f27) with a 0.1-0.15 s blend to the run loop.
- **dati montage: none.** **posa: none judged** (r3 passed offline; f27 phase vs run loop is still unverified, check it in the next capture).

## Next capture (gate before any pose review)
Same views, plus on-screen debug text: current Blend Stack asset name + run start time. PASS gate for "clip on screen":
at v046 (clip f4) one knee at hip height and both fists at chest in az90; pose-match RMS of the clip below idle's.
