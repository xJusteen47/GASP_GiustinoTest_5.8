# Riorganizzazione cartelle asset: piano (fase 1, 2026-11-16)

Base: `claude/integ-playtest-1116` b528f9a8. Inventario: 499 asset versionati (466 in Content, 33 nei plugin). Mappatura asset per asset: `handoffs/riorganizzazione-cartelle-mappa.csv` (239 righe: 189 da spostare, 17 in archivio, 32 restano, 1 da verificare). Riferimenti contati leggendo le tabelle dei nomi degli .uasset (LFS) e cercando i percorsi in C++/ini/script (`refs.py` + `mapgen.py`, thread a0j7ip).

## Cosa si scopre
- `Content/Blueprints`, `Content/Input`, `Content/Levels` sono quasi tutti GASP (Epic, 269 file dell'import iniziale): **non si toccano**, altrimenti si perde la possibilità di aggiornare GASP. Si tirano fuori solo i nostri 18 file finiti lì (wall layer, DepthMarker, Equipment, L_Test_Hub, L_WallSlideAB, DefaultLevel_Harness).
- La cartella disordinata vera è `GASNPPTest/Combat` (64 asset: kit, armi, hurtbox, esempi, mappe, materiali fisici tutti insieme).
- I kit non hanno riferimenti fra asset perché li trova l'Asset Manager per cartella (`PrimaryAssetTypesToScan` = `/Game/GASNPPTest`): da aggiornare. Quasi tutto il resto è citato per nome dagli scenari Python/C++.
- 81 file di codice, ini e script contengono percorsi `/Game/...` da riscrivere: i redirector non li sistemano.
- Dipendenze plugin→progetto da correggere (contro la regola dei plugin autonomi): ini di GSTNHeld, GSTNSurface, GSTNLoadoutMenu e test di GSTNDefence/GSTNPiggyback puntano a `/Game/...`.

## Struttura proposta
| Radice | Contenuto |
|---|---|
| `/Game/GSTN/` (gioco, "definitivo") | `Core` (personaggio, game mode), `Characters/Overlord/` (kit+arma) con `Moves/<DAir,Dropkick,Jab1,Jab2>/` (AM, AS, GA, TL della mossa insieme) e `Variants/`, `Combat/Reactions`, `Input` (+`Actions`), `Movement`, `Backpacks`, `Animation/Layers` e `Placeholders`, `DepthMarker`, `Equipment`, `LevelObjects`, `Maps/` (L_Test_Hub + `Hub/Materials`) |
| `/Game/GSTNTests/` (fixture dei test) | `Combat/Kits, Weapons, Supers, Mods, Hurtboxes, Timelines, Archetypes/<Light,Medium,Heavy>, Rules, Stances, Surfaces, Backpacks`, `Maps` (L_Test_Combat, L_WallSlideAB, DefaultLevel_Harness) |
| `/Game/GSTNExamples/` | esempi per designer (CombatSeams, DoubleShot, Cosmetics, kit di esempio) |
| `/Game/GSTN/_Archive/` | DAir V7/V8, Jab V2 (catene intere), AM_React_GroundHit: niente li usa; **non si cancella nulla** |
| Plugin | `Content/<Sistema>` = shipping (es. `GSTNCombat/Rules`), `Content/Examples`, `Content/Test` = fixture. Unico cambio: `GSTNParts/Samples` → `Examples` |
| Resta | `AnimPipeline/` (sorgente Live Link, doppione voluto delle AS del dropkick), tutto GASP. `NewBlueprint` (Actor vuoto in DefaultLevel) da verificare a occhio |

Prefissi: già quasi tutti UE (KIT_, WPN_, GA_, AM_, AS_, TL_, HBS_, DA_, IA_, IMC_, MI_, PM_ ecc.). Conflitto da notare: `PM_BoostJump` (piggyback pair move) usa il prefisso dei Physical Material. Rinomine proposte nella colonna nota (es. `KIT_Test_Overlord` → `KIT_Overlord`), **non** in fase 2.

## Fase 2 (fisso, editor, Opus medium, ~10 $, +20% piano)
1. Momento sicuro: dopo che il PR di `integ-playtest-1116` è in `feature`, e quando nessun branch aperto ha modifiche binarie non integrate (mario-run, direzione attacco, test kit/walk feel: o mergiati o fermi). Il coordinatore lo segna in INDICE.md; `_ue59/IN-USO.txt` preso per tutta la fase.
2. Script editor `Tools/Reorg/reorg_apply.py` legge il CSV: `EditorAssetLibrary.rename_asset` (lascia redirector), poi Fix Up Redirectors su `/Game` e plugin, poi cancella le cartelle vuote. Idempotente.
3. Script testo `reorg_rewrite.py`: riscrive i percorsi nei 81 file (C++, ini, Tools) dal CSV; `PrimaryAssetTypesToScan` → `/Game/GSTN`, `/Game/GSTNTests`, `/Game/GSTNExamples`. Gli handover storici non si riscrivono.
4. Mappe: si risalvano solo quelle spostate (L_Test_Hub e le 3 di test) e i due GASP già modificati che le referenziano (SandboxCharacter_Mover e ABP). Le altre mappe GASP non cambiano.
5. Build + 734 test + apertura editor pulita (nessun redirector rimasto, nessun "missing asset" nel log), un solo commit, PR verso feature.
6. Altri branch: non fanno rebase dei binari. Rieseguono `reorg_apply.py` + `reorg_rewrite.py` sul loro branch (stesso risultato, è deterministico), committano, poi mergiano feature: conflitti solo sugli asset modificati da entrambi, come sempre.

## Decisioni per l'owner (applico la consigliata)
1. Esempi: **A** in `/Game/GSTNExamples` ora, nel plugin dopo; B nel plugin GSTNCombat subito (+~4 $, tag degli esempi da spostare nell'ini del plugin).
2. Rinomine (via `_Test_` da Overlord, `GASNPP` → `GSTN`): **A** fase 3 separata (~3 $); B insieme alla fase 2.
3. Dipendenze plugin→`/Game`: **A** sistemarle nella fase 2 (righe ini spostate nel `DefaultGame.ini` del progetto, ~+1 $; gli asset di test di GSTNDefence/GSTNPiggyback che usano reazioni e DA_Locomotion restano annotati); B più avanti.
