# Skid stock base review (A = stock, M_Relaxed_Run_Turn_R_180_Rfoot)

Date 2026-11-20. Reviewer: read-only, blind to code/handovers. Inputs: `docs/notes/briefs/skid-identity.md`, pack `stock_mv_skid_*`
(4 grids, mp4, `stock_assert.txt`, `assert_skid.json`). Frame numbers are VIDEO frames (`vNNN`, 60 fps, as printed on the grids).
Two extra crops of the mp4 were made in the scratchpad (side view v116-v164 every 3 frames) to read the feet.

**Verdict: FAIL on identity (traits 1 and 2). Stock is a good mocap turn, but it is not the owner's skid. A v3 restyle is justified.**

## 1. Identity traits

| # | Trait | Result | Frames | Label |
|---|---|---|---|---|
| 1 | Back foot planted for the whole skid | **FAIL** | v119-v137 | posa |
| 2 | Only the front (braking) foot slides, driven by a designer marker | **FAIL** | v119-v146 | posa + dati montage |
| 3 | Smooth blend in and out | **PASS (partial evidence)** | in v116-v122, out v152-v164 | (gioco for the missing part) |

Trait 1, detail. The motion is a one-leg plant, the opposite of the brief:
- v119-v125: the LEAD foot plants ahead of the body; the rear leg is already off the ground, trailing behind.
- v128-v137: the rear leg swings forward and tucks (knee up at hip height) while the body sits low on the single planted foot.
  The rear foot is in the air for about 18 frames. No rear-leg braking, no weight on the rear leg.
- v140-v149: pivot on the planted foot, the free leg reaches out toward the new direction; at v146-v152 the body reads
  almost unloaded (shadow detached from the feet in the side view), i.e. a small hop out of the pivot.

Trait 2, detail.
- Pose: there is no "one foot slides, the other holds" picture. There is one support foot only, so whatever slide the
  capsule deceleration produces lands on that foot (v122-v140, large dust cloud), and the pivot at v140-v149 spins on it.
- Data: nothing in the pack shows an authored slide window (marker/curve) on the clip. The slide length is whatever root
  motion and capsule braking give, not tunable. Label: dati montage.
- Not measurable from this pack: the amount of slide in cm. The cameras follow the character and there is no foot metric.

Trait 3, detail.
- Blend in, run -> plant (v116 -> v119 -> v122): continuous, no pose snap at 3-frame sampling. PASS.
- Blend out, pivot -> run (v152 -> v158 -> v164): continuous, lean builds progressively. PASS.
- Skid -> idle is NOT in the evidence: the video ends at v219 while the character is still decelerating after the release
  (v200-v218). The brief's "skid -> idle/run" is only half verified. Label for the gap: gioco (capture length), not a pose fault.

## 2. Life

| Item | Result | Frames | Note |
|---|---|---|---|
| Anticipation | weak | v116-v119 | Run goes straight into the plant in 3 frames. Mocap-correct, no readable "brake!" beat. |
| Arc | good | v122-v152 | Pelvis drops deep (v128-v137) and rises through the pivot, one clean arc. |
| Weight | good, wrong character | v128-v137 | Deep sit sells mass, but it is an agile athlete's sit-turn, not a heavy body resisting its momentum. |
| Follow-through | weak | v149-v164 | Arms and chest arrive with the hips; no overshoot or settle after the turn. |
| Root vs capsule | not verified | v122-v146 | Support foot under a decelerating capsule: likely skate, hidden by dust. No metric in pack. |
| Style bar (Ganondorf in Astro Bot) | FAIL | whole window | Realistic, light, quick. Not cartoony, not heavy, no held silhouette. Label: posa. |

Readability note: the dust cloud covers the feet in v122-v146 in three of four views, exactly where the identity is judged.

## 3. Technical

- `ASSERT: OK mv_skid *Run_Turn_*180* f140-f175: weight >= 0.8 on 100% (min 1.00)`. PASS as written.
- Open point (gioco/capture, to check on frames.jsonl, which I did not read): the assert uses timeline frames `f`, the grids use
  video frames `v`, and the pack gives no mapping. The visible plant starts at v119 and the pivot ends near v152. If the offset is
  about +20 (reverse input f95 ~ v117, release f170 ~ v192, both consistent with the pictures), the asserted window f140-f175 is
  about v160-v195, which is the run-out AFTER the pivot. In that case the assert does not cover the braking part at all, and
  the clip playing in v119-v152 is not proven by this pack. The assert window should start at the reverse input.
- No life metrics in the pack (idle-feet creep, root x end, foot speed). Step 7 of the brief has nothing to compare against yet.

## 4. What v3 must change to beat stock

1. posa: rear foot down and pinned from first contact to the end of the skid (replaces the airborne tuck v128-v137).
2. posa: front foot is the braking foot, extended ahead, heel/sole dragging; body leaning back against the motion.
3. dati montage: authored slide window (marker/curve) on the clip that drives the front-foot slide, tunable length;
   rear foot locked in world space during that window.
4. posa: no hop out of the pivot (v146-v152): turn with weight transferring rear leg -> new front leg, feet on the ground.
5. posa: add a 2-4 frame anticipation at contact and an overshoot/settle of chest, head, arms after the turn.
6. posa: heavier, wider, held silhouette (style bar); keep stock's pelvis arc, it is the best part.
7. gioco/capture: longer capture (through idle), dust off or reduced for the review pack, a foot-speed metric for both feet,
   and an assert window that covers the brake (from the reverse input), with the v<->f mapping printed on the grid.
