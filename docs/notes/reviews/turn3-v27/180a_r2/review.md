# Review: turnaround 180 L, variant 27 (pivot then run), round 2

Reviewer: independent cloud review (rules 10, 13, 15a). Source: `claude/runstart-generic-desktop` @ 22300411,
`docs/notes/reviews/turn3-v27/180a_A/`. Round 1: `reviews/turn3-v27/180a/review.md` (FAIL).
Looked at: every frame v84-v107 of variant 27 in `ab_26_vs_27_{az180,az90}_real.mp4` (v = 79 + file number of the
extracted frame), leg close-ups v91-v102 in both views, the builder's `grid_ab_game.jpg`, `facing_v82-111.csv`,
`assert_v27.txt`, `assert_pivot_weights.json`. 5 images. Sheets here: `r2_v84-107_az180.jpg`, `r2_v84-107_az90.jpg`,
`r2_legs_v91-102.jpg`. Frames were judged as sheets, the video was not played at speed.

## Verdict: PASS (can go to the owner). 0 FAIL items, 4 notes.

## LIFE (frames first, no numbers)
- v84-v93: one continuous turn. Head and chest go first, the L foot steps out and plants, the R foot stays planted and
  rolls onto the ball. Torso upright, both forearms tucked at the belly the whole time. No stall side-on: the round 1
  hunched one-leg hold is gone.
- v93-v98: body almost facing the run, still upright. R foot leaves the ground, passes low beside the L ankle and
  touches down on the toe. This is the link between pivot and run, it reads as a step, not as a wait.
- v96-v101: the sprinter drop (side view: lean grows frame by frame from v96), first travel at v96.
- v99-v107: rev. Standing on R with L folded behind v99-v104, L lands v105, arms pump from v100.
- Reads as "pivot, step, go" in one beat. Not a robot spin (feet step, wide stance), not a crouch-and-wait.

## Identity traits (rule 13a)
1. Clean pivot, 180 in 6-9 frames: PASS at the upper limit. Hips -2 deg at v84, -149 at v93 (`facing_v82-111.csv`):
   9 frames to within 23 deg of the final -172.
2. Head and shoulders lead: PASS. Chest ahead of hips by 10-20 deg v85-v91 (csv), head in profile first on az180.
3. One planted pivot foot, other foot steps out already turned, never crossing behind: PASS (v85-v92).
4. Pelvis only -3/-4 cm in phase 1: NOT MEASURED. No pelvis height in the pack. By eye the dip is small and comes
   from the wide stance. See note 3.
5. Arms tucked, no jitter: PASS v85-v99 on the sheets (same forearm pose frame after frame).
6. Phase 2 starts when the pelvis is within 20-30 deg: PASS. Drop starts v96 (hips 12-20 deg from final), first rev
   contact v97-v99.
7. Feet on two lines, no crossed ankles: PASS, with note 1.

## Round 1 items
1. gioco, yaw stall: FIXED. Hips -83 at v89, -135 at v92, -160 at v96 (round 1: about 90-110 until v96).
2. posa, early drop: FIXED. Upright through v95, lean from v96.
3. posa, R leg hanging: FIXED. Round 1: in the air v91-v101 folded behind. Now toe-off v93, in the air v94-v96 low
   (foot at mid-shin height, never folded), toe contact v97-v98, flat and loaded at v99. I read the landing earlier
   than the builder's v101: v99-v101 the R foot is flat and it is the L that is in the air.
4. posa, arms open: FIXED. Tuck holds to v99. See note 2 for the hand.
5. cattura, ASSERT window: FIXED for the gate (`ASSERT: OK ... f88-f132 ... min 0.86`). See note 4.

## Notes (not FAIL, cheap to do with the other three directions)
1. posa: the first R contact v97-v98 lands close to the L foot (about one foot width apart on az180). No crossing,
   but it is the narrowest moment. If the owner sees "feet together", land it 5-8 cm further out.
2. posa: R hand is open with spread fingers at v102-v103 on az180 while the arm swings back. Close the fist.
3. cattura: add pelvis height (cm vs idle) per frame to the facing csv, so trait 4 can be checked with a number.
4. cattura: `assert_pivot_weights.json` says `ok: false` (window 82-104, coverage 0.739) while `assert_v27.txt`
   says OK. Cause: the window starts 2 frames before the clip and the blend-in takes v84-v88 (0.17 to 0.86). Make
   the two files agree (window from flip + blend time).
5. gioco, small: the chest stays about 25 deg short (-154) from v93 to v102 and squares up v103-v106, because the
   actor yaw arrives late (-122 at v93, -160 at v102). Visible as a 3/4 torso during the first rev step. Not
   blocking; a faster tail of the capsule yaw rate would remove it.

## Not checked
Real speed playback and the game camera feel (game view character is about 20 px: travel looks equal to 26 at
v125-v140, not measured). Jitter was judged on sheets, not on a per-frame elbow trace. The other three directions
are not built (rule 15e: this one first).
