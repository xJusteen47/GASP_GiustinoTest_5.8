# Dropkick tech bounce r4: director's design (2026-11-21, redo, not a tweak of r3)
Sources: brief.md, review-f4.md, base/grid_low_whiff_{az90,az180,game}.png, base/frames.md (low_whiff v159-v172, low_hit v157-v171).

## 1. Judgement of the base (r3 in game today)
LIFE: no anticipation is needed (it is an impact), but there is no breakdown either: one rigid pike (legs ~25 deg up, knees bent) is
translated up and down. Nothing lags, nothing arrives late, so the 3 frames at the top (49/51/50 cm) read as a float. The pelvis path itself is
close to a fair parabola: the float is a POSE problem, not a height problem. Arms open on time but on a body that does not move under them.
| # | Defect | Label |
|---|---|---|
| 1 | Body through floor: NOT seen today in low_whiff / low_hit (pelvis 15-23 cm at contact). High-jump cases are not in this base: recheck there | gioco (verify, not blocking) |
| 2 | The move's a59 shows for 1 frame at touchdown (v159 whiff, v157 hit) BEFORE clip f0, pelvis 56 -> 15 in one frame, pose tilted in the side view. The visible contact frame is a59, not our f0, so the first-frame rule and the stacco are judged on a frame we do not author | dati montage (decide BEFORE pose work) |
| 3 | Same clip plays differently: whiff rate 1.04 and pelvis 17/19/23 on f0-f2, hit rate 1.00 and 23/32/40 | gioco (measure: log capsule z per frame) |
| 4 | Squash 1 frame, rigid pike, knees 20-30 deg, peak 51-57 | posa |
| 5 | GroundHit f1 -> f2: pelvis -11, head -23 cm in 1 frame; r3 arrives at -8 so the pelvis is fine, the head whip is inside the owner asset | dati montage (owner asset, see questions) |
| 6 | Game camera: body ~45 px seen along its axis; only width, height off the floor and leg lift can read | posa (design below) |
No frozen frame (the air loop a22-a26 keeps falling 25 cm/f). No pop at the clip -> GroundHit blend in the pelvis. Nothing blocks pose work
except item 2, which is one decision: either the bounce clip starts ON the touchdown frame, or a59 is re-posed to key f0 below.

## 2. Idea
A wrestling back bump of a rigid log, then a decaying double bounce: impact 1 (ours) -> bounce -> impact 2 (GroundHit f2) -> GroundHit's own
+11 cm hop. The body is a shallow V at contact (hips first), the impact flattens the V over 3 frames (that IS the squash), the hips leave
first (bow), chest/head and heels catch up at the apex (flat), overshoot into a shallow V on the way down (hips land first again). Arms stay
crossed 5 frames, then are left behind in the air while the trunk drops. One wave through the body, no rigid pose held for 2 frames.

## 3. Keys (12 frames, f0..f11, 1:1 at 60 fps; f12 = GroundHit f1)
Pelvis z is WORLD (floor = 0). Clip z = world - capsule offset of that frame. Assumed capsule hop (replace with the measured one):
f0 0, f1 7.6, f2 13.9, f3 18.8, f4 22.2, f5 24.3, f6 25, f7 24.3, f8 22.2, f9 18.8, f10 13.9, f11 7.6, f12 0.
Spine/head sign as in the r1 sheet: negative = curl forward / chin tucked, positive = laid back. Leg = angle of the straight legs above the floor.
| Key | Pelvis z / clip z | Pitch | Spine | Head (world z) | Arms | Legs | Why |
|---|---|---|---|---|---|---|---|
| f0 CONTACT | 15 / 15 | 82 | -8 | -22 (24) | crossed, tight | together, straight, 15 deg up, knees 0 | hips hit first; shallow V, chest and heels still in the air: clear change from the air pose (head was +22 over pelvis) but still the log |
| f1 | 12 / 4 | 90 | -2 | -16 (20) | crossed, elbows sink 2 cm | 10 deg, knees 5 | upper back slaps down 1 f after the hips |
| f2 SQUASH | 11 / -3 | 92 | +4 | -6 (16) | crossed, elbows sink 3 cm, shoulders shrug 2 cm | 5 deg, knees 10 (max), calves touch | deepest; lower back pressed flat, heels arrive 2 f after the hips |
| f3 (hold end) | 12 / -7 | 91 | +6 | +8 (13, touches lightly) | crossed | 3 deg, knees 6, heels on floor | head arrives LAST; 3 frames f1-f3 with the pelvis on the floor |
| f4 RELEASE | 23 / 1 | 94 | +10 | +10 (16) | crossed | heels still on the floor (legs point ~10 deg DOWN from the hips), knees 0 | hips leave first, chest/head and heels drag: bow, convex up |
| f6 | 40 / 15 | 93 | +4 | +2 (31) | L hand 25% along the arc, R still crossed | 9 deg, together | heels peeled off at f5; left arm starts at f5 |
| f8 APEX | 45 / 23 | 90 | -4 | -8 (44) | L 60%, R 20% (started f7), elbows lead, wrists 1 f late | 16 deg, together | body passes through flat; no hold key here |
| f10 | 41 / 27 | 86 | -10 | -14 (52) | L 80%, R 55%, hands in WORLD targets (they stay up while the trunk drops) | 22 deg (peak of the leg follow-through at f9-f10), start to part 30% | shallow V again, hips lead the fall |
| f11 | 36.5 / 29 | 83 | -13 | -13 (48) | L 90%, R 75% | 18 deg, parted 65% toward GroundHit | moving through, NOT a copy of GroundHit f1 and NOT a hold |
World pelvis every frame: 15, 12, 11, 12, 23, 33, 40, 44, 45, 44, 41, 36.5 -> GroundHit 30, 19. Frames not in the table (f5, f7, f9) are
spline in-betweens that must hit these heights within 2 cm.

## 4. Mechanics
- **Contact order**: hips f0 -> upper back f1 -> calves/heels f2 -> head f3 (light touch, chin was tucked: the helmet does not slam).
- **Squash**: 3 frames (f1-f3), pelvis 3-4 cm below its f0 height (floor report: lowest surface never under -2 cm), spine from -8 to +6, legs
  15 -> 3 deg, knees give together to 10 deg max at f2 and are straight again at f4. The clip counter-lifts the capsule (clip z down to -7) so
  the body stays glued to the floor while the capsule already rises: this delay is what makes the rebound a consequence.
- **Rebound**: world pelvis 12 -> 45 (+33 from the squash, +30 from lying), apex at f8, 6 cm lower than r3 in whiff, 12 lower than in hit. The clip
  adds no extra jump: clip z climbs in one monotone line from -7 to 29 and ends on GroundHit's 30. Launch is the fastest frame (+11), then
  +10, +7, +4, +1: nothing accelerates upward after f4.
- **Arms**: crossed f0-f4. Left starts f5, right f7 (2 frames overlap). Arc: hand lifts off the chest toward the ceiling first (via point
  ~12 cm above the straight line at mid-way), then out; ends at GroundHit key 0 hands (`cards/reaction/reaction_groundhit.plan.json`, r1 sheet:
  L [50,-45,46], R [-44,-54,52]). No hand faster than 18 cm/frame in world. f10-f11 as world targets so the arms lag high while the trunk falls.
- **Fall into GroundHit**: world pelvis steps -1, -3, -4.5, -6.5 (f11 -> f12), then the asset's -11: one accelerating fall, impact 2 at
  GroundHit f2. Pitch 83, spine -13, head -13, legs up 18 deg at f11 are one step before GroundHit key 0, with velocity, never a hold.

## 5. Curve sheet
| Part | Timing | Easing | Overshoot / follow-through |
|---|---|---|---|
| Pelvis z (world) | f0-f3 floor, f4-f8 rise, f8-f11 fall | impact: linear in, no ease (f0 is already on the floor); rise: fastest at f3->f4, ease out to f8; apex: pass through, no hold; fall: ease in (gravity), fastest at f11 | none upward; squash -4 cm |
| Pelvis pitch | 82 / 92 / 94 / 90 / 83 | 82->92 in 2 f, fast; rest smooth spline | +2 past flat at f2, +4 at f4 (hips lead) |
| Spine | -8 -> +6 (f3) -> +10 (f4) -> -13 (f11) | follows the pelvis 1 f late | +10 arch at f4 is the drag of the chest |
| Head | -22 -> +8 (f3) -> +10 (f4) -> -14 (f10) | 1-2 f behind the spine all the way | arrives last on the floor (f3), last off it (f5) |
| Arms | L f5-f11, R f7-f11 | ease in 2 f, then constant along the arc; no ease out (GroundHit finishes it) | wrists 1 f behind the elbows; arms stay high on the fall |
| Legs | 15 -> 3 (f3) -> heels drag f4 -> 22 (f9-f10) -> 18 | slap down fast f0-f2; leave the floor last (f5); float up after the apex | peak 1-2 f after the pelvis apex; together and straight to f9 |
Only `hold: true` allowed: none. If the executor needs a stop, it is a design error: report it.

## 6. What must read in the GAME camera (review order: traits, f0, stacco, then the rest; judged on the real-speed game-camera mp4, not a mosaic)
1. Width: one narrow log for f0-f5 (arms crossed, legs together), width doubles between f6 and f10, never in one frame. Carries traits 1-2.
2. Stacco: the head-and-shoulder mass drops from +22 over the pelvis to the floor in 4 frames and the far end (feet) drops with it:
   the silhouette gets shorter and flatter for 3 frames. That is the landing clip separating from the active pose.
3. Trait 3: 3 frames with no shadow gap, then the gap opens fast and closes accelerating; the feet (far end) rise AFTER the body.
4. End: legs still one column at f9, a V only in the last 2 frames, so the bounce cannot be mistaken for the generic knockdown.

## 7. Checks for the executor before capture
Floor report on every key; world pelvis per frame against the list in section 3 (+-3 cm) in BOTH whiff and hit; knees <= 10 deg; no key
pair with identical pose; capture low and high jump, whiff and hit; side + behind + game camera, real speed.

## 8. Questions for the owner (nothing bent)
- a59 (table item 2): may the bounce clip start on the touchdown frame, or may a59 of the dropkick be re-posed to our f0? Without one of
  the two, the first-frame rule is judged on a frame nobody directed.
- GroundHit f1 -> f2 moves the head 23 cm in one frame. We arrive falling, so the pelvis is continuous; the head whip stays. Accept it as
  impact 2, or allow a data option to enter GroundHit at f2 from a clip one frame longer that ends on its own floor contact?
- The three traits and the 25 cm hop are all kept; none of them blocks a good result.
