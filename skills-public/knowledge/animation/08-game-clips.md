# L1 · Game clips: root motion, loops, transitions, responsiveness (`P-GAME`)

Level 1, specific to real-time games but not to any engine or game. A game clip is not a film shot: it is played,
blended, warped, cut and repeated by systems. It must be good **alone** (isolation) and good **in the system** (in game).

### P-GAME-01 — Isolation first
- **Rule:** every clip passes an isolation check before it is tried in the game: foot slide, loop seam, root motion,
  pops, curves/markers, first and last pose, life metrics (`P-MEASURE-02`).
- **Why:** a clip that fails alone never passes in the setup, and it can mask or cause errors in other clips.
- **Severity:** FAIL (process): no in-game review of a clip without its isolation report.

### P-GAME-02 — Root motion matches the body
- **Rule:** the root (the bone or channel that carries the character's displacement) moves the way the body's weight
  moves: smooth, without lag or lead against the pelvis, without vertical bob unless the gameplay wants it.
- **Why:** the game moves the capsule from the root (or from its own physics); a root that disagrees with the body
  produces slide even when the feet are perfect in the clip.
- **Measure:** pelvis horizontal position relative to the root over time: bounded, no drift; root speed curve continuous;
  feet still in world space during plants when the root drives the character.
- **Broken looks like:** a stop clip whose root stops before the body (the body slides forward over the planted feet);
  a run start whose root accelerates linearly while the body accelerates in steps.
- **Severity:** FAIL.

### P-GAME-03 — Root motion vs in-place with displacement
- **Rule:** decide per clip whether the clip drives displacement (root motion) or the game does and the clip follows
  (in-place, speed-matched by play rate, stride warping or distance matching). Never both; document which.
- **Measure:** the clip's metadata says which; in game, foot slide is checked at the speeds the system will use.
- **Severity:** FAIL when undocumented and sliding.

### P-GAME-04 — Loops close and keep life
- **Rule:** in a loop, the last frame flows into the first with continuous position **and velocity**; secondary motion
  is phase-continuous across the seam.
- **Measure:** pose and speed difference between last→first equals that of neighbouring frames; no velocity spike.
- **Broken looks like:** a hitch once per cycle; a hand that resets.
- **Severity:** FAIL.

### P-GAME-05 — Responsiveness budget
- **Rule:** an input-driven action shows a clear change within the game's response budget (often 1-3 frames at 60 fps
  for attacks and jumps); anticipation is compressed or moved into readable early frames (`P-ANTIC-03/04`).
- **Measure:** frames from input to first visible change of the leading part.
- **Severity:** FAIL when the clip delays the read beyond the budget the project sets.

### P-GAME-06 — Start and end poses are hand-off poses
- **Rule:** the first frame matches what the system blends from (or is designed to be cut into); the last frame is a pose
  the next state can take over (idle, run, the next combo). Hand-offs re-plant feet one at a time (`P-CONTACT-03`).
- **Measure:** difference between this clip's end and the next clip's start; foot positions through the blend.
- **Severity:** FAIL when the blend pops or both feet move.

### P-GAME-07 — Hit-pause (hitstop) is a designed hold
- **Rule:** on a connected hit both attacker and target may freeze for a few frames; the pose that freezes must be the
  impact pose (or very near it), and the motion resumes with the follow-through, not from a dead stop.
- **Measure:** the frozen frame is the frame of maximum extension or contact; the first frame after the pause continues
  the arc.
- **Severity:** FAIL if the frozen pose is a passing pose.

### P-GAME-08 — Readable at gameplay distance and camera
- **Rule:** the action reads at the size and angle the player sees it (often from behind and small on screen).
- **Measure:** review captures from the game camera, not only from a side turntable (`P-POSE-03`).
- **Severity:** FAIL when the action vanishes from the game camera.

### P-GAME-09 — The data the engine reads is on the clip
- **Rule:** curves, markers and notifies the animation system needs (foot contact/lock, distance, speed, sync markers,
  blend windows, designer markers for intended slides) are present and correct on every clip that enters the system.
- **Measure:** compare the list the graph actually reads (from the project, never from memory) with the clip's data.
- **Severity:** FAIL ("data") when missing; it looks like an animation defect in game.

### P-GAME-10 — Systems adapt clips; clips do not carry per-clip hacks
- **Rule:** prefer data and systems (warping, IK, matching) that make any clip work, over constants tuned for one clip.
- **Why:** a setup that needs per-clip numbers cannot accept a new animation set.
- **Severity:** WARN (architecture).

### P-GAME-11 — Helper nodes are clean on export
- **Rule:** placement and helper controls the engine does not read (master/offset/temporary controls) are at identity on
  the exported clip, and the baked result is the same pose frame by frame; the exported root carries only what the clip
  documents (`P-GAME-03`).
- **Measure:** export a test bake, compare joint world positions with the authoring scene; helpers' channels all zero.
- **Severity:** FAIL.

### P-GAME-12 — A displacement root rotates only in heading
- **Rule:** when the root carries displacement, its rotation is limited to heading (yaw); flips, rolls and tilts live on
  the pelvis, so the engine can extract travel and turning cleanly.
- **Measure:** root pitch/roll within a couple of degrees on clips where only the body tilts.
- **Severity:** FAIL on root-motion clips.

### P-GAME-13 — Attack clips carry their frame data
- **Rule:** an attack is partitioned into startup, active and recovery ranges authored as named markers/notifies on the
  clip; the contact/extension frame lies inside the active range, which is short (a few frames).
- **Measure:** markers present; frame counts per range; frame of maximum extension inside the active range.
- **Severity:** FAIL ("data") when missing.

### P-GAME-14 — Strength tiers are separable by timing
- **Rule:** heavier tiers get longer startup (anticipation), longer holds on impact and longer recovery, bigger weight
  shift and arcs, not only bigger or faster motion; the lightest tier still changes pose on the next frame after input.
- **Measure:** startup/active/recovery counts and hold lengths compared across tiers of the same character.
- **Severity:** WARN (FAIL when two tiers are indistinguishable in timing).

## Sources
Cooper, *Game Anim: Video Game Animation Explained*; public engine documentation on root motion, motion matching and
warping (concepts only); practice summarised in sources.md.
