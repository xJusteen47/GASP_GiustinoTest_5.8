# Dropkick tech bounce r4: cloud review (2026-11-21)

**Verdict for the pose: FAIL (narrow).** In the side view r4 is clearly better than r3 and the three identity traits hold in whiff.
It fails on the two things the brief puts right after the traits: the first frame does not show the weight arriving, and the stacco
is not readable in the game camera. Two non-pose problems (a59 frame, hit case) would hide a good pose anyway.
Seen: 7 grids, 8 single frames (game v158/159/161/164/168, az90 v158/159/162). The mp4s were not played: timing is from frames.md.

## Life (pictures first)
1. Whiff, side: impact -> flat log on the floor v160-v162 -> hips leave v163 -> arc -> fall. Contact order reads (hips v159, back v160,
   head v161-v162). Rise +10,+10,+6,+3,+1, fall -2,-4,-9,-11: a fair arc, launch is the fastest frame. Better than base, whose rigid
   pike with bent knees just translated. **OK.**
2. Apex v166-v168: pelvis 45/46/44 is still a 3-frame plateau (base 49/51/50), but the pose changes through it (legs and chest keep
   rising), so it no longer reads as a float. **OK, borderline.**
3. v168-v169 (f9-f11): the head keeps climbing to 56/56 while the pelvis already falls 44/40: the body does a sit-up crunch at the
   top. It reads as a voluntary curl, not a passive log, and it sets up finding 9. Design asked 52/48. **posa**
4. v161 -> v162 are almost the same picture (pelvis 16/16, head 15/14): a near hold on the floor. Minor. **posa**

## Identity traits
5. Legs together and straight: yes, v159-v168 (az90 and az180), part only at v169. Base had knees bent 20-30 deg. **PASS**
6. Arms crossed at contact: crossed v159-v163, left arm starts v164, right v166, wide at v168-v169, never in one frame. **PASS**
7. Bounce as a consequence: PASS in whiff (3 floor frames, then release). **Not true in hit**: same clip, same root (1/7/12/17 vs
   5/11/16/20) but pelvis 24/29/33/30 on f0-f3 (whiff 18/16/16/16), head 42/35/33/28. The body hovers 13-17 cm up during the "squash",
   dips 3 cm and leaves: no floor contact at all, peak 51 instead of 46. **gioco / dati montage** (not the pose, but it is half the uses).

## First frame and stacco (game camera)
8. f0 (v159 whiff) against the frame before it (v158, a59): pelvis goes UP 14 -> 18, head does not move (37 -> 37), and the legs flip
   from ~15 deg below the floor line to 15 deg above it. So the change is a 30 deg leg kick in one frame, not "pelvis down, the body
   starts to give". The head/chest mass only drops one frame later (37 -> 22 at v160). Design asked pelvis 15, head 24 at f0. **posa**
9. Game camera, honest: the character is about 25 px tall in a 480 px frame and is seen along its own axis. What I can read:
   v158 dark compact blob, v159 taller pale streak (the legs), v161 a little flatter, v168 wider (arms). The arm opening reads. The
   stacco is a one-frame shape flicker on a frame pair where one side (a59) is not ours; the squash and the shadow gap do not read.
   Against base on the same frames the two game grids are close to indistinguishable. **posa + dati montage** (no proof it reads).
10. a59 still shows for one frame at touchdown (v158 whiff, v157 hit), tilted, feet through the floor (white tip under the grid in
    az90 v158). The visible contact frame is still one nobody directed: design question 8a is still open. **dati montage**

## Technical
11. Floor: no penetration inside our clip (lowest pelvis 16, body lies on the grid at v160-v162). Only a59, see 10.
12. Hand-off v169 -> v170 -> v171: pelvis 40 -> 31 -> 20, continuous, no pop. Head 56 -> 42 -> 19 (hit 59 -> 42 -> 18): 37-41 cm in
    two frames, worse than base (46 -> 41 -> 18). The second step is the owner asset (**gioco**), the first is our end pose (**posa**, see 3).
    Arms go from raised hands to a flat T in the same two frames; same as base, owner asset.
13. Frozen frames: none (lying hold after v186 is the intended hold). Capture ASSERT OK, rate 1.04 whiff / 1.00 hit as in base.

## Next round (max 3, gravest first)
1. **dati montage / gioco, before any pose work**: make the clip own the touchdown frame (no a59 frame at v158/v157) and find why
   hit plays f1-f3 at pelvis 29/33/30 with the same root (blend-in from a59, hit-stop, or the kick pose still weighted). Target:
   pelvis <= 18 on f1-f3 in BOTH whiff and hit, and f0 as the first frame on the floor.
2. **posa, f0-f1**: the nearest mass to the game camera is the head and chest, so put the stacco there. f0: pelvis 18 -> 14 (never
   above the frame before), head 37 -> 26, legs stay at 15 deg; f1 head 22 -> 18. Then give f2 and f3 a real difference (knees 10 deg
   at f2 only, head touches at f3) so v161/v162 are not a hold.
3. **posa, f9-f11**: head 56/56 -> 50/46 (at most pelvis +8 at f11), spine no more than -8, so the trunk falls with the hips and
   GroundHit f1 (head 42) is on the path. Also capture the game camera at native size or add a 4x crop: at 25 px no review can
   certify the stacco.
