# character-kits-1: kits for BirdKnight, FlameFury, Voyager and first Voyager movement tuning (cloud, 2026-11-24)
Branch `claude/project-thread-8mc1et`, thread "Preparare i kit personaggi e tarare Voyager". Owner 22:51Z: estimates on a "medium"
character; set up the other proposed kits; start tuning the Voyager. Reused: KIT_Test_Playtest (shared systems), FGSTN_LocomotionKitExtension
(per-kit profile already supported, INDICE §5.3), the run-stage fields of setup_run_start_ramp.py, gstn_content rules.

## Done (code/data scripts only, no C++)
- `Tools/GAS/gstn_content.py`: `ROSTER` + folder rule `KIT|WPN_Test_<Name>` -> `Characters/<Name>/Kits`.
- `Tools/GAS/setup_character_kits.py`: KIT_Test_<Name> = copy of KIT_Test_Playtest (display name in caps), DA_Locomotion_<Name> = copy of
  the LOCAL DA_Locomotion_Default, kit extension relinked to its own profile. Existing assets kept; Voyager values only on creation
  (CHARKITS_VOYAGER=1 rewrites, CHARKITS_MODE=dump reports). Prefix KIT_Test_ kept so the shared-system scripts reach them.
- `docs/notes/gasp-divergenze.md` row 29 (28 went to the falling mode in #57).

## Voyager values (paper model, linear ramp, 62.5 Hz; Default = last dump 470/2000 r14/0.15, 610/1400 r30/0.25, 850/1200)
stage 1 500 / 2600 ramp 10 f / 0.30, 32 f to stage 2; stage 2 650 / 1700 ramp 20 f / 0.30; stage 3 850 / 1400; stamp 10 f x0.15.
Steps into stage 2/3: EntryBurstFrames 3 x6 / 2 x8 (as setup_run_stage_steps.py on the Default, thread u1p2ha).
Model: 200 cm/s at 0.22 s (Default 0.21), 600 at 0.66 s (0.88), 800 at 1.26 s (1.44), 439 cm in 1 s (404). Stamp B (21 f x0.05):
200 at 0.40 s; stamp C (none): 200 at 0.14 s. Jump untouched (Default designer). To be measured in game, not trusted.

## To do (desktop, editor closed, ~1 min, the owner's local Default must be there)
`UnrealEditor-Cmd ... -run=pythonscript -script=Tools/GAS/setup_character_kits.py -unattended -nullrhi`, read
Saved/CharacterKits/setup_character_kits.txt, play the 3 kits from the GSTN Debug Options kit list, commit the 6 new .uasset (never
DA_Locomotion_Default). Optional: setup_kit_visuals.py does NOT recolour them (Visual copied from Playtest, not empty).

Owner 2026-11-24 23:00Z: 1A (stamp 10 f x0.15) 2A (values above) confirmed.

## Resume point (2026-11-25 00:31Z, owner's hotspot off for the night)
Owner OK for the desktop (00:22Z, "ti lascio entrambi per la notte"). The RC session found the owner's tuned DA_Locomotion_Default in the
MAIN checkout GASP_Giustino2 (not _ue59): run there, after a backup of the Default, _ue59/IN-USO.txt as machine lock, no git
checkout/stash there (owner's VFX work). Order: setup_run_stage_steps.py from claude/project-thread-u1p2ha (STEPS_MODE=dump, then
STEPS_ANIM_SET=0), then setup_character_kits.py, then PIE side videos of the 3 kits + Playtest, then commit the 6 .uasset from a
separate worktree of this branch. Stopped by the network before or during the scripts: see the RC report in the thread.

## Desktop run (2026-11-25/26, commit 1899bbb2)
Kits and profiles made in GASP_Giustino2 from the owner's Default (real values: stage 1 600/2000 180 f, stage 2 610/1400 94 f, stage 3
850/1200, stamp 16 f x0.10). Owner kept Voyager stage 1 at 500 (card B). PIE: Voyager 600 cm/s at 0.62 s vs 0.74-0.78 s, stage 3 at 2.4 s
vs 5.1 s (docs/notes/reviews/character-kits-1/README.md). Voyager collar pink close to Playtest's.
NOT done: setup_run_stage_steps.py on the Default/BirdKnight/FlameFury: the desktop agent's permission check blocks it even with the owner's
typed OK; command for the owner is in the thread. Backup: GASP_Giustino2/Saved/Backup/DA_Locomotion_Default_owner_2026-11-25.uasset.

ESITO: OK kit e profili creati, provati in PIE e committati; scalini sul Default rimandati all'owner
SUBAGENTI: 0
COSTO: ~8 $ | ~45 min desktop | 0

## Kit colours, desktop check (M6, 2026-11-27)
Branch at 70192666 (colour rules 979636c6), built in the 3dyy2t worktree: build OK (24 s). PIE `L_Test_Hub`, listen host + 2 clients, the host gets`n`KIT_Test_Playtest / Overlord / BirdKnight / Voyager / FlameFury` in turn (test_hub.kit), back-buffer `Shot` per kit. Collar (`gstn.Greybox 1`)`nand back box (mannequin): copper, copper, turquoise, black, red, as asked. Proof: `docs/notes/reviews/character-kits-1/collars_m6_greybox.png`,`n`collars_m6_mannequin.png`.

ESITO: PASS colori dei collari giusti su capsula greybox e manichino
SUBAGENTI: 0
COSTO: ~2 $ | ~10 min desktop | 0
