# Knowledge index (project layer)

The **general** knowledge (animation and VFX principles, criteria with stable IDs, checklists, review rubric, figures,
measuring tools) lives in the skills repo **`xJusteen47/game-anim-vfx-review-skills`**, folder `knowledge/`, mirrored
here in [`skills-public/knowledge/`](../../skills-public/knowledge/INDEX.md) (start at its `INDEX.md`). This folder holds
only what is specific to this game.

## Read this first
1. Any animation or VFX task: open `skills-public/knowledge/INDEX.md` → "Read this first" for your task.
2. Then [profile-gstn.md](profile-gstn.md): the owner's style and rules for this game, with their scope.
3. Then the worked case for your move type in [cases/](cases/) if one exists.
4. Authoring or generating clips: run `skills-public/tools/animkb/kb_motion.py metrics` on every output and attach
   the json to the handover. No in-game review without it.
5. Reviews cite IDs: `P-…` (animation), `V-…` (VFX), `X-…` (shared), profile rows by their item name.

| Topic | File | One line |
|---|---|---|
| Style profile (level 2) | [profile-gstn.md](profile-gstn.md) | north star, mass, timing scale, views, VFX palette and behaviour, owner rules with scope |
| Locomotion cases (level 3) | [cases/locomotion.md](cases/locomotion.md) | run stop, run start, skid/turn, landing, return to idle |
| Attack cases (level 3) | [cases/attacks.md](cases/attacks.md) | light / medium / heavy, frame data, impact and hit-pause |
| Course study notes (private) | [study/](study/) | paraphrased notes from licensed courses, with timestamps; never copied to public repos |
| How to ingest a course | [study/INGESTION.md](study/INGESTION.md) | audio → transcript → notes → frames → integrate |
| Measuring tools | `skills-public/tools/animkb/` | `kb_motion.py` (life metrics, figures, lifeless rebuild), `fbx_to_bvh.py`, `kb_diagrams.py` |
| GASP Mover 5.9 structure | [gasp/gasp-structure-5.9.md](gasp/gasp-structure-5.9.md) | how the stock Mover ABP + chooser + pose search pick clips, exit rules, root motion/curve needs, our setup vs stock, rules to add custom clips/states (evidence dumps in gasp/evidence/) |

## Three levels, never mixed
General principles (skills repo) · this game's style (profile) · worked cases (examples, not rules). The owner's words
are recorded with their scope; a remark made for one move or frame is never applied to everything.
