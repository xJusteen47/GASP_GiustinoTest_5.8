# Verdict run start v4r8ch: FAIL (hand-off fix itself PASSES; two entry defects remain)

Identity traits
1. Run cycle sped up, cadence slowing: OK. Rev-up now readable from the first frame (two quick foot switches v83-v94, then normal strides, plants about v88 / v95 / v113 / v132 / v149). No pump, no stomp, no special pose.
2. Feet planted on contact: OK with a note. R plant v88-v90 holds within 1 cm; L plant v95-v103 creeps forward about 7-9 cm while flat (v96-v99).
3. Arms and lean as the run, no pop: KO. One broken frame at v93 (see 1 below). No pop at rate 1.0 (v101-v105) and none at the hand-off.

Fixed versus v4r8c (top row)
- Floating half step v122-v134: FIXED. Feet now cross once at v126-v127 (both 12-19 cm above floor for 2 frames, a normal flight), L lands v132-v133 and holds v135-v141 (x 244.7-248.6). Before, both feet hung 21-34 cm high for about 10 frames. No skipped, double or floating step, no hop through v119-v151.
- Rev-up hidden by the 0.3 s blend-in: FIXED visually (run pose is there at v83-v84 instead of v95+).
- 1-frame foot pop v93: NOT fixed, only changed axis.

FAIL items
1. dati montage, v93 (1 frame, clip time 0.183 s in BOTH captures, so it is in the clip, likely the seam between the 3.5x cycle and the 1:1 strides): front view shows torso twisted and leg splayed sideways for one frame; metrics: foot_l y 140.8 -> 133.2 -> 141.1, foot_r y 157.3 -> 166.4 -> 157.8 (stance flashes to idle width, 33 cm).
2. gioco, v82-v84 (entry pop): idle at v82, full run pose at v83 in ONE frame (R foot 9 -> 50 cm up, arms and lean snapped) while the feet are still at idle width; at v84 the support foot jumps 8.6 cm back and 11 cm sideways. The 0.05 s blend reads as a cut, not a fast blend; no anticipation frame at all.
3. gioco, v85-v90 (note, verify): clip weight is 0.82 at v85, falls to 0.50 at v90, back to 1.0 only at v99, mirroring the old 0.3 s curve. Looks like a second instance of the clip cross-fading over 0.3 s under the new entry, so "0.05 s blend-in" is only half true. The assert window v97-v115 does not cover it.
4. posa / dati montage, v85-v86 (minor): during the first 3.5x switch neither foot is within 15 cm of the floor (L 28.7/42.4, R 37.6/25.1) while the actor is still: a tiny hover on the spot.

Not counted: touchdown skids of about 14 cm at v113-v117 and v132-v135 (same as the stock loop plants, inherited); actor_x jumps +7 cm ahead of the root at v107 and v131 (also in v4r8c at v125), mesh stays smooth.

What the owner will notice: the run-in to the loop is finally clean (no more floating half step), but the start snaps from standing to the run pose in a single frame and there is a one-frame body twitch at v93 seen from the front.
