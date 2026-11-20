# Character Animation from Video Reference (Sarah Doudini, Gnomon), study notes
Private study notes, paraphrased, from the licensed course. Timestamps refer to the lesson video (whisper transcript, may mis-hear).
Rules marked GENERAL transfer to any pipeline; STYLE depend on the course's film/character-acting taste.

## Scope
The course teaches a planning-first workflow for acting and body-mechanics shots in Maya (feature-film/VFX background, a cartoony-to-realistic
biped with a rig using IK/FK switching and a prop cat). Sequence: analyse the shot (scene, character, audio), collect or shoot reference,
act it out and pick takes, build a "super cut" of the best bits, edit/stabilise/retime it, load it as an image plane, block from it (stepped),
add breakdowns ("blocking plus"), go to spline, then polish. Dialogue/lip-sync/eye acting are a large share and mostly do not transfer to
combat or locomotion clips. What transfers to game animation: reference is the starting point, never guessed poses; shoot with the same
fps and a 3/4 camera; shoot the real props and effort; pick key poses and timing from the footage, not frames at regular intervals;
edit the reference into one clean cut; do not copy it, push and cut; add breakdowns in blocking because spline exposes what is missing;
show stepped first and fix timing before spline. What does not transfer: speech subtext, Mehrabian-style body/voice percentages (the 55/38/7
figure is popular psychology and over-generalised, treat as motivation only), lip sync, face-only tracking, director-review politics.
Frame rate: reference is shot at the same fps as the Maya scene; 30 fps (NTSC) is used and 25 for PAL; clips at 29.97 and 24 appear in the
compositing demos. No game fps is discussed (02 tips 01:23 only says shoot faster for fast action).

## Lesson 00 - Preview (0:34 transcript, garbled)
- Promise: professional use of video reference, planning, shooting, editing, blocking to polish. Nothing rule-like. - 00:00 - TOOL

## Lesson 01 - Introduction (3:00)
- Reference is for subtleties that make motion read as real; plan to bring a shot to a believable, empathic performance. - 01:12 - STYLE/NEW (see N-REF-01c)
- Shooting acts as a quick way to explore choices without touching the computer. - 01:36 - GENERAL / NEW (N-REF-03c)
- Pick appealing, clear poses and drop non-essential moments, keep only what counts. - 02:41 - GENERAL / P-POSE-01, P-POSE-06
- Match camera, fps, light, props between reference and software. - 02:02 - GENERAL / NEW (N-REF-04c)

## Lesson 02 - Video Reference Planning (about 17:30 of content)
- Do not guess how a character moves; guessing costs more time than researching and gets overruled by supervisors. - 01:00 - GENERAL / NEW (N-REF-01c)
- You cannot invent convincing fantasy motion before knowing the real one; observe first. - 01:39 - GENERAL / NEW (N-REF-01c)
- Reference is not rotoscoping: copying gives lifeless moments; interpret, find key poses, push them to be more readable and appealing. - 03:18 - GENERAL / P-LIFE-07, P-POSE-06, P-STRETCH-03
- Even mocap is tweaked; a technical copy of reality has no artistic sensibility. - 03:54 - GENERAL / P-LIFE-07
- Plan before posing to avoid the blank page: poses, rhythm, decisions fixed first; budget about 1-2 days of a two-week shot for planning and reference. - 04:56 - GENERAL / NEW (N-REF-02c)
- Reference types: comics (dynamic poses), live-action film (subtle acting), animated films studied frame by frame (squash and stretch), documentaries (animal behaviour), footage of yourself or friends. - 06:32 - GENERAL / TOOL
- Muybridge-style sequences: quadrupeds share walk/trot/run patterns, big difference is spine flexibility; good for cycles. - 07:07 - GENERAL / P-WEIGHT-06 (supports)
- Frame-by-frame web players and reference libraries speed up checking timing without downloads. - 11:44 - TOOL
- Analyse the shot with scene, character, audio: where it sits in the sequence, the high and low point, purpose; character age/body type/baseline mood; audio mood, attitude, subtext, rhythm. - 12:24 - GENERAL / NEW (N-REF-02c)
- Body type changes motion (skinny moves quicker); character baseline decides how an emotion shows, so avoid cliche gestures. - 14:08 (and 03 02:36) - STYLE / P-TIME-02 (supports)
- Follow the rhythm of the source (audio, or for us the move's intended beat) when blocking; feelings and subtext before literal words. - 15:11 - STYLE (dialogue) / NEW (N-REF-06c for rhythm)
- Pre-shot sequencing: new shot must hook up to the previous action and the pace changes (fast then slow). - 01:39 of 03 - GENERAL / P-GAME-06 (supports hand-off poses)

## Lesson 03 - Setting Up With Acting Tips (about 28:00)
- Dedicated reference room or just an empty space and a decent camera is enough; act only what is physically safe. - 00:00 - GENERAL / TOOL
- Source real reference for body-mechanics shots (cat run/stop, scared) and personal reference for acting shots; use comics to push poses. - 00:46 - GENERAL / NEW (N-REF-01c)
- Mimic the animal or character type to find physicality (a "cat-like" woman, bear docs for a bear). - 20:05 - STYLE / NEW (N-REF-05c)
- Identify the operative words (one or two per line) and put the significant action on them; shifting the emphasis changes the meaning. - 07:29 - STYLE / P-TIME-01 (supports contrast), P-HOLD-03
- Vary the rhythm with strong and weak beats, avoid monotony; contrast in poses and in timing. - 06:03 - GENERAL / P-TIME-01
- Do not loop the audio while filming (rushes lines); memorise rhythm, then act, redo takes (second takes are usually better). - 09:17 - STYLE
- Try the crazy opposite (standing vs sitting, still vs gesturing) to check you are not stuck in the predictable option; if no clear idea, do not start animating. - 10:24 - GENERAL / NEW (N-REF-03c)
- Animators are "shy actors": plan every small action and its driver, parts of the body move because of each other. - 11:17 - GENERAL / P-WEIGHT-06, P-OVERLAP-02
- Relax, act as if alone, boring is fine; surprising detail appears in the parts where nothing seems to happen. - 12:36 - GENERAL / NEW (N-REF-03c)
- Constant motion does not make a character alive; reaction to an input does. Overreacting reduces contrast. - 13:39 - GENERAL / P-HOLD-01, P-HOLD-02 (refines)
- Every action has a reason (an external or internal input); emotion leads, the body/face follows. - 14:20 - STYLE
- Between two expressions show the process (e.g. happy, thinking, worried) rather than a pop. - 15:42 - STYLE / P-ANTIC-03 (thought before action, analogous)
- Review your own take with the sound off to check intent reads; ask friends what they read. - 23:12 - GENERAL / NEW (N-REF-07c)
- Match the camera fps to the software scene (30 NTSC, 25 PAL), stage like the animation camera, slightly wider to keep all parts in frame. - 25:12 - GENERAL / NEW (N-REF-04c)
- Use props of similar volume and weight, otherwise the effort is not shown; pantomime of heavy objects is hard (a light toy gun failed as reference). - 26:11 - GENERAL / P-WEIGHT-03, P-WEIGHT-04 (supports)
- Good light and sharp focus on a tripod, blur and shadow lose the reference. - 27:07 - TOOL

## Lesson 04 - Edit Your Reference Part 1 (about 31:20)
- Restate the message to convey before judging takes; look at each take with sound off and ask whether body language delivers it. - 00:00 - STYLE / NEW (N-REF-07c)
- Authenticity check: expression that arrives after the line/action (e.g. eyebrows change late) reads fake; feeling should come first. - 03:05 - GENERAL / P-ANTIC-01 (supports, intent precedes the action)
- Reject a nice movement if it does not serve the message or if it hides the face/key read; being attached to a take is not a reason. - 03:39 - GENERAL / P-POSE-06, P-POSE-01
- Keep an interesting offset between limbs (lead/lag of leg and hand) and consider making a part faster for texture. - 04:51 - GENERAL / P-OVERLAP-01, P-OVERLAP-02
- Take the best part of each take (best first beat from take 4, best last beat from take 2); keep only 3-4 top takes, too many clips confuses. - 01:38 - GENERAL / NEW (N-REF-03c)
- Build a "super cut" of the best bits into one clip with sound, then export both a movie and an image sequence in the same folder. - 01:52 and 19:13 - GENERAL / NEW (N-REF-03c), TOOL
- Reasons to edit reference: raise brightness/contrast, change fps or speed, crop empty space, stabilise a shaky clip, track a subject, trim to the useful part. - 06:34 - GENERAL / NEW (N-REF-04c)
- Flip the reference horizontally to see it with fresh eyes. - 10:08 - GENERAL / NEW (N-REF-07c)
- Match all clips to the same size/fps in the cut; same codec/resolution as the scene; sync to the audio by key words, accuracy is not needed since lip sync is redone in Maya. - 10:48 and 28:33 - TOOL
- Cut stills to keep what you want (e.g. hold the character still until the chosen movement starts). - 29:33 - GENERAL / P-HOLD-03
- Speed up or slow down chosen sections (e.g. 110%) to improve rhythm without leaving the realm of the source. - 28:50 - GENERAL / NEW (N-REF-06c)
- Annotate/draw over reference frames to mark poses; frame-by-frame scrubbing. - 14:22 - TOOL
- Number of padding zeros in image sequence names, H.264 export, JPG sequence, same fps. - 20:17 - TOOL

## Lesson 05 - Edit Your Reference Part 2 (about 37:00, almost all TOOL)
- Image plane: set the frame cache above the clip length for real-time playback; place the plane further back if the character is bigger than it. - 01:20 and 02:13 - TOOL
- Retime reference in Maya by keying the image number on a graph editor curve (linear tangents = 1:1, steeper = faster); less accurate than an editor. - 02:50 - TOOL / NEW (N-REF-06c)
- Offset an image plane's start frame to align an action in the reference with the shot's frame. - 06:46 and 17:00 - TOOL
- Track a face or running animal in Maya by keying plane scale/offset to keep the subject large and centred; locking unused attributes. - 08:10 - TOOL / NEW (N-REF-04c)
- Combine takes with keyed visibility and linked size/offset between planes. - 13:17 - TOOL
- Stabilise shaky footage in a compositor: pick a high-contrast spot, bump contrast before tracking, pattern/search area sized to motion between frames, step manually on motion blur, smooth the track a little, then crop and animate a transform to follow the subject. - 17:23 and 25:33 - TOOL
- Tracking one point on a moving head loses head motion, so follow the whole subject with a smoothed crop instead. - 33:41 - TOOL
- Scene fps in the compositor must match the clip (24 and 29.97 shown). - 19:47 and 30:13 - TOOL

## Lesson 06 - Blocking To Spline Part 1 (about 30:30)
- Blocking sets the ground and pitches the idea with very few key poses: enough to show physicality and main feeling, clear enough to get approval, feedback early so no time is wasted. - 00:47 - GENERAL / P-TIME-03, P-POSE-01
- Block in stepped mode: you can craft each key pose and make it clear and appealing without interpolation hiding it. - 03:17 - GENERAL / P-TIME-03
- Aim for exaggerated realism: grounded in reality (or the audience will not relate) but pushed, like a realistic vs cartoony bouncing ball, which has squash and stretch in both. - 03:40 - GENERAL / P-STRETCH-03
- Copying the exact timing from realistic reference tends to look slower and softer than wanted; the reference timing needs to be tightened. - 04:24 - GENERAL / NEW (N-REF-06c), P-TIME-01 (supports)
- Do not chase perfect timing and spacing in the first blocking; that is the next phase. - 04:47 - GENERAL / P-TIME-03
- Choose main poses at the key moments of change that make obvious what the character is thinking/doing, and the "sparks" in the reference (a pose, a particular movement that carries personality). - 05:27 - GENERAL / P-POSE-01, P-POSE-06, NEW (N-REF-05c)
- Align the reference start frame with the first key (offset 13 frames), then pose "by contact": first key is the contact pose, then the next contact. - 07:29 - GENERAL / P-CONTACT-02, NEW (N-REF-05c)
- Get the body mechanics right with the main body first (root, then limbs), add props (the cat) afterward. - 09:34 - GENERAL / NEW (N-REF-08c)
- Read weight in the reference: where it goes (shoulder/hand keep the balance, knee takes the weight, a small anticipation then down). - 13:13 and 15:06 - GENERAL / P-WEIGHT-01, P-WEIGHT-03, P-ANTIC-01
- Still stretches in the reference are where the face carries the thought; keep them still so the audience looks at it. - 15:49 - STYLE / P-HOLD-02 (refines)
- Change the facial expression before a big movement, otherwise it is lost during the action. - 17:49 - STYLE / P-ANTIC-01 (supports)
- Isolate the movement: head still while the arm lifts, the wrist stays, then the head turns last into the final expression; shoulder leads, then chest, then hand while the head stays rigid for tension. - 18:02 and 19:23 - GENERAL / P-OVERLAP-01, P-OVERLAP-02
- A very exaggerated action in the reference can be pushed further in the animation. - 18:21 - GENERAL / P-STRETCH-03
- Another reference angle shows what the first hides (the cat's legs reaching the floor); animal behaviour is not reproducible by acting. - 26:12 - GENERAL / NEW (N-REF-04c)
- Locked elbows and fingers for long contacts prevent popping. - 29:37 - GENERAL / P-CONTACT-04
- Practical: split the viewport (persp, camera, graph editor, reference), colour the character to read the shape, selection set to key the whole body. - 01:52 and 14:10 - TOOL

## Lesson 07 - Blocking To Spline Part 2 (about 40:00)
- Blocking plus: now focus on timing, spacing and physicality; add breakdowns for anticipation, overlap and the moving action, otherwise the information is lost in spline. - 00:19 - GENERAL / P-TIME-03, P-ANTIC-01, P-OVERLAP-01
- Before adding keys, list rig issues: IK/FK switches (hand in IK while planted, in FK when lifted), locked knees and elbows to avoid popping, knee intersecting the ground. - 00:55 - GENERAL / P-CONTACT-01, P-CONTACT-04
- Read the reference piece by piece and add a breakdown at each weight event (weight on knee, hip down, settle as a moving hold). - 05:07 - GENERAL / P-WEIGHT-03, P-HOLD-02
- A slow movement in the reference needs more keys to show it. - 14:33 - GENERAL / P-SPACE-01 (supports)
- Add a breakdown for anticipation before a big move, keep the support point locked near the same spot. - 14:57 - GENERAL / P-ANTIC-01, P-CONTACT-01
- Break a complex move into steps from the reference: anticipation of the hand, push down with the hip, small change first and bigger later, shift weight to raise the hand, switch FK/IK at the contact change. - 19:16 - GENERAL / P-WEIGHT-06, P-SPACE-03 (supports)
- Switch contact modes (IK/FK, space follow) on a single frame, never blended, to avoid popping. - 35:04 - GENERAL / P-CONTACT-04, P-SPACE-06
- Make sure arcs are right and clean skin intersections and popping in blocking plus; improve arm/chest shapes for appeal. - 22:58 and 23:30 - GENERAL / P-ARC-01, P-POSE-01
- Blocking plus must carry enough keys: viewing in stepped hides the true timing, since the brain smooths the in-between; going to spline without enough information gives a bad surprise. - 26:07 - GENERAL / P-TIME-03 (refines), P-SPACE-04
- Get approval on blocking before spline; name scene files clearly so earlier blocking or rejected ideas can be recovered. - 26:53 and 27:44 - TOOL
- Spline is the hardest part: advance one movement at a time, one chunk of the shot at a time; the spline view exposes what is missing. - 28:15 - GENERAL / P-MEASURE-01 (supports)
- Last opportunity to add breakdowns is the start of splining; later they are harder to add. - 29:11 - GENERAL / NEW (N-REF-09c)
- Put the reference aside during spline, return to it only for reminders on body mechanics or key poses. - 29:26 - GENERAL / NEW (N-REF-09c)
- Ask how to improve on the reference: add overlap and offsets between parts, push poses, play with timing, more subtle anticipation, more contrast, correct arcs, snappier where it needs, longer on the emotion. - 29:41 - GENERAL / NEW (N-REF-06c), P-OVERLAP-01, P-TIME-01
- Typical spline failures: eyes/hands popping where contacts or follows switch, intersections, spline-y inner eyebrow arcs; fix one issue at a time in the graph editor, clean curves, keep switches to one frame. - 31:24 - GENERAL / P-SPACE-06, P-LIFE-02
- Keep a longer pose where the character needs time to think; offset body and leg a bit more, retime a little for snap. - 32:22 - GENERAL / P-HOLD-03
- Squash and stretch the face/pose for emphasis on an accent and add offsets (jaw lagging when the head moves back). - 33:47 - STYLE / P-STRETCH-02, P-OVERLAP-02
- Polish: fingers, overlap, ears, hair, delays for the jaw, extra noise removed from curves, secondary motion for props; the polish is endless, so find a compromise on what is good enough because most of the audience never sees the small details. - 38:48 - GENERAL / P-OVERLAP-03, P-OVERLAP-04, P-LIFE-07

## Lesson 08 - Tips And Tricks (4:30)
- Several cameras (or a mirror) give more perspective on body parts in 3D space; avoid orthographic/straight-on only, a three-quarter angle keeps depth. - 00:05 - GENERAL / NEW (N-REF-04c)
- If the shot will be moved in the animation camera, shoot with the camera locked off so it is clear what moves; fast actions are shot at a higher frame rate to avoid motion blur. - 01:02 - GENERAL / NEW (N-REF-04c)
- Give the character a thought process; allow time to think between actions, a thought guides the next action. - 01:31 - GENERAL / NEW (N-REF-03c), P-HOLD-02
- Act in the character's mindset: remember why it moves; keep the action simple first, details later. - 02:31 - GENERAL / NEW (N-REF-03c)
- Do not copy reference: take poses and timing ideas from it, then translate. - 03:02 - GENERAL / P-LIFE-07
- Eyes carry much of the acting; when acting a dialogue, place a physical target to look at. - 03:25 - STYLE
- Staging matters; take on the scary part of the shot. - 03:49 - STYLE

## Reference -> animation method
1. Plan first: write the intent in one line (what the move/shot must say), the context (what comes before and after, so start and end poses hand off), the character baseline, and the rhythm (strong and weak beats). Time-box it (1-2 days in a two-week shot). - 02 12:24
2. Find reference: real footage for body mechanics (animals, athletes, props with real weight), acted footage for performance, comics and film for push. Never guess movement. - 02 01:00, 03 00:46
3. Shoot: fps equal to the animation scene (30 or 25 in the course; ours would be the project rate), 3/4 camera, locked off if the move will not travel, higher fps for fast actions, real props of similar weight, good light, whole body in frame, second angle or mirror. - 03 25:12, 08 00:05
4. Several takes, including a deliberate opposite; do not loop the audio or timing aid so as not to rush. - 03 10:24
5. Judge takes by intent: watch with sound off, flip the image, reject any take where the expression/anticipation lags the action or the nice movement hides the read. Keep 3-4 best takes. - 04 00:00, 04 03:05
6. Build a "super cut": best part of each take in one clip, sized and fps-matched, stable, subject centred and cropped; retime sections you want to tighten. Export movie and image sequence. - 04 06:34, 05 02:50
7. Mark the key poses on the clip: contacts, extremes of weight shift, the pose where the thought changes, the "sparks" that carry character. Offset the reference so the first key sits on the scene's frame 1. - 06 05:27, 06 07:29
8. Block stepped from those poses, body first (root, spine, limbs), props afterwards; do not chase spacing yet; push the poses and cut the rest; keep only reference moments that serve the intent. - 06 03:17, 06 09:34
9. Blocking plus: add a breakdown at every weight event, anticipation, overlap and settle, with contacts locked and mode switches on one frame; check the other reference angle; tighten the reference timing (realistic timing reads slow and soft); retime what needs snap or more time. - 07 00:19, 06 04:24
10. Review in stepped and in a rough spline preview before committing; stepped hides true timing, so add keys until the move survives spline. - 07 26:07
11. Spline chunk by chunk with the reference off to the side; fix pops at switches, intersections, eyes/hands, arcs; add what the reference lacked (overlap, offsets, contrast, exaggeration). - 07 28:51, 07 29:41
12. Polish within the deadline: secondary parts, noise removal, then stop; the audience will not see the smallest details. - 07 40:20

Keep from the reference: weight transfers, contacts, order and offsets of body parts, the timing relationship between parts, small honest nuances (a hold to think, a late hand), the places the face or key shape carries the action.
Push or cut: constant fidgeting, soft or slow timing, any movement that hides the main read (see 04 03:39), anything that does not serve the intent, regular spacing between poses. Exaggerate arcs, anticipation and contrasts, but stay grounded.

## Candidate new criteria (NEW)
| Proposed ID | Rule | Evidence (lessons) | Measurable how |
|---|---|---|---|
| N-REF-01c (P-GAME-15c) | A move that mimics real motion starts from reference (footage or acted take), never from invented poses | 02 01:00, 02 01:39, 03 00:46 | Move card lists a reference clip/ID and which poses/timings came from it |
| N-REF-02c | A reference plan precedes posing: intent line, context, baseline, rhythm | 02 12:24, 02 04:56 | Plan fields exist (what, before/after, beats) before the first key |
| N-REF-03c | Reference is edited into one clean cut of the best takes (3-4 sources), including one opposite option explored | 03 10:24, 04 01:38 | Number of takes considered, one super cut file |
| N-REF-04c | Reference matches the target scene: same fps, camera angle with depth (3/4, not orthographic), whole body in frame, props of real weight, stable and sharp; fast moves shot at higher fps | 03 25:12, 04 06:34, 08 00:05 | Reference metadata: fps, angle, resolution, motion blur level |
| N-REF-05c | Key poses are picked at contacts, weight extremes and moments of change, not at regular frame intervals | 06 05:27, 06 07:29 | Key frame list vs contact/extreme events detected in the clip; spacing between keys is irregular |
| N-REF-06c | Realistic reference timing is tightened or varied for the game (slower/softer by default), with a recorded retime of sections | 06 04:24, 07 29:41, 05 02:50 | Frame count of each phase vs reference (ratio per phase, not a single global ratio) |
| N-REF-07c | Reference and result are reviewed as silhouette/motion with fresh eyes (flipped, sound off, other viewer) | 03 23:12, 04 10:08 | Review log includes flipped check and a second-viewer note |
| N-REF-08c | Build body mechanics first (root/spine/limbs), then props and details | 06 09:34 | Order of keying in the history/notes |
| N-REF-09c | Add all breakdowns before spline; once in spline, reference only for reminders | 07 29:11, 07 29:26 | Count of breakdowns before spline vs after; spline pass without new poses |

## Numbers given
| Number | Context | Lesson |
|---|---|---|
| 30 fps / 25 fps | shoot with the Maya fps: NTSC 30, PAL 25 | 03 25:12 |
| 29.97, 24 | fps seen in clips for the compositing demos | 04 12:39, 05 19:51 |
| 7/38/55 percent | Mehrabian words/voice/body split (over-generalised, motivation only) | 03 03:45 |
| 1-2 days | planning/reference of a two-week shot | 02 05:26 |
| 3-4 takes | keep at most this many best takes | 04 01:38 |
| 110 percent | example speed-up of a section | 04 28:50 |
| 13 frames | offset reference start to align the first pose | 06 07:39 |
| 1 frame | switch IK/FK or follow on a single frame | 07 35:04 |
| 1-2 | operative words (emphasis points) per line | 03 08:27 |

## Frames worth extracting
| lesson mm:ss | what it shows | for which ID |
|---|---|---|
| 04 02:12 | take played in the viewer with the chosen movement (genuine thinking take) | N-REF-07c, P-POSE-06 |
| 04 03:18 | a take with late-coming eyebrows (fake expression, frame 173 note) | P-ANTIC-01 |
| 06 07:59 | first key pose on the character over the image plane | N-REF-05c |
| 06 19:38 | shoulder, chest, hand lead with head held | P-OVERLAP-02 |
| 07 06:06 | breakdown poses for weight onto the knee and hip | P-WEIGHT-03, P-HOLD-02 |
| 07 22:35 | blocking-plus playblast frames compared to the reference | N-REF-05c, N-REF-06c |
| 07 31:24 | the spline view where eyes and hands break | P-SPACE-06 |
| 05 22:36 | tracker with pattern and search area (TOOL reference only) | N-REF-04c |

## What contradicts or refines our criteria
- P-HOLD-01 (no dead frames) and the course's "constant motion does not make life": refinement, a character may be still while the thought changes (03 13:39, 06 15:49) as long as it is a chosen moving hold (P-HOLD-02). Our game clips still need life in holds.
- P-TIME-03 (judge timing in stepped first): refined; stepped hides the true timing, so check a rough spline preview before leaving blocking plus (07 26:07).
- P-ANTIC-03 (compress, do not delete): consistent; the course adds that the face/idea change precedes the big action (06 17:49) and that realistic timing reads soft, so tighten (06 04:24). For games the tightening should be larger.
- P-LIFE-07 (clean but empty): supported; a straight copy of reference gives "lifeless moments"; reference is a source, not the target.
- STYLE caveats: acting rules about speech subtext, eyes, lip sync, and film-director approvals are not game criteria. A shot is judged against a story beat; a move against responsiveness (P-GAME-05), which the course does not consider.
- Nothing in the course covers loops, root motion, hitstop or frame data; reference there needs a game-specific layer.
