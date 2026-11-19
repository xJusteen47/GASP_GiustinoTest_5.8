# Review skills for game animation and VFX

Agent skills (Claude Code `SKILL.md` format, readable by any agent by path) for reviewing game character animation and
visual effects from in-game video, with an adversarial critic and a written reference dossier.

**Status: not published.** This folder is prepared for a possible public repository. Publishing, repository name and
license wait for the owner's explicit OK. No license is granted yet.

| Folder | What |
|---|---|
| `anim-reviewer/` | Judges an animation from in-game video, writes one edit instruction at a time. `scripts/rv_frames.py` = mp4 → one labelled frame grid (ffmpeg + Pillow). |
| `anim-executor/` | Applies one instruction, runs gates, returns before/after and PASS/FAIL. Never judges. |
| `vfx-reviewer/` | Judges a visual effect from in-game video (12 questions, 2 instrumental checks). `vfx-principles.md` = general game-VFX theory with sources. |
| `shared/` | `contract.md` (reviewer ↔ executor files), `aesthetic-critic.md` (blind critic), `reference-dossier.md` + `dossier-template.md`. |

## How a project uses them (two layers)
Copy or vendor this folder unchanged. For each skill, the project adds its own thin skill that says "read `project.md`,
then the generic skill", and a `project.md` with paths, tools, rig, style rules, north star, language and approval
rules. The project layer wins on conflicts. Never edit the generic core for one project: that is how copies diverge.

## Requirements
Vision-capable model for the reviewers; Python 3 with Pillow and ffmpeg on PATH for `rv_frames.py`. Engine notes in the
text use Unreal Engine as the example; the method is engine-agnostic.
