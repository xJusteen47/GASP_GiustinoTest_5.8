# Hitbox editor UX: plan only (cloud, 2026-11-15)
Branch `claude/project-thread-r8xxb2` from `claude/integ-cp3` 069287f7. No code changed.
Output: `docs/notes/hitbox-editor-ux-plan.md` (copy of project file `handoffs/hitbox-editor-ux-plan.md`): 8 friction points, checkpoints H0-H4 (37-48 $), 3 owner questions.
Sources read: `Plugins/GSTNCombatEditor/README.md` (timeline panel, toolkit, preview, gizmos, bake, outliner), `Tools/GAS/setup_overlord_jab_kit.py`.
Key finding (evidence): `setup_overlord_jab_kit.py` `_timeline()` builds a fresh `GSTN_CombatTimelineData` and `asset.set_editor_property('data', data)`
(line ~179) on an existing TL_Overlord_Jab asset: a re-run erases hand-placed hitboxes. Same pattern in setup_overlord_dair_kit.py / dropkick (add_hitbox_track on new data).
Friction points 2-8 are deduced from the README "Not built" notes, not observed: nobody has used the editor as a designer yet (H0 checks).
Applied defaults (rule 6): show animation and sim frame numbers both (setting); path vs bone-following hitbox stay both.
Owner questions (coordinator carries them): 1 try 10 min first (A rec) / 2 victim dummy in preview / 3 presets.
Next: H0 (desktop) and H1 (cloud) can start without answers.
Cost: ~60k context, Opus, no subagents; estimated ~$4-5.
