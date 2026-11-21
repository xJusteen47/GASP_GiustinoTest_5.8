# Handover, review turnaround 180 L variant 28 (style pass), round 1
Thread cloud, Fable 5.2 high, read only. Branch `claude/project-thread-rdcazm`. Evidence `claude/runstart-generic-desktop` b49ec08a.
- Result: **FAIL pose**, not for the owner. Review: `docs/notes/reviews/turn3-v27/180a_style/review.md` (copy in project files
  `reviews/turn3-v27/180a_style/`).
- Capture ASSERT: OK (`assert_v28.txt`), gates `fails: []` (`gates_joints_AS_GSTN_RunStart_Generic_w2t180Ls.json`).
- FAIL items, all pose: P1 arms do not lead (start with the chest at v86-v87), P2 arms hold one pose v88-v94 then tuck in 2 frames,
  P3 left hand on the front of the hip from behind v88-v90, P4 left fist at belly centre v97-v101, P5 right foot kept on the old
  heading v88-v93 (27's feet untouched, owner complaint open).
- Keep: upright pivot (pelvis), no jitter, left step, phase 2 legs.
- Note G (game/root, not a round): pivot reads in two beats, about 11 frames; not re-measured.
- Applied choices: judged the play-camera readability as part of "arms lead"; treated the untouched right foot as an open owner item.
- Next: style round 2 on the desktop (arm layers f0-f14, first pump, right foot yaw), then a new cloud review.
- Reviewer notes: file k = v(k+76); 27 left, 28 right; crop 560x600+1180+200 on the right half works for az180 and az90.

COSTO: 3 $ | 0 min desktop | 75k token review
