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

ESITO: PARZIALE preparazione cloud fatta e controllata (check_2b 0 problemi), spostamento sul fisso non ancora fatto
SUBAGENTI: 0
COSTO: ~4 $ | 0 minuti desktop | 0 token review
