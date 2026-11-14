# Get-up roll abilities: travel curve the clips expect (2026-11-14, anim-finish-3)

Both roll clips are authored IN PLACE (root at the origin on every frame, the body ends standing on the root). The ability moves the
capsule. A constant speed makes the body slide on its back and the planted hands and feet skate. The clips expect this instead
(clip frames at 60 fps; fraction of the ability's total distance):

| Clip | Direction | f0 | hold until | main travel | ease out | after |
|---|---|---|---|---|---|---|
| `AS_React_Roll` (forward rock-up, 24 f) | toward the feet | 0 | f6: 0 (rock back on the upper back, knees to chest) | f6-f10: 0 -> 0.7 (sit-up and hand push, feet land f10) | f10-f15: 0.7 -> 1.0 (low tuck over the feet) | f16+: 1.0, no travel while rising |
| `AS_React_RollBack` (over the right shoulder, 24 f) | toward the head | 0 | f4: 0 (legs swing up) | f5-f12: 0 -> 1.0, most of it f8-f12 (hips pass over the shoulder, hands planted, feet land f12) | none | f13+: 1.0, no travel while rising |

Numbers behind it (offline, from the plans; `travel.py` in the thread's scratch, hands and feet kept from sliding):
- RollBack: the clip itself implies about 65 cm toward the head between f5 and f12 (11 cm by f8, 44 by f10, 63 by f12). If the ability's
  distance is far from 65 cm the planted hands slide by the difference: 60-90 cm looks right, more needs a clip with authored travel.
- Roll (forward): the clip implies almost no travel (feet tuck under the hips, about 10 cm). Any distance the ability adds is a slide; it
  hides best in f6-f10 (body in the air between the back and the feet). After f10 the feet are planted: travel there skates them, so
  keep the f10-f15 share small. If the slide still shows in game, the clean fix is travel authored in the clip (root motion).

Not done here: the abilities (`GSTN_GA_GetupRoll`) still move at their own curve; this is a note for the code thread, no C++ changed.
The Play captures in this folder play the clips in place (no ability travel).
