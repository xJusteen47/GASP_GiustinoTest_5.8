# L1 · Weight, balance and contact (`P-WEIGHT`, `P-CONTACT`)

Level 1. Weight is what makes an audience believe a body has mass. It is read from three things: where the centre of
mass is relative to the support, how the body loads before it moves, and how contacts absorb and push.

## Centre of mass (COM)
Standing human: just below the navel, in front of the sacrum; on a game rig near the pelvis, slightly above and forward.
The **base of support** is the polygon of the feet (and hands) in contact.

### P-WEIGHT-01 — Static means balanced
- **Rule:** in any pose held still, the COM projects inside the base of support.
- **Why:** outside the base the body must fall, step or push within a few frames; a still pose outside reads "wrong"
  even to people who cannot say why.
- **Measure:** COM (trunk average is enough) vs the span between the feet along the motion axis (`--kind weight`).
- **Broken looks like:** a held end pose leaning past the toes; a stop pose with all the weight on the heels.
- **Severity:** FAIL on holds ≥ 6 f; outside the base during motion is normal.
- **Figure:** `figures/A-weight-stop.png`.

### P-WEIGHT-02 — Shift weight before lifting a foot
- **Rule:** the pelvis moves over the supporting foot before the other foot leaves the ground (indicatively 2-6 f before).
- **Why:** otherwise the body is hanging from a string.
- **Measure:** frame of foot lift vs frame the COM enters the stance foot's area.
- **Broken looks like:** a step that starts with the foot while the pelvis stays centred.
- **Severity:** FAIL for starts, steps, turns, return-to-idle.

### P-WEIGHT-03 — Load before force (the "down before up" rule)
- **Rule:** to push the body up or forward, the body first lowers or compresses; to stop, it lowers and leans back.
- **Why:** legs push by extending, so they must be bent first. It is also the anticipation the eye uses to read mass.
- **Measure:** pelvis height dips before take-off, starts and big strikes; the lowest point precedes the push by a few frames.
- **Broken looks like:** a jump that rises straight from a standing pose; a run start with no dip.
- **Severity:** FAIL for jumps and run starts (unless the profile wants instant "toy" starts, which then still show
  a 1-3 f compression).

### P-WEIGHT-04 — Absorb on landing and on stopping
- **Rule:** impacts are absorbed: knees and hips bend after contact, the lowest point comes after the contact frame,
  then the body recovers. Heavier = deeper and longer.
- **Measure:** pelvis-height curve: contact frame, then lowest point indicatively 3-5 f later (realistic), then rise.
  `figures/A-contact-jump.png` (mocap) shows the delay; the lifeless rebuild flattens it.
- **Broken looks like:** the body stops dead on contact; or the lowest point is on the contact frame (no absorption).
- **Severity:** FAIL for landings and stops.

### P-WEIGHT-05 — Contrapposto and counter-rotation
- **Rule:** hips and shoulders tilt and twist in opposition; when the pelvis rotates one way, the chest compensates.
  Parallel hip and shoulder lines = mannequin.
- **Measure:** angle between hip line and shoulder line in front and top views on key poses.
- **Severity:** WARN (FAIL on idle and end poses that are held).

### P-WEIGHT-06 — The kinetic chain transfers force from the ground
- **Rule:** power moves start at the feet and travel up: foot -> knee -> hip -> trunk -> shoulder -> arm.
- **Why:** that is where the force comes from; an arm-only strike looks weak.
- **Measure:** order of rotation onsets in the chain (`P-OVERLAP-02` measures the lag).
- **Severity:** FAIL for strong attacks; WARN for light ones.

---

### P-CONTACT-01 — A planted foot does not slide
- **Rule:** while a foot bears weight it stays put (height ~0 AND horizontal speed ~0); roll through heel, ball, toe
  without translating.
- **Why:** sliding feet are the most visible error in game animation.
- **Measure:** `foot_contacts.max_slide` during plants. Free mocap with a heel-to-toe roll: up to ~2.5% of height over a
  whole contact (measured); a sliding plant is typically several times that and continuous.
- **Broken looks like:** skating; a pivot that rotates the whole foot about its centre.
- **Severity:** FAIL.
- **Exceptions:** **designed** slides (skid, slip, ice), which must be marked by the author on the clip (a marker, a
  curve or a notify), so the review does not take them for a bug.
- **Figure:** `figures/A-contact-stop.png`.

### P-CONTACT-02 — Contacts exist
- **Rule:** in locomotion, each foot has distinct plant phases; a clip where feet never reach "height ~0 and speed ~0"
  together has no contacts.
- **Measure:** contacts count > 0 for each foot in every step.
- **Broken looks like:** feet paddling over the floor; measured on the lifeless run rebuild (CMU 09_01): 0 contacts, the
  mocap had 2 per foot.
- **Severity:** FAIL.

### P-CONTACT-03 — Feet re-plant one at a time
- **Rule:** when a pose changes stance (end of a stop, return to idle, after a landing) the feet move one at a time, each
  with lift, travel and plant; never both feet airborne to "teleport" into a new stance.
- **Why:** a person cannot move both feet without jumping.
- **Measure:** frames where both feet are off contact while the body is not in a jump.
- **Severity:** FAIL.

### P-CONTACT-04 — Hands and other contacts obey the same rule
- **Rule:** a hand on a wall, an opponent or the ground is a contact like a foot.
- **Severity:** FAIL when it slides without intent.

## Sources
Williams (weight, walk contact-down-passing-up, weight shift); body-mechanics curricula (Animation Mentor, iAnimate,
AnimSchool) as summarised in sources.md; measurement on CMU 16_08, 16_05, 09_01.
