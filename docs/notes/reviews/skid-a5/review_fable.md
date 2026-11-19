FAIL - a4 blockers are fixed (0 frozen frames, release stop is clean), but the skid's second half now pops and is not the same motion twice, and the hand-back stance slides 30 cm
# Review skid attempt 5 (Fable, 2026-11-19)
Source: Saved/Skid/cap5, 60 fps, f = v. Looked at both az90 grids and 5 single frames (release az90 f138, f205; hold az90 f135,
f208, f216). Numbers from frames.jsonl (actor + foot offset = world, planted = foot z about -77/-78), a4 recomputed from
Saved/LF3/rs4/mv_skid with the same script. Not judged: mp4s, sound. ABP state is still not logged (abp_err), so states are deduced.
Timeline release: reverse f118, speed 0 at f132, release f145/146, actor still at f177 (97 cm after the release).
Timeline hold: reverse f117, speed 0 at f131, exit f145/146, full speed f156, hand-back about f191 (deduced), release f205.

## Fixed since a4 (keep)
- Release case, gioco: 0 frozen frames f119-f209 (a4: 20). Leaves at once: left foot lifts f146-147, two real steps (L lands f163,
  R lands f177), pelvis -23 -> -2 by f153. L planted f163-f209 moves 0.5 cm, R planted f177-f209 1.5 cm (a4: 84 cm). f205 reads
  as an idle facing the new way.
- Hold case, gioco: exit drag of the right foot 41 -> 16 cm (1060 -> 1044, f146-f149, then fixed f149-f152). First plants: L
  f161-f167 7 cm, R f176-f181 5 cm. No frozen frame f118-f249, no pop after f152 (hand-back and f205 release included).
- Total right foot travel on the floor from speed 0 to lift: a4 74 cm -> 42 (release) / 49 (hold).

## FAIL items
- gioco 1 (new): right foot pop inside the skid. Release f138: 598 -> 586 in one frame (12 cm, y +7.5); hold f137: 1093.5 -> 1084.5
  (9 cm). a4 has no jump there (smooth 5-6 cm per frame). After the pop the foot keeps slipping while planted: 23 cm (release
  f137-f146), 33 cm with 24 cm sideways (hold f137-f146).
- gioco 2 (new): the skid after speed 0 is not one fixed motion. Same input up to f145, three captures, three poses. a4: left
  foot steps out 28 cm and comes in to y -132, pelvis -24.8. Release: same up to f136, then the left foot stops at y -104 (f137).
  Hold: from f132 the pose stalls for about 6 frames (f132-f137: left foot fixed at 1118, never steps out, pelvis stops at -20,
  head 6 frames late, catches up f138-f145 at 13 cm in 6 frames). az90 hold f135 is an upright narrow crouch, release f138 a wide
  low skid. Something switches between speed 0 and speed 0 + 6 frames; candidate: StickAlong changes sign when the velocity reverses.
- gioco 3: hand-back stance slides. Hold, left foot planted f192-f198 (z -77/-78) goes 588 -> 558 = 30 cm (5 cm per frame; the two
  stances before it 7 and 5 cm). Starts 1 frame after f146 + 45, so it is the 0.2 s hand-back blend.
- gioco 4 (minor, run stop, not the skid): after the f205 release the right foot is carried 6-9 cm above the floor f204-f211
  (460 -> 369), then slides on the floor 33 cm f212-f218, fixed from f224 (0.6 cm). az90 f208/f216 read as a brake step. L fixed
  f228-f249 (0.9 cm). Right foot then shuffles 37 cm at z -74 f235-f249.
- gioco 5 (minor): pops on the lifting left foot at the exit, both cases: 27 cm in one frame (release f147, hold f146; a4 had 21
  at f150). Planted right foot jumps 10 cm sideways one frame later (release f149, hold f148).
- gioco 6 (minor): release f118 identical to f117 (1 repeated frame at the reverse, a4 had 2); hold has none. Idle after the
  release: left foot creeps 18 cm at z -72/-74 f184-f203 (known idle stance drift).
- capture gap (again): release at f205 is 14 frames after the hand-back, 2 frames after its blend ends. A held run through the
  hand-back is still not shown: only one stance, and it slides.
- dati montage: left foot in the skid now 14 cm (release f137-f146) / 8 cm (hold), was 36. posa: none.

## Slide per planted foot (target 15 cm) - a4 -> a5 release / hold
L in skid 36 -> 14 / 8 | R in skid 35 smooth -> 23 / 33 with a 9-12 cm pop | R exit blend 41 -> 16 / 16 | L first plant 13 -> 0.5
(stop) / 7 | R second plant 6 -> 1.5 (stop) / 5 | L at hand-back f192-f198: not captured -> 30 | after early release 84 -> 1.5.

## Next fix (one)
Make the skid clip play untouched from the reverse to its exit frame (f146): find what fires between speed 0 and f138 (pop
f137/f138, stall f132-f137) and remove it. First check StickAlong: it is measured along the velocity, which flips at f131/f132;
measure it along the skid's exit direction, latched at skid start. Verify with two captures that give the same pose f131-f146.
Capture needs: fix the ABP state log, and hold the stick to v265 so the hand-back (f191) has 60 clean frames after it.
