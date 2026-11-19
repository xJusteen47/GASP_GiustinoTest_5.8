PASS - the dash clip plays in game from v45, all 3 identity traits read, the hand-off to the run has no pop; 5 non-gating fixes (dust size first).

Reviewer: fable, in-game capture Saved/Dash/cap5 (60 fps). Seen: 5 grids (mv_dash side/behind/front34/game, base side), 8 single
frames (az90 v44 v50 v54 v61 v66 v74, az45 v56, game v54), frames.jsonl v38-112 of both sequences. Frames are capture frames (v).
Limits: only one case captured (idle, stick v40, L3 v43, one direction); the dash from a run already in progress, the most common
case in play, is NOT covered. mp4 not opened. ABP state not logged: the clip is identified from poses and numbers.

## Identity traits
1. Compact load + dust - PASS. v45-50: pelvis_z 85 -> 58.6 (v50, no hold at the bottom), lean 17 -> 37 deg, fists by the hips
   (hand z -27), knees deep and hunched (az90 v50). Dust burst starts v50 at the feet. Base at the same frames jogs upright in place.
2. Lunge - PASS. v52-57: lean 56/58/58/56/54/52 deg (6 f above 50), pelvis 66-70, rear (right) leg straight back with the toe
   grazing (z 8-12), both fists low and forward (hand_r x 42-49, z -35), head down (az90 v54, az45 v56). Split peaks 83 cm at v56.
3. Heavy settle - PASS. Left heel down v60 with the pelvis flat at 69.2-69.4 on v58-60 (no rise in the air: r2 fix 1 holds in
   game), compression to 66.4 at v62, rise 70.8/74.7/77.4 on v64-66 as the foot passes under the hips (az90 v61, v66).

## Does the clip play, and the hand-off
- Stage 0 -> 1 at v44, dash counter 0 at v44, load pose from v45: the clip starts 1 frame after the boost. Root: dx 0.9/2.7/5.1/
  7.2/8.6/9.2 then 9.8 cm/f from v52 (588 cm/s, base 450). Top speed is reached in ~8 f, not the ~4.3 f of the curves sheet.
- Contacts: left foot in the world 102.6 -> 104.2 cm on v60-65 (1.6 cm slide over 49 cm of root travel). Push foot ankle -45.1 ->
  -34.3 on v48-53 while the heel lifts (z 1 -> 9): toe pinned, no visible slide.
- Clip end ~v69-70 into the run: pelvis 79.6/80.6/81.4, feet and hands continuous, no pop, no double pose.
- v47 is identical to v46 (dash counter 2 on both) and v52/v76/v100 skip a sim frame (dx 19.2-19.5): capture cadence, also in base
  (v74, v98). Not a frozen animation frame.

## Items (all non-gating)
1. [gioco - VFX] Dust too big, opaque and long: from the game camera (behind) the cloud is as large as the character and sits
   between camera and body on v50-66, right on the lunge (game v54, behind grid v50-58); it lives until ~v82 (32 f). It is a flat
   white blob, not lit smoke (VFX direction v3). Make it ~1/3 the size, low at the feet, gone in ~15 f.
2. [gioco / dati montage] Run-start flick before the load, v43-45: the stick arrives 3 f before L3, so 2 frames of the plain run
   start play first: right foot up 13 cm at v44 then back on the ground at v46, hand_r x 12.7 -> 27.9 -> 15.6 -> 8.4 and z -2.5
   -> 5.0 -> -13.6 (az90 v44 vs v46). Reads as a small twitch before the anticipation. Blend in from the current pose over 3-4 f
   or accept the boost a few frames earlier so the run start never shows.
3. [gioco] Second step is flat and late, v70-88: pelvis 81.4-82.6 level for 10 f (v71-80), no dip on the right-foot contact at
   v76-77 (the sheet expected ~v71), right foot creeps 281.7 -> 286.7 on v77-82, then the body climbs 12 cm to 93.6 by v88 and the
   lean goes to -1 deg (v87-91). After a heavy catch the brute pops up into the stock upright jog: enter the run loop at the phase
   just before its right contact (open since r1 fix 8 / r2 fix 5) and keep some forward lean while boosted.
4. [posa] Lunge split still short: 83 cm at v56 against 0.92 m in the sheet (~96 cm at this character's scale). Push the rear
   foot back ~10 cm on clip frames 10-13. Low priority: the line already reads (az45 v56).
5. [gioco] Mesh lag on a skipped sim frame: pelvis offset from the actor jumps -9/-10 cm (v52, v76, v100) and takes ~15 f to
   recover (v76 -9.2 -> v91 0.1). Same in base; a real hitch would show it too. Not an animation defect.

Capture to add before calling the move closed: L3 pressed while already running, and L3 on the same frame as the stick.
Base note (out of scope): mv_dash_base jogs in place for ~25 f (v44-66, 7 cm of travel while the feet step 15-25 cm high).
