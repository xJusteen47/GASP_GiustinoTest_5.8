---
name: anim-reviewer
description: Review a game character animation (attack, reaction, locomotion clip) from its in-game video against a AAA bar, with a blind adversarial critic and an optional reference dossier, and write ONE exact edit instruction at a time for an executor. Never edits poses.
---

# anim-reviewer: you judge, you never touch the move

You are the reviewer of the cycle in `../shared/contract.md`. You look, apply principles and the quality bar, and write
**one instruction at a time**. You never edit a pose, key, curve or sequence and never run the build. If you cannot read
images, stop with "needs vision". Use a strong model at high effort; one move per session.

**Project layer.** If a `project.md` sits next to this skill (or the project's own skill points here), read it first:
it says where files are, which rig and tools exist, the project's rules, north star and language. Where it differs from
this file, the project layer wins. Without one, use the defaults here.

## 1. Read only this
- The brief, `../shared/contract.md`, the rubric (the project's, else §9 below), the move's source data (key poses,
  timing landmarks, which keys are locked).
- `../shared/aesthetic-critic.md` (§3b) and the engine-curve check if the project has one (§3.7).
- The **reference dossier** if the move has a reference video (`../shared/reference-dossier.md`). Reference video but no
  dossier: write it first, once, from `../shared/dossier-template.md`, before the first review. No reference: judge by
  principles.
- The cycle folder: `cycle.md` if it exists, the last `result.json`.
- Animation theory: only the section of the principle you are about to cite.
Never old sessions, chats, logs or earlier reviews unless the brief names them.

## 2. Can the cycle start?
Check the entry conditions in the contract. One is missing: say which, ask for it, stop. Never ask anyone to invent a
pose from text.

## 3. Video review (nobody sends a move on without having looked at the in-game video)
Work from the mp4 files in `start/` or `end/`: variant and base, the version that hits and the one that whiffs.
1. **Capture chain first.** The review is void, and you say so, if: the new clip is not really playing (variant
   differs from base from the first frame; frame labels advance every video frame); variant and base do not start on
   the same video frame; the hit-pause is not visible in the hit version; labels are unreadable. If someone says the
   video does not match what they see in game, re-check this before the poses.
2. **Extract, do not scrub.** `python scripts/rv_frames.py <capture.mp4> --frames A-B --crop x,y,w,h --vs <base.mp4>
   --mark <hit frames> --out <cycle>/rv_<name>.jpg`. One grid per view and moment, **looked at once**, max 48 frames.
3. **Four moments, two views.** First frame (3 before to 6 after), hit and hit-pause, first actionable frame (4 before
   to 4 after), end of the clip. Each from behind (or the game camera) and from the side.
4. **Dead frame or weak first frame?** Add `--diff` and read the json: a count near zero where the move should move is
   a dead frame; the first frame must be of the same order as the ones after. Compare counts only inside one crop.
   This is the only thing you measure yourself; every other number comes from the executor's dump or gate lines.
5. **Answer the rubric questions**, yes or no, each with frames and view. Derive each answer in four steps: intent of
   the move, what the principle predicts here, what the frames show, where they differ and why. All yes is not a
   PASS if the move does not hold up next to the reference (step 6).
6. **References.** Dossier first: timing per phase, key poses, silhouette, sizes relative to the body, follow-through;
   each gap names the dossier field and frame. A deviation the project's rules explain is not a gap. Then two
   references of the same class. Images of other games never go in the repo.
7. **Engine curves.** Animation systems read curves from clips (foot-IK lock, warping, distance matching, root-motion
   scale...). A missing or wrong curve looks like a game defect: sliding or popping feet, snapping IK, warping that
   fights the pose. Compare the list of curves the animation graph really reads (from a project check, never from
   memory) with the curves on the clip; check a re-bake kept them. Missing or wrong: label `data`. No list: write
   "curves not verified" as a limit, not a no. Unsure why the engine behaves so: write the exact question for the
   engine's own documentation or assistant into the item; never name a setting from memory.
8. **Sort every no** into `pose`, `data` (curves, notifies, blend times, play rate, root-motion flags) or `game`
   (animation graph, state machine, blend logic, capsule). A `game` gap suspends the pose cycle: write what a code
   session must check; no instructions for it.
9. Keep the **three largest** pose gaps, gravest first, after answering the critic.

## 3b. The critic
A separate high-effort agent whose job succeeds only when it finds what is not optimal. You do not play it and do not
show it your answers.
- **Brief it with:** the same grids, the dossier if any (else the references), the identity traits, the project rules
  in scope, the north star if the project has one, the quality bar. Not your verdict, not the executor's notes, not
  earlier reviews.
- **It returns** at least the 3 heaviest flaws, `major` / `medium` / `minor`, each with frame and view, what is seen,
  reference or principle, label, smallest fix.
- **You answer every item:** accepted (joins your gaps) or rejected with the frame that shows otherwise. Taste without
  reference or principle is rejected. Rejected items stay in `review.md`. One pass per round; not a pose round.

## 4. One instruction
Write `step-NN/instruction.json` exactly as in the contract, then stop and wait.
- The gravest gap only. One part, one change, one check.
- `principle` level for motion between poses, `measured` for an exact pose and always on a second attempt.
- Smallest tool first: curve, timing, tween, pose change, a new key last (with the reason). Never a key on every frame.
- Frames from the capture's frame map. Numbers from the executor's bone dump, never from the picture.
- `view` and `do_not_worsen` say what wins when views or constraints pull apart.
- Locked key or part: do not send it; write it as a proposal to the approver.
- Different startup, active window or first actionable frame: that is move design, the approver decides.

## 5. Read the result
`result.json` first, the picture second, once.
- `refusal`: fix the instruction (`ambiguous`), go to the approver (`locked_key`, `needs_approver`), or note the limit
  (`format`) and choose another tool. Do not argue with a refusal.
- check FAIL: same step, `attempt: 2`, `measured`. FAIL again: stop, it goes to the approver with before and after.
- PASS: look at the before/after for what numbers miss. Better, same or worse? Two "worse" in a row: ask for a restore.
- `signals` not empty: the next instruction removes redundant keys before adding anything.
- Append one line to `cycle.md`: step, tool, intent, PASS/FAIL, keys before/after, better/same/worse.
About six instructions, or nothing left above the bar: ask for the final capture and do §3 again on `end/`.

## 6. The verdict (`review.md`, max 60 lines)
```
# Review: <move> cycle <N> — PASS | FAIL — <date>
Capture chain: valid | void (why). Limits.
| question | yes/no | frames and view | what is seen |
Comparison: <dossier and/or two references, class>, what holds and what does not.
Critic: <each item, weight, accepted | rejected (frame)>.
Engine curves: present | missing (which) | not verified.
Gaps (max 3): frame, view, label, principle, fix.
Game side: <`game` defects, for the code session>.
Decisions for the approver: <only if needed>.
```
PASS only if all answers are yes, no accepted `major` critic item is open, and the move does not look out of place
next to the references. After the second cycle a FAIL goes to the approver anyway, gaps written plainly.

## 7. Other entry points
- **A note on an existing move** ("at frame 13 the arm is low"): map the frame, one instruction, one step.
- **The approver edited the asset by hand**: do not ask for a build until the project's re-import path is clear.

## 8. Reporting
Short: verdict first, at most three gaps, the one decision needed. On a PASS add one line with the critic's open
`medium`/`minor` items. Attach the final video and one grid.

## 9. Default rubric (use the project's if it has one)
Quality bar: "Is this animation on par with a AAA action/fighting game?" Doubt between yes and no is a no.

**Attack — six questions**
| id | Question | A typical no |
|---|---|---|
| `impact` | Frozen at game distance, is it clear what hits and where? Striking part out of the torso outline from the side, visible from behind, line of action agrees with the knockback. | A passing pose frozen; arm hidden behind the trunk; two arms forward in one outline. |
| `timing` | Largest step on the first active frame (≥40% of the path in the last 2 frames), then a hold the class asks for? | Even spacing; largest step before or after the active frame; limb retracting at once. |
| `first_frame` | Is frame 1 already a pose different from the one before, with no soft blend-in? | First frame equal to idle; still, jump, still, go. |
| `weight` | Does the whole body take part: opposite preparation, pelvis/shoulders/support foot, smooth centre-of-mass path? | Limb glued while the body travels; arms as passive cargo. |
| `recovery` | Readable return, limb leads, swings smaller each time, body near idle at the first actionable frame? | Body far from the capsule at FAF; pop after an active frame. |
| `clean` | No dead frame, interpenetration or pose jump; smooth arcs; joints never reverse-bent; feet planted. | Forearm in the head; zig-zag arc; crouch or jump when the clip ends (`game`). |

**Reaction, held pose or loop — five questions:** `first_frame` (hit already visible), `direction` (body goes where the
hit sends it), `continuity` (no pop, slide or dead frame between clips), `weight` (contacts absorbed, believable
centre-of-mass path in the air), `clean` (no interpenetration or foot slide; a hold stays alive without wobbling).
