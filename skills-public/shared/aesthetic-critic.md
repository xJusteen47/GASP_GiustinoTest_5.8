# The aesthetic critic (animation and VFX reviews)

Next to the judge who gives PASS/FAIL, every aesthetic review has a **critic** whose job succeeds only when it **finds
what is not optimal**. Scope: every video review of an animation or a visual effect. It does not replace the judge: it
tests the judge. The skills that apply it are `anim-reviewer` (§3b) and `vfx-reviewer` (§3b); this file is the principle.

## Why
A judge who must say PASS or FAIL tends to see what it expects and to stop at "good enough". A role that wins only by
finding flaws looks where the judge does not. Together they raise the bar without the art director having to be the critic.

## Principles (not recipes)
1. **Separate and not anchored.** The critic is a separate agent, ideally the strongest model available at high effort.
   It sees the video, the grids, the references (the reference dossier when there is one, `reference-dossier.md`), the
   project's rules in scope and the identity traits of the move or effect. It does **not** see the judge's verdict,
   the builder's notes or earlier reviews: it must look with its own eyes.
2. **Success = real flaws.** It always lists **at least the 3 heaviest flaws**, ranked `major` / `medium` / `minor`. If
   it truly finds fewer than three, it says so and says where it looked. Inventing a flaw is not a success: each one
   must survive the judge's check.
3. **Every flaw is visible.** Frame and view, what is seen, the reference or principle it betrays, the label (the
   skill's fail labels), the smallest fix. Taste alone is not enough: it needs a reference or a principle.
4. **Weight is judged at real speed.** `major` = a viewer would notice it at first glance at real speed, or it breaks
   an identity trait or the project's north star (if the project layer names one). `medium` = noticed on a second
   look. `minor` = only frame by frame.
5. **The judge answers every item.** Accepted (it becomes a gap) or rejected with the frame that disproves it. No item
   is ignored. Rejected items stay written in the review.
6. **What reaches the approver.** Only work with a PASS from the judge **and** no accepted `major` item still open.
   Open `medium`/`minor` items go to the approver in one line, so they know what was chosen not to fix.
7. **Small cost.** One critic pass per round, same grids as the judge, no new capture. It does not count as a
   revision round. If it finds only engine/game-code flaws, they go to the code queue as usual.

## What does not change
The capture-chain check, the round limit, and the rule that design decisions belong to the approver. The critic never
writes instructions for the executor: the judge writes them, starting from the gravest accepted item.
