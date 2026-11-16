# Balloon knockback: Smash Ultimate vs our KnockbackModel (2026-11-16)

Sources (public): ssbwiki "Launch" and "Hitstun" pages (frame data research, not official).

## Smash Ultimate
- Launch speed = 0.03 x KB per frame, decays by 0.051 per frame along the launch; gravity still applies, air friction off.
- Hitstun = 0.4 x KB (minus 1). Hitstun cancel: air dodge from frame 40, aerial from 45.
- Balloon (tumble only, not set knockback): from hitstun ~30 to 80 the first frame runs at x1..x6 "frame speed"; each frame the multiplier
  falls linearly with the hitstun left and is 1 once 30% of it is left. The path is the same, flown faster: KO-percent hits fly away fast
  and stop quickly, the victim is locked for fewer frames (90 KB: 36 -> 33 frames; 145 KB: 58 -> 41 frames).

## Ours (GSTN_KnockbackModel + FGSTN_LayeredMove_Knockback)
- Launch velocity K cm/s set on the first frame; after it only gravity (no decay of the launch speed: DecayPerFrame is 0 for launches).
- Hitstun = K x HitstunFactor (0.02) x hit and hurtbox multipliers, min 6: tumble threshold 1800 cm/s = 36 frames, 4000 cm/s = 80.
- So strong launches already fly far per hitstun frame; without a cap a launch upward kept the victim helpless until landing (fixed by B).

## What was added
- `FGSTN_ReactionTuning::Balloon` with Ultimate's constants, OFF by default (per ruleset, since the model lives in the rules).
- Hitstun with it on (default constants): 40 -> 36, 60 -> 43, 80 -> 50, 120 -> 75 game frames (`FGSTN_BalloonMath::GameFrames`).
- Difference from Ultimate: no cap at 80 (past it M0 stays 6, the saving grows with the hitstun).
- Not added: Smash-style launch speed decay (0.051/frame). It would change every launch's distance; a possible next data knob.
