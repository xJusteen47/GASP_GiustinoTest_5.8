# Dropkick: why the owner saw no crossed arms and no oomph in the r4 videos (2026-11-21, laptop, read only)

Source: Saved/DropkickTB4/r4e (the capture behind r4b/dropkick_low_*.mp4), frames.jsonl + az90/az180 shots. Images in this folder.

## 1. Capture chain (checked first, rule 10)
- **The "hit" video never hits.** dropkick_low_hit frames.jsonl: the dummy stays at (255,-460,90) for all 260 frames, its combat
  `history` is empty, no montage on it; the attacker lands at x 207, 48 cm short. So no hitstop, no reaction: the owner never saw the
  kick connect. Command: python over frames.jsonl, `d['dummy']['loc']`, `d['dummy']['combat']['history']` (same in base, base2, r4..r4e).
- **The "low" dropkick is pressed 7.8 m up.** capture.log: "crouch on v120 (height 785 cm, falling)". The playtest kit jumps ~919 cm
  (max `h` in frames.jsonl), the press waits for trigger_h 90 cm but gives up after AIR_GIVE_UP = 100 frames
  (pie_capture_move.py:159). The video is a 35-frame passive fall in the loop pose (v122-v156), then the landing.
- ASSERT only checks the tech bounce clip weight, so both defects passed. Fix: PIE_CAPTURE_AIR_GIVE_UP + new sequences
  dropkick_sweet_hit / dropkick_sweet_whiff (press at 380 cm falling, so startup f15 meets the dummy's chest).
- The poses in the video match the cards (one pose per frame, montage AM_OverlordDropkick 0..15 then loop 22..26): the arms defect is
  in the clip, not in the capture.

## 2. Arms (identity trait): really not crossed in the KICK
- v125 (startup f3): forearms held level in front of the chest, hands stacked away from the body (steering-wheel read).
  v137 (f15, strike) and v146 (loop): fists up by the chin, forearms pointing out of the chest = a raised guard.
- Cause: a0c7bf9f (11-15) moved r0's hands 12-18 cm forward to fix chest penetration but left the elbows at the sides; chest-frame
  numbers of the loop pose (k5/chest.py): right elbow on the midline (-3.5 cm), right hand at the LEFT shoulder height (up -2 cm),
  26 cm out of the chest.
- Why the review said PASS: r4/r4b reviewed only the tech bounce clip (v158-v168), where the arms ARE crossed; the kick clip was never
  reviewed after the 11-15 restore (jab-dropkick-r1 ended "awaiting rule-10 review", no review.md in reviews/jab-dropkick-r1/).

## 3. Kick oomph
- The body lays back (pitch 30 -> 55 -> 80) during the same frames the legs extend (f8-f15): one slow lean, no load, no shot.
  The tuck at f4-f8 is a seated pose (feet 24 cm under the pelvis), not a ball. Head and hands lag 2-3 frames (part_delays), so the
  strike frame is soft. In the video the contact (if any) would come in the loop, the weakest pose.
- Gameplay: one hitbox, same damage 10 / knockback 400 / hitstop 6+4 from f15 to the landing. A real sweetspot (strong early window,
  weak late) is a gameplay change: owner decision, not done (rule 12).
