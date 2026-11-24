# Riorganizzazione Content, fase 2b: kit e mosse in sottocartelle (preparazione cloud)

Thread cloud, Opus, 2026-11-24. Branch `claude/project-thread-e7z54v` da `origin/main` e2917db9 (PR #40, che contiene già la fase 2a). Scelta owner **A** (2026-11-24 15:23Z): tutto `Content/GASNPPTest/` in `Content/GSTN/`, un commit. Proposta: project files `handoffs/kit-mosse-cartelle-proposta.md`. Piano generale: `docs/notes/content-riorganizzazione.md` (1A 2A 3A).

## Struttura (regole in `Tools/GAS/gstn_content.py`)
- `GSTN/Characters/Overlord/Kits/`: `KIT_/WPN_Test_Overlord*` e `KIT_/WPN_Test_Playtest*` (il playtest porta il dropkick dell'Overlord).
- `GSTN/Characters/Overlord/Moves/{DAir,Dropkick,Jab,FThrow,Playtest}/`: AM_, AS_, GA_, TL_ di ogni versione insieme. Quale versione è approvata lo dice `anim-approvate.md`, non la cartella (una sottocartella `Variants/` cambierebbe percorso a ogni approvazione).
- `GSTN/Combat/{Reactions,Archetypes,Hurtboxes}/`, `GSTN/Animation/AirCrouch/`, `GSTN/Input/`, `GSTN/Core/` (personaggio e game mode), `GSTN/Maps/Hub/`, `GSTN/Data/Locomotion/` (DA_Locomotion_Default: solo con l'owner, 3A).
- `GSTN/_Test/{Combat,Maps,Stances,Minions}/`, `GSTN/_Examples/{Combat,Cosmetics,DoubleShot}/`.
- Nomi invariati (rinomine = fase 3, 2A). `AnimPipeline/` (anche `Published/`) non si tocca.

## Fatto in cloud (solo testo)
- `Tools/GAS/gstn_content.py`: una tabella nome -> cartella; gli script chiedono `gc.path(nome)` / `gc.move_folder('Dropkick')` invece di una cartella scritta a mano. Pura Python.
- `Tools/Reorg/content-map-2b.csv`: 276 righe generate dalla tabella (`git ls-files Content/GASNPPTest`), `DA_Locomotion_Default` con `quando=owner`.
- `reorg_rewrite.py --map Tools/Reorg/content-map-2b.csv` applicato: 78 file, 191 sostituzioni; poi a mano ~45 script con cartelle (setup kit e mosse, test, scenari), `DefaultGame.ini` (scansione kit `/Game/GSTN`), 1 C++ di test, README e doc vivi, riga dair v9 di `anim-approvate.md`.
- `reorg_apply.py`: salta le righe `quando=owner` senza `REORG_OWNER=1`; non risalva mai `DA_Locomotion_Default` / `CHT_*_PoseMatch`.
- Nuovi: `Tools/Reorg/reorg_inventory_2b.py` (asset non in git sotto `/Game/GASNPPTest` sul fisso, aggiunti al CSV), `Tools/Reorg/check_2b.py` (CSV e script d'accordo, nessun testo vivo col vecchio percorso): `CHECK 2b: 276 righe, 0 problemi`.
- Nessun asset spostato: il branch NON va unito prima del passo desktop (il testo punta già ai percorsi nuovi).

## Passi sul fisso (checkout principale, editor chiuso; dopo che `_ue59` e il fisso sono liberi)
1. `git fetch; git checkout claude/project-thread-e7z54v; git merge origin/main`; tag `pre-reorg-2b-2026-11-24`; zip di `Content/GASNPPTest` + `Config` in `Saved/Backups/Reorg-2b/`.
2. Inventario: `REORG_DRY=1` poi reale, `UnrealEditor-Cmd.exe GASP_Giustino2.uproject -run=pythonscript -script="<abs>/Tools/Reorg/reorg_inventory_2b.py"`. Ogni `NO RULE` -> regola in `gstn_content.py` (o `_Archive`), poi `python Tools/Reorg/check_2b.py`.
3. Owner presente per `DA_Locomotion_Default` ("ok salva" o backup): solo allora `REORG_OWNER=1`.
4. `REORG_MAP=<abs>/Tools/Reorg/content-map-2b.csv` + `reorg_apply.py`: prima `REORG_DRY=1`, poi reale (log `Saved/Logs/ci/reorg_apply.txt`).
5. `python Tools/Reorg/check_2b.py`; cartelle vuote di `GASNPPTest` via dall'editor.
6. Prove: `Run-Timed.ps1 -Preset Build`; `-Preset Test -Tests GSTN` + `Check-TestLog.ps1` (atteso come fase 2a: 2 fail noti Loadout.Screen); `-Preset Capture -Script Tools/Reorg/smoke_hub_pie.py`; un giro del PIE harness; kit nell'Asset Manager (`KIT_Test_Playtest`, `KIT_Test_Overlord` caricabili da `gstn.Debug.KitAtStart`).
7. Un commit dei binari sullo stesso branch, PR verso main, `_ue59` riallineato dopo.
- Branch vivi con binari nei vecchi percorsi (capsula, lanci): dopo il merge si allineano con `reorg_apply.py` + `REORG_MAP=content-map-2b.csv` e `reorg_rewrite.py --map ...`.

## Fatto sul fisso (2026-11-24 16:10-16:50Z, checkout principale, scelta owner A)
- Stash `pre-reorg-2b WIP` (`_ue59/IN-USO.txt` "libero" + `Content/Blueprints/NewBlueprint{1,2}`), backup `Saved/Backups/Reorg-2b/` (DA_Locomotion_Default + zip GASNPPTest/Config), tag locale `pre-reorg-2b-2026-11-24`. Main era già dentro.
- Inventario: 276 asset, 0 fuori da git, 0 senza regola. Apply: 275 spostati, 275 redirector tolti, 109 referencer risalvati, `DA_Locomotion_Default` ASPETTA OWNER (resta in `GASNPPTest/Combat/`, intatto: hash uguale). Nessun redirector rimasto (il DA non punta a nulla di spostato; i kit puntano al DA, che non si muove). `check_2b`: 0 problemi.
- `reorg_apply.py`: il commandlet tiene aperto il pacchetto redirector, `os.remove` falliva (WinError 32) -> ora scrive `REDIRECTOR DA TOGLIERE <file>` e il chiamante lo cancella a editor chiuso (57 file); un redirector rimasto al vecchio percorso non conta più come asset (prima un secondo giro dava CONFLITTO). Terzo giro: 275 GIA FATTO + 1 ASPETTA OWNER.
- GSTNDefence è spento di default: l'Asset Registry del commandlet non vedeva i suoi `GA_Getup{Neutral,Roll,RollBack}_Test`, rimasti sui vecchi `AM_React_*` (lo smoke li segnalava). Ripuntati con `Tools/Reorg/fix_getup_montages_2b.py` (solo il montage della timeline). Da ricordare per i branch vivi.
- Gate pose approvate: `AS_OverlordDAirV9` (approved/dair-v9) spostato -> blob diverso (il nome del pacchetto è dentro il .uasset). `regress.py` ora cerca il file con lo stesso nome al tag e confronta il blob (resta FAIL, ma dice "moved from"). Prova: `Tools/Reorg/compare_moved_anim.py` (vecchio file rimesso al vecchio percorso dal backup, sha = tag): 42/42 frame, 93 tracce, delta max 0.000000 cm / 0.000003 gradi, 0 curve, 0 notify -> animazione identica. Build fatte con `-SkipRegress`. Anche `Published/dair/v9/AM_OverlordDAirV9` risalvato (riferimenti). **Decisione owner** (sotto).
- Prove: Build OK (`Saved/Logs/ci/build-2b.log`); Test GSTN **1937 ok, 0 fail** (`test-2b.log.ue.log`; i 2 fail noti Loadout.Screen ora passano) dopo il fix di un crash in `GSTN_CosmeticsCoreTests.cpp:266` (`Slots.Add(Slots[0])` asseriva alla riallocazione, arrivato da main); smoke hub `SMOKE: OK` (`smoke-2b.log.ue.log`). Grep: nessun `/Game/GASNPPTest/` mancante; resta `DA_GSTN_SurfaceResponse_Default` non trovato, già noto prima (neck-hold-desync-2), non 2b.
- Non fatto: cartelle vuote sotto `Content/GASNPPTest/` (Combat/*, Examples, Input, Stance, TestMaps) non cancellate (comando negato dal permesso); git non le vede, si tolgono a mano. Harness PIE e kit: vedi sotto.

- Dopo il commit (16:45-17:15Z): stash `pre-reorg-2b WIP` ripreso; conflitto su `_ue59/IN-USO.txt` risolto tenendo la riga del branch (decisione coordinatore), stash@{0} TENUTO, `NewBlueprint1/2` di nuovo non tracciati. Il checkout era su `claude/content-reorg-apply` prima del 2b.
- Harness PIE (`m3_suite_driver.py`, 29 scenari, listen + 2 client, 60/15 ms, DefaultLevel): 24 PASS, 5 FAIL (`air_jump_fastfall`, `tilted_gravity_jump`, `super_code`, `spike_stats`, `loadout_change`). Gli stessi 5 falliscono con gli stessi messaggi su main e2917db9 (prima del 2b): non sono dello spostamento. Report `Saved/R31/report-2b.txt` e `report-main5.txt`, log `Saved/Logs/ci/harness-2b.log`, `harness-main5.log`. Il commit di preparazione 1cb3b2b8 da solo non è una base valida (testo già sui percorsi nuovi, asset ancora nei vecchi: il driver si è bloccato).
- Kit nell'Asset Manager (commandlet): `KIT_Test_Playtest` e `KIT_Test_Overlord` caricati da `/Game/GSTN/Characters/Overlord/Kits/`, PrimaryAssetId `GSTN_PrimaryKit`, elencati nel tipo (47 kit): OK (`Saved/Logs/ci/kit_am_check.txt`).

## Decisione per l'owner (gate dair v9)
A (consigliata) tag nuovo `approved/dair-v9-2b` sul commit dello spostamento + riga in `anim-approvate.md`: il vecchio tag resta, il gate torna verde senza ammorbidirlo; costo ~0.3 $.
B `regress.py` accetta un file spostato solo con una prova ossa registrata (json in `approved-dumps/`): più generale, ~1 $.
Finché non si sceglie, ogni `-Preset Build` su questo branch va con `-SkipRegress`.

ESITO: PARZIALE spostamento 2b fatto e provato (build, 1937 test, smoke); manca la scelta owner sul gate dair v9 e DA_Locomotion_Default con l'owner
SUBAGENTI: 0
COSTO: ~4 $ cloud + ~6 $ fisso | 40 minuti desktop | 0 token review
