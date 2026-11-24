# character-kits-1: kits for BirdKnight, FlameFury, Voyager and first Voyager movement tuning (cloud, 2026-11-24)
Branch `claude/project-thread-8mc1et`, thread "Preparare i kit personaggi e tarare Voyager". Owner 22:51Z: estimates on a "medium"
character; set up the other proposed kits; start tuning the Voyager. Reused: KIT_Test_Playtest (shared systems), FGSTN_LocomotionKitExtension
(per-kit profile already supported, INDICE §5.3), the run-stage fields of setup_run_start_ramp.py, gstn_content rules.

## Done (code/data scripts only, no C++)
- `Tools/GAS/gstn_content.py`: `ROSTER` + folder rule `KIT|WPN_Test_<Name>` -> `Characters/<Name>/Kits`.
- `Tools/GAS/setup_character_kits.py`: KIT_Test_<Name> = copy of KIT_Test_Playtest (display name in caps), DA_Locomotion_<Name> = copy of
  the LOCAL DA_Locomotion_Default, kit extension relinked to its own profile. Existing assets kept; Voyager values only on creation
  (CHARKITS_VOYAGER=1 rewrites, CHARKITS_MODE=dump reports). Prefix KIT_Test_ kept so the shared-system scripts reach them.
- `docs/notes/gasp-divergenze.md` row 28.

## Voyager values (paper model, linear ramp, 62.5 Hz; Default = last dump 470/2000 r14/0.15, 610/1400 r30/0.25, 850/1200)
stage 1 500 / 2600 ramp 10 f / 0.30, 32 f to stage 2; stage 2 650 / 1700 ramp 20 f / 0.30; stage 3 850 / 1400; stamp 10 f x0.15.
Model: 200 cm/s at 0.22 s (Default 0.21), 600 at 0.66 s (0.88), 800 at 1.26 s (1.44), 439 cm in 1 s (404). Stamp B (21 f x0.05):
200 at 0.40 s; stamp C (none): 200 at 0.14 s. Jump untouched (Default designer). To be measured in game, not trusted.

## To do (desktop, editor closed, ~1 min, the owner's local Default must be there)
`UnrealEditor-Cmd ... -run=pythonscript -script=Tools/GAS/setup_character_kits.py -unattended -nullrhi`, read
Saved/CharacterKits/setup_character_kits.txt, play the 3 kits from the GSTN Debug Options kit list, commit the 6 new .uasset (never
DA_Locomotion_Default). Optional: setup_kit_visuals.py does NOT recolour them (Visual copied from Playtest, not empty).

Owner 2026-11-24 23:00Z: 1A (stamp 10 f x0.15) 2A (values above) confirmed.

ESITO: PARZIALE script pronto, non eseguito sul fisso (coda del fisso)
SUBAGENTI: 0
COSTO: ~3 $ | 0 min desktop | 0
