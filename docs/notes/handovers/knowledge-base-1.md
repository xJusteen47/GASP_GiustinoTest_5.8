# Handover: animation and VFX knowledge base (2026-11-19)
Thread cloud (+ desktop VM shell for the courses, no editor, `_ue59` untouched). Opus; 4 Sonnet subagents for the course digests.
Branches: game repo `claude/project-thread-m6s9y2` (from `claude/project-thread-hdoy7s`); skills repo
`xJusteen47/game-anim-vfx-review-skills` branch `knowledge-base` (from `main` 0cdf55c).

## Done
- **Level 1 (skills repo `knowledge/`, mirrored here in `skills-public/`)**: animation criteria with stable IDs in 9
  families (`P-TIME/SPACE/ARC/WEIGHT/CONTACT/ANTIC/SETTLE/OVERLAP/HOLD/LIFE/POSE/STRETCH/GAME/MEASURE`), checklists per
  move type, generic review rubric (LIFE, WEIGHT, ROOT are FAIL axes), sources; VFX as a **separate** base
  (`vfx-criteria.md` `V-*` + `vfx-theory.md`, moved from `vfx-reviewer/vfx-principles.md`); `anim-vfx-links.md` (`X-01..10`);
  `PROFILE-TEMPLATE.md`; `INDEX.md` with "Read this first" at each level.
- **Tools** `tools/animkb/`: `kb_motion.py` (life metrics, figures, lifeless rebuild), `fbx_to_bvh.py`, `kb_diagrams.py`.
- **Levels 2/3 (this repo `docs/knowledge/`)**: `profile-gstn.md` (owner rules with scope; the Astro Bot/DK "look"
  section moved here from the generic VFX theory), `cases/locomotion.md`, `cases/attacks.md`, `study/` (course notes,
  `INGESTION.md`), `INDEX.md`. Project skill layers (`.claude/skills/gstn-*/project.md`) point to it.
- **Courses** (owner's Gnomon, `F:\Corsi`): audio → whisper.cpp base.en in cloud → paraphrased notes per course.
  Audio, transcripts and 32 frames stay on the desktop in `F:\Corsi\_studio_claude\`. Nothing from the courses in the skills repo
  except criteria rephrased and generalised (13 new `P-*`, 2 `X-*`, new family `P-REF` with 8 criteria and a
  procedure). Three courses digested: Body Mechanics, Stylized Attacks (two parts), Video Reference.
- Draft edit for the account skill: project files `anim-skills/gstn-move-animation-kb-edit-2026-11-19.md`.

## Evidence (rule 11)
- Life metric: `stop_sync_share` mocap 0.00-0.25 vs lifeless rebuild 0.88-0.96 on CMU 02_05, 09_01, 16_05, 16_08, 16_41
  (`skills-public/knowledge/animation/figures/metrics-cmu-vs-lifeless.json`, from `kb_motion.py compare <clip> <clip>_L`).
- Course fps = 24: Maya playback field on `F:\Corsi\_studio_claude\frames\SA05_0500.jpg` and `SA08_0844.jpg`.
- One subagent claim corrected after checking the transcript (SA11 06:43: effect timing agrees with `X-01`).
- Whisper model `ggml-base.en.bin` SHA-1 137c40403d78fd54d454da0f9bd998f78703390c (official value).

## Choices applied (rule 6)
- English for the KB (agent use), Italian overview in project files `knowledge/panoramica-knowledge-base.md`.
- The skills repo is now where generic content is edited; `skills-public/` here is a mirror made with `git archive`
  (supersedes the earlier "skills-public is the source of truth" note in `skills-split.md`).
- Course numbers enter only cases (level 3) as ratios with 60 fps equivalents; level 1 keeps book/physics/measured ranges.

## Not done / open
- `stop_sync_share` not yet calibrated on our failed clips (the two FBX in the repo are 25-frame clips, too short).
  Next: export the failed run start/stop clips from the desktop and measure them.
- Course frames are not annotated (they stay private on the desktop); the public figures are from CMU and diagrams.
- Owner: whether to merge both branches; whether the skills repo goes public (course content is not in it).
