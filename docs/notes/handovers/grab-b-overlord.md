# Handover grab-B (2026-11-28, desktop) - B non afferra con il kit Overlord
Branch `claude/project-thread-h46ltf` da main 82dfd88d.

## Causa
Nel playtest del 27-11 l'owner prende il kit dal pickup "OVERLORD JAB" del hub (`LogGSTNKitInteract: KitSwap ... took kit=KIT_Test_Overlord
left=KIT_Test_Playtest at GSTN_KitPickup_13`, log dell'editor dell'owner, 23:54 e 00:09 UTC). `KIT_Test_Overlord` non ha MAI avuto contenuto di presa
(nessuna versione in git): nessuna riga `Command.Link.Grab`, nessuno slot `Command.Link.*`, nessuna `FGSTN_LinkKitExtension`. Senza estensione i
manichini non vengono offerti come bersaglio (`Interact.Target.Grab`, M7 G1) e B non ha un comando di presa. PR #53 aveva dato la presa solo a
`KIT_Test_Playtest`. Esclusi: mappa del pad (B -> IA_GASNPP_Interact in IMC_GASNPP_Test; lo stesso B fa il kit swap), stash dell'owner
(`GSTN_CombatRules_Standard` stash vs main: stessi nomi, nessuna riga di link), validatore del loadout (nessun errore nel log).

## Fatto
- `Tools/GAS/setup_overlord_kit_grab.py`: copia sul kit Overlord la presa del kit Playtest (slot e righe standard via `WriteKitLinkContent` con
  `GA_Ganon_Playtest_Grab` e il suo rilascio, lanci di Ganondorf, regola di tenuta 90 + 1,7/%, niente mash). Salvato con `Save-Assets.ps1` in _ue59.
- `Tools/GASNPPTest/grab_b_check.py`: prova PIE su L_Test_Hub (manichini del hub), B iniettato via Enhanced Input (IA_GASNPP_Interact).

## Prove
- Prima: `KIT_Test_Overlord` 0/8 (nessun bersaglio, nessuna presa); `KIT_Test_Playtest` presa e spin OK.
- Dopo (`Saved/Logs/grab-ganon/grab_b_check.txt`): OVERALL PASS 12/12, su entrambi i kit: bersaglio offerto, B afferra, Attack lancia (13 %),
  giro di stick = spin. Nota: a 0 % la tenuta di Ganon dura 90 frame e ogni spin ne toglie 20: lanciare DOPO uno spin ha una finestra di pochi frame.
- Build `Saved/Logs/ci/build.log` OK (136 s); test GSTNLink 133/133 PASS.

ESITO: PASS presa con B su KIT_Test_Overlord (PIE 12/12), build OK, GSTNLink 133/133
SUBAGENTI: 0
COSTO: ~6 $ | 16 min desktop | 0 token review
