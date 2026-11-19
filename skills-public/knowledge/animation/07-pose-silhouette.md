# L1 · Pose, silhouette, stretch (`P-POSE`, `P-STRETCH`)

Level 1. Key poses carry the story; their quality is judged frozen, at the size and angle the player sees them.

### P-POSE-01 — The key pose reads in silhouette
- **Rule:** filled in black, the key pose still tells what the character is doing.
- **Measure:** silhouette of key frames from each view that the game shows; the acting part sticks out of the torso outline.
- **Broken looks like:** the striking arm in front of the chest from the camera's view; both arms in one blob.
- **Severity:** FAIL on impact and end poses.

### P-POSE-02 — Line of action
- **Rule:** a strong pose has one clear line (C or S) running through the body in the direction of the force.
- **Measure:** draw the line from the supporting foot through spine to head/striking hand; it is continuous and agrees
  with the action's direction.
- **Broken looks like:** a straight, vertical trunk on a strong action; a line that bends against the force.
- **Severity:** WARN (FAIL on impact poses of strong actions).

### P-POSE-03 — Poses read from every view the game shows
- **Rule:** a move is judged from the views the player has (game camera, often behind) and from the side; a pose that
  works only from one view is incomplete.
- **Measure:** frozen frames from each required view.
- **Severity:** FAIL when the action vanishes in a required view.

### P-POSE-04 — No twinning
- **Rule:** left and right sides are not mirrored in pose or in timing.
- **Measure:** compare paired joints; identical angles or simultaneous peaks.
- **Severity:** WARN (FAIL on idle and end poses).

### P-POSE-05 — Clear pose changes
- **Rule:** consecutive key poses differ enough to read; a pose change smaller than the noise of the clip is not a key.
- **Severity:** WARN.

### P-POSE-06 — Appeal and intent
- **Rule:** the pose shows intent (what the character wants) and personality; asymmetry, clear shapes, contrast between
  straight and curved lines.
- **Severity:** judgement; cite a reference.

---

### P-STRETCH-01 — Volume is preserved
- **Rule:** when a body part squashes, it widens; when it stretches, it thins. Rigs without scale can suggest it with
  pose (compression of the spine, extension of the limbs).
- **Severity:** WARN.

### P-STRETCH-02 — Stretch on speed, squash on impact
- **Rule:** stretch along the motion on the fastest frames; squash on contacts and impacts; then recover.
- **Measure:** the extreme of the stretch is on the fastest frame, the squash on or just after contact.
- **Severity:** WARN; the amount belongs to the project profile (level 2).

### P-STRETCH-03 — Exaggeration is consistent
- **Rule:** exaggeration pushes what is already true (timing contrast, pose, spacing); its amount is a style choice and is
  consistent within one character.
- **Severity:** WARN; the scale is set in level 2.

## Sources
Thomas & Johnston (staging, solid drawing, squash and stretch, exaggeration, appeal); Williams; game-animation practice
on readability at camera distance (Cooper).
