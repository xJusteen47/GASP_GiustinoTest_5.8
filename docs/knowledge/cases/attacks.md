# Worked cases (level 3): attacks

Examples applying level 1 (`skills-public/knowledge/animation/`, IDs `P-*`, `X-*`) and the profile
(`../profile-gstn.md`) to attacks. **Not rules.** Course numbers come from private study notes
(`../study/stylized-attacks-part1.md`, `part2.md`). The course works at **24 fps** (checked on two frames of the Maya
playback options); in brackets the 60 fps equivalent (× 2.5). They describe the instructor's realistic-stylised
fighter, so for our toy timing treat them as an upper reference and keep the **ratios**.

## Case A-1 · Light, medium, heavy: what changes between tiers
Course demonstration (same character and punch, 24 fps [60 fps]): light ≈ 8 [20] → medium 18 [45] → heavy 26-28
[65-70]; medium anticipation ≈ 4 [10] vs heavy 10-11 [25-28] held; heavy hit hold 7-8 [18-20], close to the wind-up hold;
light move-in to the hit 2 [5] then hold.
- **Ratios to keep:** medium ≈ 2-2.3× light; heavy ≈ 1.5× medium; heavy anticipation ≈ 2.5× medium; heavy hit hold
  ≈ 0.6-0.7× its anticipation hold. The extra length goes into **anticipation, holds and settle**, not into a slower
  strike: the strike itself stays a short burst (`P-GAME-14`, `P-SPACE-08`).
- **Light:** pose changes on the next frame after input, moves in over ~2 course frames, then **holds** the hit pose
  so it does not flicker (`P-ANTIC-04`, `P-TIME-04`).
- **Medium:** a held anticipation read, a stepped weight shift, a rebound in the settle (`P-ANTIC-05`, `P-SETTLE-04`).
- **Heavy:** wide wind-up with opposite lines held long, whip C → reverse C → S, land on the front foot, hold, then a
  slow recovery with feet re-planting (`P-POSE-07`, `P-WEIGHT-06`, `P-CONTACT-03`).
- **Profile on top:** Overlord is medium-heavy and toy-timed; the quality bar is Smash-level. Our tiers are light
  (X), strong (RT) and specials: compare tiers of one kit with `P-GAME-14` before judging one move alone.

## Case A-2 · Frame data on the clip
- Course: startup ≈ 4 [10], active ≈ 3 [7-8], recovery the rest (≈ 20 [50]) on a demo punch; overshoot sits 1-2 frames before the
  hit frame and pulls back.
- **Level 1:** `P-GAME-13` (named startup/active/recovery ranges on the clip), `X-01` (pose extreme, effect peak and
  hit-pause start on one frame), `P-GAME-07` (the frozen pose is the impact pose).
- **Profile:** designer-facing frame data counts from 1; sim at 62.5 Hz.

## Case A-3 · Recovery that is not a straight line
- Course: a breakdown at about the middle of the recovery, then retiming the recovery keys by one or two frames each,
  and a small rebound of the hitting arm before the hand-off.
- **Level 1:** `P-SETTLE-04`, `P-LIFE-02` (one eased segment from hit to idle is the lifeless signature), `P-GAME-06`.

## Case A-4 · Effects for readability (where animation hands over to VFX)
- Course: impact burst held ~2 [5] frames at peak scale then scaled/moved with the force; camera shake from the contact
  frame, three decaying swings; a retime that slows the anticipation and holds the hit.
- **Level 1:** `X-01`, `X-09`, `X-10`, `V-IMPACT-01`. The instructor tried delaying the burst and went back to the
  impact frame (transcript checked), so no conflict with `X-01`.
- **Profile:** hit-pause freezes Mover movement in sim; the Sakurai visual branch is not merged (project memory).
