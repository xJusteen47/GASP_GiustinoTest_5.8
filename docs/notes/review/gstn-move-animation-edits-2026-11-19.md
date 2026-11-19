# Proposed edits to the account skill `gstn-move-animation` (2026-11-19)

Draft for the coordinator to propose to the owner (`propose_skills`, kind improvement, `edits`). Each `old` occurs
exactly once in the synced SKILL.md read on 2026-11-19. Goal: align the skill with today's additions (blind critic,
reference dossier, GASP curve check, Unreal Editor AI assistant, judge against the reference, project rule 3).

## 1. Effort: "Never Fable" contradicts project rule 3 (Fable judges animations)
old:
```
- Pose and timing decisions, judging an animation, reading silhouettes: **high** effort,
  or xhigh when the owner asks. Never Fable.
```
new:
```
- Pose and timing decisions, judging an animation, reading silhouettes: **high** effort,
  or xhigh when the owner asks. The rule-10 video review follows project rule 3: Fable high
  with the `gstn-anim-reviewer` skill, its blind critic and the reference dossier.
```

## 2. Cloud review: name the reviewer skill
old:
```
  - The cloud thread reviews images only of the final version.
```
new:
```
  - The cloud thread reviews images only of the final version, with `gstn-anim-reviewer`
    (blind critic, reference dossier, GASP curve check). Only a PASS reaches the owner.
```

## 3. References: write the dossier before animating
old:
```
- **First step for every new move**: run
```
new:
```
- **Reference dossier (owner 11-19)**: if the move has a reference video (owner clip, SSBU
  GIF), break it down once in writing before animating, from `docs/notes/review/dossier-template.md`
  (principle in `dossier-riferimento.md`): timing per phase in game frames from 1, key poses,
  sizes relative to the body, follow-through, identity traits, what not to copy, what is not
  measurable. It is the target while animating and the yardstick in review. Example:
  project files `handoffs/dash-ganon-ref/NOTE.md`.
- **First step for every new move**: run
```

## 4. Checks: the reference wins over a full set of passing checks
old:
```
   (`sheets/<move>_before_after.png`). For reference work, also compare with the reference
   (`sheets/<ref>_vs_vN.png`).
```
new:
```
   (`sheets/<move>_before_after.png`). For reference work, also compare with the reference
   (`sheets/<ref>_vs_vN.png`) and its dossier: all checks passing is not enough if the move
   does not hold up next to the reference video.
7. **GASP curves.** GASP reads animation curves from clips and montages (foot IK, warping,
   distance matching, `RootMotionTranslationScale`, others). Compare the curves on the baked
   clip and montage with the list the ABP really reads (the project's GASP curve check; never
   names from memory) and with the base clip: a re-bake must not drop them. Missing curves
   look like game bugs (sliding or popping feet, snapping IK).
```

## 5. Unreal: the editor AI assistant for engine questions
old:
```
- UE 5.8's AnimationAssistantToolset can only edit (Sequencer, Control Rig, keys). It never
  reviews a clip, so the review is always ours.
```
new:
```
- UE 5.8's AnimationAssistantToolset can only edit (Sequencer, Control Rig, keys). It never
  reviews a clip, so the review is always ours.
- **Engine questions (owner 11-19):** when in doubt on an Unreal question (ABP, montage and
  curve settings, Control Rig, Niagara), ask the Unreal Editor's built-in AI assistant first
  (needs the editor open); it gives engine-specific advice. Use it for how the engine works,
  never as the judge of how the move looks.
```
