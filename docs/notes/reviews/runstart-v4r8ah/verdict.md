# Verdict run start v4r8ah: FAIL (hand-off fixed, new entry defects)

Identity traits
1. ONE high-knee pump from the first frame, then run strides: OK (L knee up v84-v90, foot z 64; then R drive knee, then strides; no stomp, no standstill).
2. Feet planted: OK inside the pump and drive (R support v84-v90 drifts 1.5 cm; L plant v99-v105 3.8 cm). KO only in the entry frame, see F1.
3. Arms at rest, forward lean: OK (hands at hip height, elbows out, nothing above the shoulder or through the body).

Life (mp4, before numbers): the rev-up is now readable from the first frame (before the fix the pump was mostly blended away: top row v088-v092
is almost a standing pose). Lean and drop into the drive v97-v105 have weight, dust at v95 lands on the foot switch, long first stride v106-v114
follows through. No anticipation before the knee (accepted by the "almost instant" entry), but the entry itself pops.

Hand-off into the loop (the reason of this fix): FIXED. Before: L never touched down (hovers z 21-27 at x 209-214, v127-v132), R landed twice
in a row (x 146 then x 293). After: R plant v116-v123 x 145-152, L plant v133-v139 x 275-281, R plant ~v148 x 390-400: alternate, 17 frames each,
no skipped, double or floating step, no hop. Plant slide 8-11 cm at touchdown, inside the inherited loop slide.

FAIL items
- F1 gioco, v82-v84: pop from idle. v83 is a one-frame foreign pose (L leg kicked forward and wide, foot +27 cm up and +13 cm forward in one frame,
  stance still wide), then v84 the support R foot snaps 11.5 cm sideways in one frame. Clear in the front view; it is 2 frames, not a 3-frame blend.
- F2 gioco / dati montage, v89: one-frame twitch inside the pump. L knee and foot flick 10 cm outwards (foot y 141.9 -> 131.6 -> 142.5) and both
  elbows flare for one frame. New in this version.
- F3 gioco, v86-v94: clip weight is not held during the pump: 0.82 at v85, falls to 0.50 at v90, back to 0.87 at v95 (a V, with Walk_Leans
  rising in step). The assert only covers v97-v115, so it does not see this. Likely the same cause as F2: something re-blends just after the entry.

Technical notes (not FAIL): v95 = v96 identical frame (capture hitch, also in the metrics). Actor x at v200 788.1 vs 795.6: no concern.
Summary planted travel went up (L 42.6 -> 51.6, R 45.2 -> 58.2 cm) because there is one more real plant; per plant it is in the loop range.

What the owner will notice: the run-up into the loop is finally clean, left-right-left with no floating step, but the very start flickers:
a snap on the first two frames and a small knee/elbow twitch half way up the pump.
