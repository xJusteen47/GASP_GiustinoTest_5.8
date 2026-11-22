# Handover: jab arc + dropkick pose diagnosis (cloud, no editor, read-only, 2026-11-15, ~$5 est.)
Base `claude/project-thread-rmz1zr` 9d956124. Images `docs/notes/reviews/jab-dropkick-diag/{jab,dropkick}_confronto.png` (scripts beside them,
`python <script> <outdir>`); copies in project files `reviews/jab-dropkick-diag/`. Owner feedback 11-15 14:10Z / 14:13Z.

## Jab 1: the slap stops at contact (cause = KEYS, not hitbox, not montage)
- Card `overlord_jab1_v3.json` r2: chamber at the left jaw f4, contact f6 at 29 deg right of centre, then a "disdain hold" f6-f12.
  Hand travel per frame (`overlord_jab_v3_round2_notes.md` line "Hand steps"): 41.0, 75.8 into f6, then 3.6 3.1 0.4 1.0 1.0 0.4 cm.
  Capture cp5b v27-v33 (a6-a12, `jab_confronto.png`): body and hand frozen. The arc ends where it should be half-way.
- Arc was shortened twice: 40 -> 29 deg so the hand lands on the dummy (r2), and the plan ends the swing on the contact line.
- Not the hitbox: active 6-8 (`timing/OverlordJab1V3_timing.csv`), it does not stop the arm. Not the montage: cp5b shows 1 pose/frame.
## Dropkick: legs apart + arms flung (cause = POSE REWRITE of the Antigravity base)
- Antigravity r0 (`overlord_dropkick_{startup,loop,land}.r0.backup.json`, `_v1`): loop pelvis 80 deg, feet (2,-5,-82)/(-8,-5,-82)
  = legs together and straight, arms crossed on the chest (Overlord trait, owner Bison refs, `anim-pipeline.md` 145-166).
- Replaced by commit 26733f24 (11-14, thread anim-finish): feet (11.7,36.2,-75.5)/(-6.1,20.6,-76.9) = 16 cm staggered, 35 deg barrel
  roll, arms flung overhead/back. Reasons written in `overlord_dropkick_notes.md`: "crossed arms failed legibility from behind",
  "legs and arms leave the trunk in an X". Marked "owner to confirm" in a handover only; never asked.
- Hitbox until landing: owner confirmed 11-15 14:13Z it is right. Keep `setup_overlord_dropkick_kit.py` active window.
## Rule 12 (how a good version was replaced without the owner)
1. Brief bf61354c said the cards "only had pose names": false, r0 had full poses. 2. The executor applied the "behind view wins"
silhouette rule over the move's identity (dropkick = feet together, Overlord = arms crossed). 3. Rule-10 review judged the air part
against the rules, not against the owner's base. Prevention (applied, memory): a base the owner names is the base; changing a trait
in its `_design` needs an owner decision BEFORE build; reviews compare with that base.
## Plan (owner decisions in the thread message; recommended applied if no answer)
- Dropkick: restore r0 startup+loop poses, keep timing/hold/hitbox code; land clip re-keyed only at its start to leave from the r0
  loop pose (still ends on lieup). Desktop: build, capture, rule-10 review. ~$15-20.
- Jab 1: keep f0-f6, contact mid-swing; f7-f10 hand keeps travelling to the right of the body, slowing; short hold at arc end; FAF 22.
## Owner answers 11-15 14:19Z (msg cmsg_01CRvUdrWm1dJrM4q54YDBLN3bt1hdy2t88z79oCs9mc8a)
1A (restore Antigravity dropkick) + "improve follow-through and secondary motion" (scope: these moves, quality goal).
2: reference poses are open in Cascadeur (MCP, owner's machine): jab 1 slap toward the RIGHT, jab 2 palm toward the LEFT; improvable but
the base. 3 -> owner 14:19Z: jab hitbox active for most of the arc and HE places the hitboxes: threads do not author jab hitbox timing. Next: desktop thread, brief `handoffs/jab-dropkick-r1-desktop-brief.md`.
