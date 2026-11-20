# Stylized Attack Animations, part 1 (Gnomon course, lessons 01-06), study notes

Private study notes, own paraphrase. Source: Gnomon "Stylized Attack Animations", lessons SA01-SA06, from whisper
transcripts (may mis-hear; ambiguous numbers are flagged). Timestamps are `lesson mm:ss` in the original video.
Tags: **GENERAL** = holds for any attack animation (candidate for the public knowledge base); **STYLE** = taste of this
course or genre (project profile only). Mapping: existing ID, `NEW`, or `TOOL`.

## Scope
The course teaches how to build fighting-game attack animations for a stylised, chunky "brawler/boxer" character. The
rig is an "x-rig" in Maya with the Animbot toolset (tween machine, push/pull, arc tracker, world-space copier, silhouette
view, bookmarks for frame ranges). Part 1 covers: choosing a fighting style and an idle stance that is the hub of every
move (01-02); the pose ladder idle -> anticipation -> overshoot -> impact -> recovery (03); fighting-game frame data
startup/active/recovery and blocking retime (04); a light jab of about 8 frames built for instant feedback (05); the same
punch turned into a medium attack of about 18 frames by adding anticipation, holds and settle (06).
What transfers to a 3D action/fighting game: the pose ladder and opposing spine curves, the frame-data partition, the
"light = change on the next frame, short anticipation, long recovery" recipe, holding the impact pose for weight, and
recovery as a designed return to idle. What does not transfer as-is: the exact frame counts (the course never states
its frame rate; see "Numbers given"), the Maya/Animbot workflow, the 2D-fighter stance iconography.
The course says nothing on VFX, smear frames or hit-pause length. Those are not covered in part 1.

## Lesson 01 - Establishing the Attacker's Fighting Style (about 21 min)
- The fighter stance is the character's centrepiece: it must show identity (personality, fighting archetype, weapon) at a glance - 00:24-01:45 - STYLE - maps to P-POSE-06
- Characters that look alike (palette swaps, same game) need clearly different stance families (squat, upright, aggressive, weapon-led) for quick identification - 03:18-03:50 - STYLE - NEW (identity, not a timing/pose-quality rule)
- The pose's direction of lines leads the eye to what matters (centre, weapon tip) - 02:00-02:21 - GENERAL - maps to P-POSE-02
- Every stance must be balanced: a centre line over the support; too far over and the character falls, too far the other way and it looks weightless and ungrounded - 02:32-03:18, 06:15-06:50 - GENERAL - maps to P-WEIGHT-01
- A stance can be symmetric yet slightly different left/right to read as sturdy; break up "arms at the sides" with angles and shape language (squares = strong/stable, triangles/edges = agile) - 04:02-05:30 - STYLE (shape language) / GENERAL (break symmetry) - maps to P-POSE-04, P-POSE-06
- The idle is the hub: every attack, jump, block or dodge starts from it and returns to it, so author idle first and design moves to come back to it - 08:09-10:48 - GENERAL - maps to P-GAME-06
- Research the real martial art or weapon you imitate (kung fu, boxing, kendo...) so balance and stance are credible - 08:46-09:41 - STYLE - NEW (process; reference research)
- Fighters shift and bounce weight left/right, never frozen - 08:37-08:52 - GENERAL - maps to P-HOLD-02, P-LIFE-07
- Building the pose: wide flat-footed base for a brawler, knees follow toes, pelvis tilt and twist counter to the spine arc, fists at different heights, face not covered from the front - 12:22-19:05 - STYLE (brawler choice) / GENERAL (knees over toes, counter tilt) - maps to P-WEIGHT-05
- Check the pose from front, side and every view, not only profile; "good dynamic posing happens from all views" - 20:20-20:42 - GENERAL - maps to P-POSE-03
- Mirror a saved fist/hand pose to save time; pose library - 16:14-16:48 - TOOL

## Lesson 02 - Creating Strong & Readable Idle Poses (about 13 min)
- The fight idle communicates the character's state and attitude (cocky, calm, defensive); it is an identity statement that also holds the player's attention - 00:29-00:46, 12:37-12:51 - STYLE - maps to P-POSE-06
- Use live-action self-reference photos and sketches as a blueprint, then adapt to the character's proportions; do not copy blindly - 01:27-02:05 (and 03 21:28) - GENERAL (process) - NEW (reference-as-blueprint; process only, not measurable)
- Avoid a "blocky" squared-off torso: lean slightly forward, curve the chest, soften the line - 02:02-02:21, 07:57-08:15 - STYLE - maps to P-POSE-02
- Three-quarter head turn rather than strict profile gives a readable eye and face - 02:42-03:11 - STYLE - maps to P-POSE-06
- Do not leave a hard kink at the ankle (straight to bent) - it reads broken; smooth the joint chain - 05:43-06:10 - GENERAL - maps to P-POSE-05 (nearest; actually joint-line quality, partly NEW)
- Stay inside the model's limits: stretching the torso stretches the textures; reduce pelvis stretch to restore a clean side silhouette arc - 08:39-09:14 - GENERAL - maps to P-STRETCH-01
- Use silhouette (flat black view) to judge the pose; nudge the hand and thumb so they read, and remove negative space between feet and ground - 10:42-11:47 - GENERAL - maps to P-POSE-01, P-CONTACT-01
- Feet may be pushed slightly through the floor in the rig to get flat contact/compression when the model is not flat-footed enough - 11:47-12:02 - TOOL (rig trick) - maps to P-CONTACT-02
- Idle needs micro-movements between poses to stay alive; look for a breathing cycle (promised for a later lesson) - 00:46-01:07, 12:51-12:57 - GENERAL - maps to P-HOLD-02, P-LIFE-07
- World-space copier for sub-controls; reset pose hotkey - 05:09-05:37 - TOOL

## Lesson 03 - Anticipation, Impact & Follow-Through (about 31 min)
- The anticipation is the visual clarity key: it shows what the character is about to do; holding it conveys how much strength goes into the action - 00:54-01:23 - GENERAL - maps to P-ANTIC-01
- Between light, medium and heavy attacks, the anticipation is what ultimately determines how heavy the attack reads - 04:05-04:13 - GENERAL - maps to P-ANTIC-02 / P-TIME-02
- A hit needs two ingredients: a spine line (curve) and the opposing curve; the anticipation bends the line the opposite way of the strike (e.g. D-curve vs C-curve, spine and hips rotated backward) and the swap between the two poses sells force - 03:00-03:44, 05:41-05:57, 14:14-15:07 - GENERAL - maps to P-WEIGHT-03 (load before force), NEW (opposing-curve contrast measured as angle difference)
- Weight shifts back in the anticipation, forward in the impact; stop pushing the back leg before it hyperextends (the leg snapping marks the limit) - 06:00-06:08, 09:12-09:36 - GENERAL - maps to P-WEIGHT-03
- The impact pose pivots the foot off its toe, hips lead and rotate, chest follows, head turns into the punch "like an aiming tool", eyes stay on target - 15:22-20:21, 27:55-28:04 - GENERAL - maps to P-WEIGHT-06, P-OVERLAP-02
- Spine line of action from foot to fist in a clear sequence; don't break the back; judge it flipping between poses - 18:29-20:48 - GENERAL - maps to P-POSE-02
- Clear path of action for the striking arm: offset arc, arm not hidden in front of the body, punch stands out in silhouette from the front - 11:48-12:35 - GENERAL - maps to P-ARC-01, P-POSE-01/03
- The overshoot ("snap") pose goes past the contact point: an exaggerated extreme that "catches the brain" and gives bite; the sequence for the fist is anticipation corner -> overshoot -> hit -> recovery - 22:15-23:32, 25:37-26:13 - GENERAL - maps to P-SETTLE-01, P-OVERLAP-05; ordering note in "What contradicts or refines"
- Push the overshoot by stretching the rig past normal limits (IK stretch, translation), cheating more than the clean poses allow - 26:38-28:39 - STYLE (stretchy cartoon rig) - maps to P-STRETCH-02
- Scale the fist up on the overshoot frame for impact flavour, if the rig allows - 29:08-29:38 - STYLE - maps to P-STRETCH-02 (nearest)
- Key poses of the punch: idle, anticipation, overshoot, hit, recovery back to idle (idle copied to the end key) - 22:59, 30:14-30:44 - GENERAL - maps to P-GAME-06, P-SETTLE-02
- The rig-level tween/push-pull tools give a 50/50 chance; always hand-finish - 24:13-26:33 - TOOL

## Lesson 04 - Understanding Frame Data for Attacks (about 24 min)
- Timing turns poses into slow/fast/heavy/light feel; timing + spacing + posing = animation - 00:33-00:56 - GENERAL - maps to P-TIME-02
- First retime: spread the key poses (about 2 frames to reach anticipation, then hold on it longer so the wind-up reads), 1-2 frames from overshoot to hit, then the recovery - 01:08-01:49 - GENERAL - maps to P-TIME-03, P-ANTIC-02
- Cutting a few frames between anticipation and overshoot turns a heavy-feeling punch into a medium one; shortening the return to idle (about 20 frames) because it need not take long - 02:36-03:21 - GENERAL - maps to P-TIME-02, P-ANTIC-02
- Frame data vocabulary: startup (from button press to the active window), active (frames that carry the hitbox, here about 3), recovery (the rest, until the character can act again or returns to idle) - 23:51-26:26 - GENERAL - NEW (partition of the clip; see candidates)
- Label the ranges on the timeline (bookmarks: startup, active red, recovery purple); this is the shared contract between animator, programmer and designer; later tweaks stay inside this partition - 24:36-27:14 - GENERAL (the labelled partition) / TOOL (bookmarks) - maps to P-GAME-09 (extends)
- Breakdowns must stop a key pose from landing everywhere at once: lead with hips and face, chest and pelvis ahead, arm held back, elbow leads the fist - 05:16-06:50 - GENERAL - maps to P-OVERLAP-01, P-OVERLAP-02, P-SPACE-04
- Make the anticipation quick by blend-easing into it; fast tween between poses is a start only, then break it up - 04:52-05:16 - STYLE - maps to P-ANTIC-03
- After a retime that feels sluggish, tighten the transition anticipation -> breakdown -> overshoot, keeping the tempo the animator likes - 08:23-08:54 - STYLE - maps to P-TIME-02
- Overshoot frame: slightly more rotation, duck, toe tip, palm scale up; very small movement is enough to keep it alive; the very next frame is the hit - 08:59-09:41 - GENERAL - maps to P-SETTLE-01 / P-OVERLAP-05
- Settle after the hit: a small in-between lowering the body and foot between hit and recovery; no body part should be left "not moving" while the rest works; fist must not stay in one position too long - 10:41-14:06 - GENERAL - maps to P-HOLD-02, P-SETTLE-01, P-LIFE-01
- Recovery: find the midpoint breakdown (around frame 16 of 20) and shift weight onto the new support foot before the final pose; retime until it does not look "cramped" (e.g. 14->13, 16->15) and the arm comes down earlier; ease into idle - 14:44-22:23 - GENERAL - maps to P-SETTLE-02, P-WEIGHT-02
- A hit pose that is static from the front needs rotation/movement too; check front and side - 19:13-19:51 - GENERAL - maps to P-POSE-03, P-HOLD-02
- The whole attack is first blocked (stepped) and only later splined; frame data judged on the blocked version - 22:59-23:18 - GENERAL - maps to P-TIME-03

## Lesson 05 - Light Attacks: Speed & Efficiency (about 21 min)
- Light attacks are the most responsive: the character lunges on the next frame after the press for instant feedback; they have fewer frames than medium or heavy - 00:53-01:33 - GENERAL - maps to P-GAME-05, P-ANTIC-04
- The pose changes on the very next frame; but 1 frame to the hit and back reads as a flicker (like button mashing), so the move still needs a few frames - 01:33-01:46, 04:05-04:42 - GENERAL - maps to P-ANTIC-04; minimum-readable-duration is NEW
- Stretch the keys by one frame each and end the clip around 8 frames; the timing is "significantly shorter" than the medium punch - 04:42-05:23 - GENERAL (concept) / STYLE (value) - maps to P-TIME-02
- Speed is relative change per frame: more change in fewer frames = fast; weight is less change in more frames (tortoise/hare); a light hit gets quick movement in, then holds the hit pose to look strong - 05:42-06:48, 13:42-14:06 - GENERAL - maps to P-SPACE-01, P-SPACE-03, X-08
- "Ease into the hit": quick snappy approach, then hold the active frames so the punch reads before recovery, in about 8 frames; any light attack (kick, swipe) follows this - 12:43-14:06, 20:05-20:35 - GENERAL - maps to P-SPACE-03, P-HOLD-03, P-GAME-07
- Whiplash breakdown (a curled fist rather than flung), overshoot past the contact point, then ease back in with a single in-between for the return - 07:14-08:42, 12:47-13:37 - GENERAL - maps to P-SPACE-04, P-SETTLE-01
- Duck the body, hips and chest working with the punch ("punching from below"), clavicles squeeze and release, head tilts back, toes move; all parts offset so nothing is stiff - 14:24-18:23 - GENERAL - maps to P-OVERLAP-02, P-WEIGHT-06
- More recovery frames = more "drag" (committal, heavier); fewer = a button-mashing, chain-friendly feel; the designer decides, but the ease into active frames must be there - 19:29-20:05 - GENERAL - maps to P-GAME-05, P-TIME-02
- Light attacks = minimal anticipation, more recovery time (the summary line) - 20:35-20:43 - GENERAL - maps to P-ANTIC-02, P-ANTIC-03; recovery-share is NEW
- Check the arcs of the off hand and pull clavicles/elbows with an ease - 14:28-15:48 - GENERAL - maps to P-ARC-01
- Pick up the final spline version in the editor to judge it - 19:04-19:17 - TOOL

## Lesson 06 - Medium Attacks: Balance of Speed & Power (about 28 min)
- Medium = faster than heavy, stronger than light; the "bread and butter" of chains - 00:04-00:32 - STYLE (genre role) - maps to P-TIME-02
- What differs from light: timing (light about 8, possibly about 10 frames; medium gets more) and the style of ease - 00:32-00:50 - GENERAL - maps to P-TIME-02
- Light attacks used "ease in" (compressed spacing at the end, fast approach to the hit); the medium uses the opposite - holds longer before the strike and spaces out afterwards. The instructor's naming of ease-in/ease-out is the reverse of common graph-editor usage; treat the principle (hold first, then commit) not the name - 01:23-02:23 - GENERAL (principle) - maps to P-SPACE-01, P-ANTIC-02
- More anticipation = a heavier attack: medium inserts many more frames of wind-up that communicates intent and builds energy - 02:45-03:10 - GENERAL - maps to P-ANTIC-02
- Break up the C-shaped spine into an angular shape over a single frame inside the anticipation, so the wind-up shows shape change, not only translation - 05:05-05:27 - GENERAL - maps to P-WEIGHT-03, P-POSE-05
- Trick for responsiveness plus weight: change pose right away, then start the ease-out on the second frame and hold the anticipation about 2 frames so the eye catches it - 05:37-06:37 - GENERAL - maps to P-ANTIC-03, P-ANTIC-04 (a compressed-anticipation recipe)
- Keep hands alive in the hold: small finger/knuckle motion rather than a frozen fist - 06:57-08:59 - STYLE (stylised hands) - maps to P-HOLD-02
- Track the fist arc with an arc tool and round it; keep the knee over the toe and roll onto the ball; step into the punch with the front foot - 09:23-10:50, 13:31-14:46 - GENERAL - maps to P-ARC-01, P-WEIGHT-06 / TOOL (arc tracker)
- Breakdown pose is a three-quarter chest, arm brought to a midpoint in front of the body (not a straight left-right path), chin tucked, reverse-C line of action on impact from the front - 11:09-12:18, 14:46-15:41 - GENERAL - maps to P-SPACE-04, P-POSE-02
- Euler filter for flips - 16:04-16:26 - TOOL
- Hold the punch pose for 1-2 frames longer ("12 to 14" key, then reduce by one, and move 12 -> 13), then settle; tune so the hit stays strong and alive - 17:33-19:22 - GENERAL - maps to P-HOLD-03, P-GAME-07
- Reduce overall timing by 2 more frames once it feels long (to 18 frames) - 19:22-19:44 - GENERAL (retime until it is "to the point") - maps to P-TIME-02
- Recovery details: hips not frozen and moving back slightly offset from the chest; chest comes back later/quicker at the end via a longer hold; a small rock-back rebound on the chest (very subtle) gives weight; "weight shakes" - 20:24-23:34 - GENERAL - maps to P-OVERLAP-02, P-SETTLE-01, P-SETTLE-03
- Foot sliding/popping during the step: fix before polishing; end frame must match idle - 19:54-20:49, 24:01-24:18 - GENERAL - maps to P-CONTACT-01, P-GAME-06
- Fix shaky/gimbal arms by adjusting clavicle timing and keeping arms from overlapping - 24:23-27:10 - GENERAL - maps to P-ARC-02, P-LIFE-03
- Outcome: 8 frames -> 18 frames "with more anticipation, more holds, more timing"; almost borderline heavy; the next lesson is heavy attacks - 27:14-27:46 - GENERAL - maps to P-TIME-02

## Numbers given
**Frame rate checked 2026-11-19:** the Maya scene plays at **24 fps** (visible in the playback options on frames `SA05_0500.jpg` and `SA08_0844.jpg`). Course frames × 2.5 = frames at 60 fps.

The instructor never states the working frame rate (Maya, probably 24 or 30 fps; not 60). Counts below are in the
course's own frames. Do not compare with our 60 fps tables without converting. Where the transcript is ambiguous it is marked.

| Lesson mm:ss | What | Value | fps |
|---|---|---|---|
| 04 01:14 | Frames to reach the anticipation pose in the first blocking | about 2 | 24 (checked) |
| 04 01:36 | Frames between overshoot and hit | 1-2 | 24 (checked) |
| 04 02:53 | Anticipation-to-overshoot shortened to turn heavy into medium feel | -3 | 24 (checked) |
| 04 03:12 | Recovery length after shortening | about 20 | 24 (checked) |
| 04 22:59 | Whole demo punch | about 20 frames long, 12 key poses within it (transcript ambiguous) | 24 (checked) |
| 04 24:26 | Startup frames | 4 (counted 1-4/5; ambiguous) | 24 (checked) |
| 04 25:46 | Active frames (hitbox) | 3 (frames 6, 7, 8) | 24 (checked) |
| 04 26:13 | Recovery frames | frame 9 to the end (about 20) | 24 (checked) |
| 04 11:00 | Settle pose after hit | at about frame 9 (ambiguous with active frames) | 24 (checked) |
| 04 15:01 | Recovery breakdown placed at | frame 16 of 20 (midpoint of recovery) | 24 (checked) |
| 04 20:52-21:33 | Retime of recovery keys | 14->13, 16->14/15, 18->16 | 24 (checked) |
| 05 04:42-05:00 | Light attack: key spacing increase | +1 frame between keys | 24 (checked) |
| 05 05:00 | Light attack length | 8 frames total | 24 (checked) |
| 05 13:42 | Fast move-in to hit | 2 frames, then hold on hit | 24 (checked) |
| 05 19:29 | Recovery in the light jab | from frame 6 on (extendable) | 24 (checked) |
| 06 00:37 | Light attack length recap | about 8, possibly about 10 | 24 (checked) |
| 06 06:29 | Anticipation read pose held | about 2 frames | 24 (checked) |
| 06 17:46 | Hit pose hold added | 1-2 frames (12-14 key, minus 1) | 24 (checked) |
| 06 19:30 | Medium timing reduction | -2 frames, to 18 | 24 (checked) |
| 06 27:14 | Light to medium | 8 -> 18 frames (ratio about 2.25:1; about 1.8:1 against 10) | 24 (checked) |
| all | Ratio light:medium:heavy, smear frames, hit-pause frames, VFX frames | not given in lessons 1-6 (heavy is lesson 7) | - |

## Light vs medium attack
Same character and base punch; numbers from lessons 05 and 06.

| Aspect | Light (lesson 05) | Medium (lesson 06) |
|---|---|---|
| Total length | about 8 f (about 10 max) - 05 05:00, 06 00:37 | 18 f - 06 27:14 |
| First visible change | the very next frame - 05 01:38 | pose changes at once, anticipation read held about 2 f - 06 05:37 |
| Anticipation | minimal - 05 20:35 | much more wind-up, spine C to angular - 06 02:51, 05:05 |
| Approach to hit | fast ("ease in"), 2 f then hold - 05 13:42 | holds first, then commits - 06 01:36 |
| Hit hold | hold on active frames to look strong - 05 14:00 | 1-2 f extra hold - 06 17:46 |
| Recovery | drag or short per design; more recovery than anticipation - 05 19:29 | settle with rebound, weight shakes - 06 23:10 |
| Overshoot | past contact, small scale - 05 08:42 | pushed pose with foot step - 06 13:37 |
| Spacing/feel | fast = more change per frame - 05 05:42 | weight = less change in more frames - 06 02:23 |
| Role | instant feedback, button mashing - 05 00:53 | balanced chain move - 06 00:04 |

## Candidate new criteria (NEW)
GENERAL items only. Next free numbers after those in the files (P-GAME-10, P-ANTIC-04, P-TIME-03, P-SETTLE-03 are last).

| Proposed ID | Rule | Evidence (lessons) | Measurable how |
|---|---|---|---|
| P-GAME-11 | An attack clip is partitioned into startup, active and recovery ranges, authored as named ranges/markers on the clip; the active window is short (a few frames) and the hit/extension frame sits inside it. | 04 23:51-27:14; 05 05:06-05:15 | Markers present; counts per range; extension frame (max arm reach) in the active range. Extends P-GAME-09. |
| P-GAME-12 | Strength tiers are separable by frame data: heavier tier = longer startup (anticipation) and/or recovery and longer hold on impact, not only faster or larger motion. The light tier still shows a pose change on the next frame. | 04 02:36-03:21; 05 01:23-01:46, 20:35; 06 00:32-03:10, 27:14 | Startup/active/recovery counts per tier; ratio light:medium; frame of first visible change. Refines P-TIME-02, P-ANTIC-02. |
| P-GAME-13 | Minimum readable duration: a strike that goes out and back within 1-2 frames reads as a flicker; give the hit pose a held window plus a recovery of at least a few frames, even for the lightest move. | 05 04:05-04:42, 13:42-14:09 | Frames the hit pose stays within its tolerance; whole-clip length; user check at game speed (fps-dependent, calibrate in 60 fps terms). |
| P-ANTIC-05 | The anticipation pose opposes the strike pose in its line of action: spine curve, pelvis and chest rotation, and weight go one way, the strike the other; the contrast is large enough to read when the two are flipped. | 03 03:00-03:44, 05:41-05:57, 14:14-15:07; 06 05:05-05:27 | Angle difference of spine/pelvis/chest rotation (anticipation vs impact); flip test in the game camera. Related: P-WEIGHT-03, P-POSE-02. |
| P-SETTLE-04 | After the impact, the recovery contains a designed intermediate breakdown (around the midpoint) and a small rebound or settle of the hit-side parts before the idle hand-off; the body does not move linearly from hit to idle. | 04 10:41-14:06, 14:44-22:23; 06 22:50-23:34 | Breakdown frame near 50% of recovery displaced from the straight line; rebound amplitude on chest/arm; no linear segments. Related: P-SETTLE-01, P-SETTLE-02, P-SPACE-04. |

## Frames worth extracting
| Lesson mm:ss | What it shows | For which ID |
|---|---|---|
| 03 05:41-05:57 | Idle vs anticipation spine/pose side by side | P-ANTIC-05, P-WEIGHT-03 |
| 03 15:04 | Anticipation -> impact pose swap, spine and foot pivot | P-ANTIC-05, P-WEIGHT-06 |
| 03 29:49 | Full pose ladder: idle, anticipation, overshoot, hit | P-SETTLE-01, P-GAME-06 |
| 04 24:36-26:21 | Bookmark ranges startup/active/recovery on the timeline | P-GAME-11 |
| 04 15:01-16:05 | Recovery breakdown pose at the midpoint | P-SETTLE-04, P-SPACE-04 |
| 05 05:00 | Light jab at 8 frames, keys visible | P-GAME-12, P-GAME-13 |
| 05 08:42-10:36 | Light overshoot pose, both arms past contact | P-SETTLE-01 |
| 06 05:05-06:37 | C-shape to angular spine in anticipation | P-ANTIC-05 |
| 06 17:33-19:44 | Punch pose hold added, retime to 18 frames | P-GAME-12, P-HOLD-03 |
| 02 10:42-11:47 | Silhouette view of the idle (black view) | P-POSE-01 |

## What contradicts or refines our criteria
- Refines P-SPACE-03 (strikes accelerate into contact) and P-SETTLE-01: the course puts an overshoot extreme past the contact point
  before the "hit" frame, then pulls back onto it (03 22:59-26:13; 04 08:59-09:41; 05 08:42). Our P-SPACE-03 says the largest step is
  on or just before the active frame; the course's ordering means the extreme and the active frame are about 1-2 frames apart, so
  measure the peak reach at the extreme, not only at the labelled active frame. Agree on acceleration, refine where "contact" sits.
- Refines X-01 (the impact frame is one frame): the course hits in 2 frames then holds the hit pose for several, and the active
  window is about 3 frames (04 25:46; 05 13:42). Fine with P-GAME-07 (freeze the impact pose), but the "one frame" wording should
  state that the peak is one frame inside a held window of a few frames.
- Supports P-ANTIC-02 and P-ANTIC-03: light = tiny anticipation, medium = a lot (05 20:35; 06 02:51-03:10), and "anticipation determines
  how heavy" (03 04:05). Refines the ratio table: the course's light is about 8 f and medium about 18 f total (06 27:14), but
  those ratios are for total length with unknown fps; P-ANTIC-02's 1:1-2:1 cannot be checked since anticipation frames are not stated.
- Possible tension with P-ANTIC-04 and P-GAME-05: lesson 06 05:37-06:37 holds the anticipation about 2 f after changing pose at once; consistent
  (frame 1 already new) and a useful recipe. The light jab changes on the next frame (05 01:38), consistent with P-GAME-05 budget.
- Contradiction in vocabulary only: the course calls the light-attack approach "ease in" and the medium "ease out" (06 01:23-02:04). Common
  graph-editor meaning (and our P-LIFE-02/P-SPACE-01 usage: strikes accelerate in) is the other way round. Do not copy the terms.
- P-TIME-02's stylised range (20-40% shorter durations) is compatible: an 8 f light jab at 60 fps would be 0.13 s, at 24-30 fps 0.27-0.33 s;
  until the fps is known the numbers are not calibrated against P-TIME-02.
- Adds to X-08 (strength is timing): the light attack is made to read heavy by holding the hit pose, not by size (05 05:23-05:37, 13:48-14:06).
- Not covered by any ID: the course expects idle/stance identity per character class (01, 02). Kept as STYLE, not promoted.
