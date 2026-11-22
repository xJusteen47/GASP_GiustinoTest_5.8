# Review Fable: fast ground crouch (crouch-veloce-3)

**Verdict: FAIL** (timing passes, follow-through passes light, style of the held pose fails; two game-side defects).

Seen: side grid, game grid, side frames 040/042/044/046/112/116 (8 images). Not seen: the crouch-veloce-2 base images, the mp4, odd frames. Frame counts below are therefore +/- 1 frame.

## Identity traits
1. **Fast in/out: PASS.** f040 still standing, f042 mid drop, f044 on the floor, so about 4 frames down. f044 is the overshoot (torso folded, head low, fist touching the floor), f046 is recovered, f050 is settled: settle in 6-10 frames. Exit: f112 still low (back leg starting to extend), f114 almost up, f116 standing: 4-5 frames up.
2. **Follow-through: PASS, but light.** Enter: f042 arms hang straight and behind the torso (lag), f044 they whip down to the floor, f046 they come back to the knee; head stays up at f042, dips at f044, lifts at f046. Exit: f114 one arm is still forward, f116 arms down. After f116 nothing else settles: the exit has no overshoot of chest or head.
3. **Style "Ganondorf in Astro Bot", guarded crouch: FAIL.** f046-f110 is a one-knee kneel: back knee and shin on the floor, pelvis sitting on the back heel, torso hunched, head bowed, both fists resting together on the front knee. It reads as "taking a knee / thinker", not as a fighter guarding. The game camera shows only a small dark ball, with no fists and no head line readable.

## FAIL items
- **posa, f046-f110 (held pose).** Back knee 10-12 cm off the floor (both feet planted, weight on the balls of the feet); pelvis about 10 cm higher (about 25 cm above root, not 15); chest opened up about 15 degrees; chin up, eyes forward; fists split and raised 20-25 cm, lead fist in front of the chin, rear fist at the chest, elbows out for a wider silhouette.
- **posa, f042 (enter in-between).** Tall stride pose: pelvis dropped only about 20 cm, then about 45 cm in the next 2 frames, and the feet go from together to about 55 cm apart in 2 frames. Fix: at f042 pelvis 12-15 cm lower with knees bent out; keep the front foot planted where it was at f040 and move only the back foot.
- **gioco, f041-f042 and f111-f112 (camera).** Between f040 and f042 the horizon and the red disc jump up on screen in one step (camera drops with the capsule), and it jumps back by f112 while the body is still low. This is what makes f042 look "lifted": the feet stay on their shadow, the character is not floating. Fix: interpolate the crouch camera/capsule offset over 6-8 frames, or anchor the capture camera to the root and not to the capsule. Inferred from the side frames only; confirm in the mp4.
- **dati montage (or gioco), f116-f142 (after exit).** The exit ends in a wide stride at f116-f118, holds it until f138, then the feet come together at f142: a late foot slide about 0.4 s after standing up. Fix: make the last pose of the exit clip the game idle stance, reached by about f120, so the blend out has nothing to move.
- **posa/dati, f114-f120 (exit, minor).** Add a 3-4 cm pelvis and chest overshoot at f116 and settle by f120; lag the head 1-2 frames.

## Keep
Enter timing, the f044 overshoot, the arm lag at f042-f046, the exit speed.
