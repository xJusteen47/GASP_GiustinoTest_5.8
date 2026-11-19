# Body Mechanics (Gnomon workshop, instructor Kevin), study notes

Private study notes from a licensed course. Paraphrased; every claim carries `lesson mm:ss` (transcript time, whisper
may mis-hear words) so it can be checked in the original. No transcript text, frames or video are stored here.
IDs refer to the animation knowledge base (`game-anim-vfx-review-skills/knowledge/animation/`).

Scope: a Maya workflow course on animating with the **master (placement/"god") control** as a working control instead of
a parking control. The instructor copies a stylised-action reference (a ninja-like character running along rooftops,
a step up, a big forward leap, a front flip with a spin, a landing and a run on) with a simple biped rig. He first
builds a small jump to teach the method, then the full parkour shot in blocking, then polishes hitches, absorbs a big
director note (a new wall, the character must run along it), and finally exports so the master is clean for the next
department or a game engine. The craft content is thin and scattered (it is a workflow course): jump mechanics
(squash, stretch, hang time, linear air travel), spacing and pops across blocked segments, foot contacts, limb drag
and arcs in screen space, polish and blocking depth. What transfers to a game pipeline: the discipline of keeping
the big placement/rotation separate from the pose, the hand-off rule "no animated master/root helper on export",
and the checks on pops, air trajectory and contacts. The Maya tool clicks do not transfer; the principles behind
them do. Reference clip is stylised, so some choices (long hang time, spins) are taste, not rules.

## Lesson 00 - Preview (1 min)
- A solid, repeatable workflow is what lets an animator move between styles and productions - 00:26 - TOOL
- The method of the course: use the placement control as a full animation control for big translation/rotation - 01:09 - TOOL

## Lesson 01 - Why I Like Using the Master Control (~13 min)
- Moving or turning a whole pose by one control leaves the pose itself untouched, so posing and timing can be the focus (the 2D-drawing freedom) - 01:58, 12:35 - TOOL
- Once a body is rotated, per-axis curves of the centre of mass mix (X and Z both move when only "sideways" is meant), so graph-editor edits no longer match what the viewport shows; keep big rotation in a parent so the remaining curves follow world axes - 03:10, 05:10 - maps to TOOL (and NEW P-GAME-12 for the root-channel idea)
- Pose mirroring, zeroing a control to default, and copy/paste of poses all survive only if placement is on the parent, otherwise the pose pops to a stale place - 05:38, 09:30, 11:03 - TOOL
- Posing a launch means going from squash to stretch; the squash is the load before the spring - 10:22 - maps to P-WEIGHT-03

## Lesson 02 - How to Use the Master Control, Part 1 (~23 min)
- Rotating/moving a whole character as one rigid block makes feet slide and the body deform; the cure is to break the motion up (placement, rotation, pose) rather than lock every foot every frame - 00:15, 01:16 - maps to P-CONTACT-01 (the cause) and TOOL (the cure)
- Snapping a foot to the world on one frame only works while scrubbing stepped; it does not hold when sliding the timeline, so locks must be real keys/channels - 01:49, 02:04 - TOOL
- Think of the character as sitting at a fixed default under the master: poses read as a stretch/squash/flip pose you can drop anywhere; focus on dynamic poses and squash-and-stretch mechanics - 03:42, 04:38 - maps to P-POSE-02, P-STRETCH-02
- Build the jump as stepped poses first (stand, squash, jump stretch, hang squash, land squash, relax) before any curves, because you must understand where the mechanics go - 06:04, 08:40, 10:32 - maps to P-TIME-03
- After first playback the result "is a start", not good; then fix hang time and remove ease-in on the take-off so the down-then-pop is sharp - 10:55, 11:13 - maps to P-TIME-03, P-ANTIC-04
- The apex needs real hang time; add frames at the top to get it, and add more if it feels light - 11:01, 13:19 - maps to P-TIME-02
- Take-off: a down squash, then a sharp stretch; the in-air pose still ends in a relative squash before landing - 06:17, 06:54 - maps to P-WEIGHT-03, P-WEIGHT-04
- The more care put into placing weight-bearing feet and pole vectors at block-out, the less polish work later - 13:44, 14:20 - maps to P-TIME-03
- In the air nothing speeds up or slows down the body horizontally, so horizontal travel is linear, and any lateral direction change must happen at release, not mid-air; the vertical is the curve that makes the arc - 15:31, 16:07 - maps to P-ARC-04 (refines, see below) and NEW P-ARC-05
- Remove odd bumps in curves by deleting stray keys and smoothing rather than adding more keys - 18:55, 19:07 - maps to P-ARC-02
- Double translations/rotations (a parent and a child both carrying motion) make polish hard; eliminate one before polishing - 21:13, 21:30 - TOOL
- Naming convention matters because scripts select the opposite side - 21:44 (L02; also L03 00:45) - TOOL
- Copy a squash pose and change feet/arms slightly to get the next pose in the chain (squash, stretch, hang squash, landing squash) - 07:10 - TOOL

## Lesson 03 - How to Use the Master Control, Part 2 (~28 min)
- Converting placement into per-limb world-space controls exposes all the foot sliding so it can be fixed directly - 01:46 - TOOL (the sliding itself maps to P-CONTACT-01)
- A foot locked on a frame in world space keeps its place without extra keys; doing this during blocking saves cleanup later - 04:38, 04:54 - maps to P-CONTACT-01
- Fix hang time and jump height together, lower the whole arc a bit if too high, then polish the descent - 02:39, 03:13 - maps to P-ARC-04, P-TIME-02
- Landing feet should not touch down simultaneously; add a frame so the second foot lands later - 03:26, 03:40 - maps to P-CONTACT-03
- After the impact the body stays flat on the ground (zero rotation, no further drift) - 05:41, 06:26 - maps to P-CONTACT-01, P-WEIGHT-04
- Ask of the polish pass: is the down held long enough, can it hold longer and pop up faster, does it overshoot on the pop-up - 10:48, 11:03 - maps to P-WEIGHT-03, P-SETTLE-01
- The animator keeps the master keyed and can "toggle back" to rework big motion after a change, so the temp controls are a polish tool, not a one-way door - 11:11, 14:01, 17:03 - TOOL
- A director's easy change ("jump forward", "spin 360", "do it facing another way", "move him over there") is cheap with the master: re-edit one parent curve and rebake - 13:51, 21:47, 25:20 - TOOL
- Do the big changes before the master is baked down, because arcs and screen path are easy to read there ("he moves screen-right first") - 26:57 - TOOL
- A delayed lean-back after the landing gives a little overlap between body and path - 19:25, 20:04 - maps to P-OVERLAP-01
- Flag floatiness on the descent as a known issue to fix later, do not chase everything at once - 20:46 - maps to P-TIME-03
- Hygiene: animating the master can break the export, to be addressed in the last chapter - 12:33 (L03) - maps to NEW P-GAME-11 (pointer to L08)

## Lesson 04 - Applying the Method, Part 1 (~38 min)
- A rig with only one master control cannot be used directly; add parent/offset locators (translation, flip axis, spin axis) so a clean offset carries the translation and the rotation locators are separate and stackable - 01:26, 02:09, 04:16 - TOOL (idea maps to NEW P-GAME-12)
- Place the rotation pivot at the centre of mass (COG) so a flip turns around the real centre - 03:09, 03:14 - maps to P-WEIGHT-01
- Break the shot into beats first: step-up, leap, run, big flip, landing, run on - 04:29 - maps to P-TIME-03
- Avoid perfectly straight paths and perfectly lined-up poses; a little sideways drift and rotation makes the motion look less mechanical - 05:41, 05:54 - maps to P-LIFE-06
- Keys stacked on one frame for the whole body (set of controls) avoid leaving parts out of a pose - 06:13, 06:30 - TOOL
- Key the landing first, then work backwards: easier to find the end point then the in-between - 07:44 - maps to P-TIME-03
- In-air horizontal translation is linear (again), vertical gets the apex and hang time - 08:49, 09:16 - maps to P-ARC-04, NEW P-ARC-05
- Take-off from a jump already in progress has no ease-in; the landing needs a few frames of squash after contact - 09:52, 11:04 - maps to P-WEIGHT-04, P-ANTIC-04
- Do not cheat the placement a little: "where does cheating end"; he keeps landing foot positions honest - 11:36, 11:47 - maps to P-CONTACT-01
- A run cycle that was animated separately can be inserted, offset, bent forward and repeated; the foot-on-ground root travel is derived from the cycle's own foot travel and pasted repeatedly - 12:16, 28:03, 30:13 - maps to P-GAME-02 (root speed matches foot travel), TOOL
- The cycle is cut where the action needs: "only need a little of it", not the whole run - 12:55 - TOOL
- A turn/crouch into the leap, with a twist and arms back, is the anticipation for the big jump - 31:28, 32:09 - maps to P-ANTIC-02
- Rotation of 360 degrees reads the same as 0 at the end pose, so the flip can be authored as an offset spin with the pose unchanged - 20:23, 20:36 - TOOL
- Do not move the COG out of the flip pivot; pose everything else around it so the pivot stays put - 21:31, 21:47 - maps to P-WEIGHT-01
- Landing needs the body already rotated forward and legs not spreading too fast; fix opening legs by delay - 23:51, 24:31 - maps to P-WEIGHT-04, P-SPACE-01
- More work up front gives less polish work later - 25:02 - maps to P-TIME-03
- If a pose was changed too much, snap back to the earlier pose and reuse it - 25:34 - TOOL
- Place the camera so the action is visible during the leap; the hardest part of the lesson was the camera, not the animation - 29:41 (L04), 34:18 (L05) - maps to P-POSE-03
- Stretch pose in the air is kept for most of the flight; apex higher for a big leap - 37:00, 37:53 - maps to P-STRETCH-02

## Lesson 05 - Applying the Method, Part 2 (~34 min)
- Check the jump in perspective (the shot camera) before judging; feels light, add hang time - 00:26, 00:42 - maps to P-GAME-08, P-TIME-02
- Rotation X (the flip) and rotation Y (spin) are authored as separate curves; spin amount can be tuned by one curve (1080 first, then 720 as too much) - 03:35, 04:27, 19:29 - TOOL (maps to P-TIME-02)
- The body should already lean into the direction it will rotate; push the lean in sooner than feels natural - 06:11, 06:14 - maps to P-ANTIC-02, P-WEIGHT-03
- When the body transitions from the spin into the scoop-over pose, make the pose change readable in perspective: legs lead the action from upside down, then arch, then come down - 13:09, 15:51, 15:56 - maps to P-POSE-05, P-OVERLAP-02
- Add a cushion of shared keys on every red-ticked frame so a retime does not break existing timing - 12:39 - TOOL
- Arms drag from where they were during the spin (mirrored poses make the arms trail), which adds physics and weight - 26:11, 27:05, 27:21 - maps to P-OVERLAP-02, NEW P-OVERLAP-06
- Evolve the pose across keys: copy the previous pose and change it slightly each time - 27:40, 27:46 - maps to P-POSE-05
- A pose that "pops out too fast" needs more time spread or lower amplitude; find the middle ground - 27:52, 28:53 - maps to P-SPACE-01, P-TIME-01
- Wobble in the COG near a hold is flagged and left for polish - 02:32 - maps to P-HOLD-02
- A bad landing pose is acknowledged; the landing needs more stretch before the slam - 12:06, 30:44 - maps to P-ANTIC-02, P-WEIGHT-04
- Rotate/move the end frame to fix the final tilt instead of editing everything - 31:41 - TOOL

## Lesson 06 - Polishing Out the Hitches (~35 min)
- Find the hitches: a pop after the land, a sucked-into-the-ground drop, a long hang that then speeds up - 00:21, 00:34, 25:25 - maps to NEW P-SPACE-06
- Look from the shot camera and from perspective, not only from the working view - 03:10, 08:14, 11:41 - maps to P-POSE-03, NEW P-SPACE-07
- The body goes under the ground at the landing; raise it, and the feet stay flat once they land - 03:38, 03:46 - maps to P-CONTACT-01, P-WEIGHT-04
- A first drop that is too sudden gets delayed; the body rises slowly after the land, then moves forward over a few frames instead of popping forward in two - 04:10, 04:30, 04:38 - maps to P-WEIGHT-04, NEW P-SPACE-06
- Transition forward is eased out, not constant; soften the whole transition - 04:42, 05:10 - maps to P-SPACE-01, P-SETTLE-02
- Cheat the knee (bend/length) when the rig limits it, but do not rely on cheating - 06:34, 10:52, 11:04 - TOOL
- A foot that appears stuck in space or pops from the camera view is a contact error even if the real position is fine - 09:32 - maps to P-CONTACT-01
- A big spacing pop on the push foot gives more power and weight when intended - 17:55, 18:27 - maps to P-SPACE-01
- Stylised characters can stretch more, but too much reads as a problem; pick the stretch that reads as a shape - 14:55, 15:09, 15:51 - maps to P-STRETCH-03
- Too tucked and a weird knee: pose the lower body again instead of fixing curves - 14:41 - maps to P-POSE-01
- Land on one foot first, with a flat foot and no heel strike if it reads as a fast ninja landing - 20:40, 20:51 - maps to P-CONTACT-03
- A first foot comes off the ground while the second stays; "all this could be on the ground" until needed - 21:38, 21:51 - maps to P-CONTACT-03
- Add weight with the initial spacing and a bigger cushion; the landing could be less soft and the run start faster - 23:42, 24:49, 24:56 - maps to P-WEIGHT-04, P-GAME-05
- Fix a speed-up by retiming a section (stretch time to slow the quick part), then re-key every frame and clean spikes - 25:39, 28:35, 29:50, 31:12 - maps to P-TIME-01, TOOL
- Polish pass is not a final pass: pops removed, mechanics plausible; arm/spine overlap and weight shifts are for later - 32:11, 32:22, 32:30 - maps to P-OVERLAP-03
- A highly trained character needs less overlap; do not apply crazy overlap to an agile character - 32:40, 32:52 - maps to P-OVERLAP-04, P-OVERLAP-03
- Early blocking carelessness ("I just want to see things move") costs a lot of fix time later - 13:40, 13:56, 14:12 - maps to P-TIME-03

## Lesson 07 - Addressing a Big Note (~14 min)
- A big change after approval of blocking (a new wall, a new camera) is normal; work so the change is a small edit of one parent path - 00:19, 12:07 - TOOL
- Trust the note: such notes almost always improve the shot, even late - 01:09 (L07; also L06 34:30) - TOOL (process, not a criterion)
- Fix the first problem first (no crashing through the wall) before the structural change - 01:55, 02:23 - TOOL
- Attach the placement to a curve (motion path) and keep it linear so speed is controlled by the animator - 03:04, 04:25 - TOOL
- Moving the path (changing curve control points) can raise a jump or reroute the run quickly - 09:06 - TOOL
- Sliding along the wall is fixed by adjusting the path position and snapping to world, not by deleting the shot - 06:53, 07:15, 08:32 - maps to P-CONTACT-01
- Run along the wall with the body leaning up; change the rotation order to avoid gimbal - 05:26, 05:40 - TOOL
- Speed too high on the exit: extend the transition so it does not pop - 10:28, 10:38, 10:52 - maps to P-SPACE-01
- Show a rough pass early to a lead or director: ideas first, polish after approval - 13:25, 13:42 - maps to P-TIME-03
- Rough big changes accepted: more jumps or a path change is cheap when placement is on the master - 12:17 - TOOL

## Lesson 08 - Polish & Exporting Down the Pipeline (~49 min)
- Do not heavy-polish blocking that goes to review; enough information to sell the idea and show the beats, not so much that hours are wasted if it changes; do not under-bake either - 00:32, 41:11, 41:30 - maps to P-TIME-03
- Fix small hitches in the blocking pass: an airborne foot that floats when turning a corner, snap back to where it was before - 02:51, 04:00 - maps to P-CONTACT-01
- Push-off: the stance leg is extended and straight, the knee goes over, the COG lowers a little; time the push before the leave - 04:18, 04:31, 08:21 - maps to P-WEIGHT-06, P-WEIGHT-03
- Do not let the body pop up; give it a little weight and time so it pushes up slowly to the pose - 06:57, 07:02 - maps to P-WEIGHT-03
- A lagging body is pushed forward by moving the root or body earlier - 07:19, 07:28 - maps to P-GAME-02 (body vs root lag)
- Foot leaves after the push; drag the foot behind a bit for a better push off - 14:31, 14:46 - maps to P-WEIGHT-06, P-OVERLAP-02
- Isolate the timeline range when polishing; stop playing the whole shot - 13:17, 13:24 - maps to P-GAME-01
- Animate parts that are offscreen too: true mechanics make everything else easier, "when you cheat, everything becomes the cheat" - 20:20, 20:53 - maps to P-WEIGHT-06 (process)
- Be accurate rather than cheating a foot that is hidden; it saves later fixes - 16:30, 16:46 - maps to P-CONTACT-01
- Check the arm: a hand popping into view from nowhere breaks continuity; make it come from where it was - 22:14, 22:28, 28:06, 29:24 - maps to NEW P-OVERLAP-06
- A moving arm drags and slowly deforms over time to sell weight; flip between keys to see its path - 28:14, 29:50, 30:17 - maps to P-OVERLAP-02, P-ARC-01
- Check the spacing in screen space: the COG that hovers on a frame line, pops to a place and sticks reads as a pop even if it is correct in world - 25:08, 25:30, 31:28, 31:35 - maps to NEW P-SPACE-07
- The head or body hitting a line and "sticking" then easing: drag it over earlier and ease in so energy transfers smoothly - 32:10, 32:21, 32:33, 33:06 - maps to P-SPACE-01, P-SETTLE-02
- In 3D it is easy to let the computer do the work and forget that pops still need to be removed by hand; in 2D you drew every pose - 33:17, 33:33, 33:37 - maps to P-LIFE-02, P-LIFE-03
- Two legs that pop out then have nothing between them: fix by adding a transition; drag a foot from where it came from - 33:40, 34:11, 35:10 - maps to NEW P-OVERLAP-06, P-LIFE-03
- A pop followed by a stick is worse than either alone - 35:53, 35:55 - maps to NEW P-SPACE-06
- When one arm leaves the frame, the other can take over the silhouette, a method to fill gaps in the read - 40:06, 40:11 - maps to P-POSE-01
- Another animator will inherit the shot: respect the method, document it, hand over something they can use - 01:02, 01:10 - maps to P-GAME-10 (process)
- Games export: animating the actual root/master that must stay at origin is a "no-no"; the exported clip cannot be placed anywhere because it carries its own physical placement - 01:35, 01:42, 01:51 - maps to NEW P-GAME-11, P-GAME-03
- Before export, find everything still attached to the master (arms, head, pole vectors), convert each to world-space, bake over the whole range, check that twisting the master no longer moves anything - 43:07, 44:44, 46:34, 46:41 - TOOL
- Then key the master to zero, delete the constraint, and verify the animation is unchanged ("everything plays the same") - 46:57, 47:06, 47:30 - TOOL (outcome maps to P-GAME-11)
- Leaving the master alone but unanimated can still place the character; zeroing it at origin is the safe hand-off - 47:33, 47:42 - maps to P-GAME-11

## Candidate new criteria (NEW)
| Proposed ID | Rule | Evidence (lessons) | Measurable how |
|---|---|---|---|
| P-ARC-05 | While airborne the horizontal root velocity is constant (no speed-up, slow-down or turn without a stated force); only the vertical part follows the parabola. Direction is decided at release. | L02 15:31, L04 08:49, L04 16:47 | Fit constant velocity to root XZ between take-off and landing frames; residual speed variance small; heading change zero in flight. Complements P-ARC-04 (vertical parabola). |
| P-SPACE-06 | No unmotivated velocity spike or stick at the joins of blocked segments (landing to run, flip to landing, wall run to flip): a few frames of forward pop right after a contact, or a pop then a stall, is a defect. | L06 00:21, 04:38, 11:47, 25:25; L08 32:21, 35:55 | Jerk (second difference of speed) on COG and big limbs: a spike more than ~3x the clip median within +-4 frames of a contact or key join; or a speed jump above x2 across two adjacent frames. Calibrate on mocap. |
| P-SPACE-07 | Spacing, arcs and overlap are checked in the screen space of the game/shot camera, not only in the world: a part that is smooth in world but sticks or jumps in screen space reads as a pop. | L06 03:10, L08 25:08, 31:35 | Project joints with the camera, repeat the speed/jerk checks on 2D positions; compare world and screen results. Complements P-POSE-03 and P-GAME-08. |
| P-OVERLAP-06 | A limb or foot arrives from where it was: the direction of travel is continuous across keys (arm, hand, foot appear from where they trailed, not from nowhere) and trails behind rotation. | L05 27:05, L08 22:28, 28:06, 29:24, 34:11 | Extremity velocity direction relative to the pelvis: angle change between adjacent frames above a threshold (say 90 degrees) outside planned contacts flags a break; also a lag between limb and spine peak. |
| P-GAME-11 | Placement/helper nodes that the engine does not read (master, offsets, temp controls) are at identity on the exported clip; the exported root carries only what the clip documents (P-GAME-03); the baked result is the same pose, frame by frame. | L03 12:33, L08 01:35, 43:07, 46:57, 47:42 | Check that the export root/placement is at origin with no animation (or only the documented root-motion channels); compare baked and source poses (max joint error, tolerance a few tenths of a degree). |
| P-GAME-12 | In a clip whose root carries displacement, root rotation is limited to heading (yaw); flips, rolls and tilt live on the COG/pelvis so heading and travel can be extracted and the per-axis curves stay editable. | L01 03:10, L04 02:09, 04:16, L05 03:35 | Root pitch/roll within ~2 degrees on clips where body tilt is large; heading extracted from the root agrees with the displacement direction. |

## Workflow notes (TOOL)
The master-control method, summarised (Maya, one biped rig):
1. Treat the master as an animation control: one parent for big translation/rotation, with poses authored under it (L01).
2. Add offset locators if the rig has none: translation parent, flip (X) child, spin (Y) child, pivot snapped to the COG (L04 02:09-04:16).
3. Block with stepped poses by beat: stand, squash, stretch, hang squash, land squash, relax; copy poses and change them (L02).
4. Key the master first (route), then the body poses, using one selection set key so nothing is left out (L04 06:13).
5. Make the in-air horizontal channels linear, the vertical gets apex and hang time (L02 15:31).
6. Use run cycles as inserts and paste the root travel from the foot (L04 12:16, 28:03).
7. When polishing, convert COG and feet into world-space temp controls (bake), fix sliding, and toggle back to the master for big changes (L02-L03).
8. For big notes, put the master on a curve (motion path) and edit the path (L07).
9. Before hand-off, convert every remaining master-child (arms, head, pole vectors) to world space, bake, zero the master, delete the constraint (L08 43:07-47:42).
10. Why it matters for games: the exported clip must carry a clean root at the origin that the engine can place; the course states the master/root at the origin must not be animated for engine export (L08 01:35-02:05). It does not discuss root motion extraction in the engine, COG vs root choice, in-place vs root motion, or curve/marker data; those stay with P-GAME-02/03/09.

## Frames worth extracting
| lesson mm:ss | what it shows | for which ID |
|---|---|---|
| L01 03:10 | COG channels mixing X and Z after a 45 degree rotation, graph editor next to viewport | NEW P-GAME-12 |
| L02 10:55 | first playback of the stepped jump, stand/squash/stretch/hang/land chain | P-WEIGHT-03, P-TIME-03 |
| L02 16:31 | linear horizontal curve plus vertical arc after fixing air travel (curves + viewport) | P-ARC-04, NEW P-ARC-05 |
| L04 20:23 | the flip offset (rotation X 360) and the hierarchy of master/offset/rotation locators | NEW P-GAME-12 |
| L05 13:09 | spin into scoop-over transition seen in the shot camera | P-POSE-05, P-OVERLAP-02 |
| L05 27:05 | arms trailing behind the spin (mirrored pose to give drag) | P-OVERLAP-02, NEW P-OVERLAP-06 |
| L06 00:30 | landing pop and "sucked into the ground" before the fix | NEW P-SPACE-06, P-WEIGHT-04 |
| L06 04:38 | COG after the landing, the two-frame forward pop in the graph | NEW P-SPACE-06 |
| L06 25:25 | speed-up between two beats before retiming | P-TIME-01, NEW P-SPACE-06 |
| L08 28:06 | arm coming out of the frame vs coming from where it was | NEW P-OVERLAP-06 |
| L08 25:08 | COG hovering on a grid line, pop and stick in screen space (camera grid on) | NEW P-SPACE-07 |
| L08 46:41 | twist the master after baking: nothing moves; master at zero | NEW P-GAME-11 |

## What contradicts or refines our criteria
- P-ARC-04 (centre of mass in the air is a parabola): the course adds that the horizontal component is linear and the direction fixed after release, and that the vertical curve gets an explicit hang at the apex (L02 15:31, L04 08:49). Refines the measure (horizontal constancy); the stylised hang time (L02 11:01, L05 00:42) is a taste exception, not a conflict, as long as the vertical is still smooth.
- P-TIME-02 (duration fits mass and intent): course tunes hang time by feel ("feels light, add hang") - L02 13:19, L05 00:42; no numbers in the course, so our frame ranges are not contradicted.
- P-ANTIC-03 / P-ANTIC-04: course removes the ease-in on a take-off already in motion and wants "sharp down then squash" (L02 11:13, L04 09:52). Agrees with P-ANTIC-04; adds that a clip entered from a jump needs no ease-in.
- P-WEIGHT-04 (lowest point 3-5 frames after contact): course asks for a few frames of squash and a delayed first drop (L04 11:04, L06 04:10) but gives no frame counts; no conflict, no number to calibrate.
- P-GAME-02 / P-GAME-03: the course's export statement (L08 01:35) is stronger than our wording: no animated master/root at export at all. Our criteria allow documented root motion; keep ours and add NEW P-GAME-11 for the hand-off.
- P-TIME-03 (stepped first): confirmed and sharpened: careless blocking creates the pops fixed in polish (L06 13:40, L02 13:44, L04 25:02), and blocking for review should be "enough to sell, not overworked" (L08 41:11). Our wording could add the second half.
- P-LIFE-02/03 (floaty, morphing in-betweens): course names the cause in 3D: letting the computer interpolate and forgetting pops (L08 33:17). Compatible.
- P-CONTACT-01: the course cheats knee and heel (L06 06:34, L08 20:20) but insists on honest mechanics even offscreen (L08 20:53); no conflict, it supports "contacts exist" in P-CONTACT-02.
- P-OVERLAP-03/04: for an agile trained character a little overlap is enough (L06 32:40); fits P-OVERLAP-04, could be a profile note.
