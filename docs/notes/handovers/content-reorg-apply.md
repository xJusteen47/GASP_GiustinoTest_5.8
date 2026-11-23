# Handover: riorganizzazione Content, fase 2 (i 55 file nostri fuori da GASP)

Thread desktop (fisso, editor headless + 1 avvio editor), Opus medium, 2026-11-23. Branch `claude/content-reorg-apply` da `origin/main` a5b2095, tag `pre-reorg-2026-11-23`. Piano: `docs/notes/content-riorganizzazione.md` (branch `claude/content-riorganizzazione`), scelte owner 1A 2A 3A, owner "ok salva" per i file locali.

## Fatto
- 55 asset spostati con `Tools/Reorg/reorg_apply.py` da `Tools/Reorg/content-map.csv` (idempotente): layer ABP/ALI/DA_AnimSet -> `GSTN/Animation/Layers/`, Ported57 e RunStop -> `GSTN/Animation/`, DA camera/ground contact -> `GSTN/Data/{Camera,Movement}/`, DepthMarker -> `GSTN/DepthMarker/`, placeholder -> `GSTN/Equipment/Placeholders/`, `L_Test_Hub` -> `GSTN/Maps/`, Harness e WallSlideAB -> `GSTN/_Test/Maps/`. GASP non toccato salvo i referencer.
- 24 redirector tolti dopo il risalvataggio dei referencer (41 asset: SandboxCharacter_Mover/_ABP, ABP_GSTN_Locomotion, 2 CHT, 34 KIT, 2 KIT piggyback). File locali dell'owner NON modificati.
- `Tools/Reorg/reorg_rewrite.py`: 65 percorsi in 46 file (Config, 3 C++, Tools) + `animr2/cap_one.sh` a mano. docs/handoffs/Artifacts restano storici.
- `UGSTN_GA_Jump`: DisplayName "GSTN Jump (test character only)" + nota nell'header.
- Fix di build già rotto in main: `GSTN_MissionCoreTests.cpp` (C4172, ritorno di riferimento a temporaneo).
- Scelta applicata: i 13 file della radice `Blueprints/Animation` tutti in `Layers/` (niente `Layers/Data/`), perché `setup_wall_anim_layer.py` crea ABP e DA nella stessa cartella.

## Prove
- Build: `Run-Timed.ps1 -Preset Build` -> `Saved/Logs/ci/reorg-build3.log` OK.
- Test: `Run-Timed.ps1 -Preset Test -Tests GSTN` + `Check-TestLog.ps1` -> 1875/1877 (`Saved/Logs/ci/reorg-test.log.ue.log`). Falliscono `GSTNCombat.GAS.Loadout.Screen` e `.ScreenSupportAndSuper`: la fixture è in memoria e il provider di riga del plugin GSTNBackpack aggiunge una riga; nessun percorso spostato coinvolto. Etichetta "gioco", va in coda codice.
- Editor: `Run-Timed.ps1 -Preset Capture -Script Tools/Reorg/smoke_hub_pie.py` -> mappa caricata da `/Game/GSTN/Maps/L_Test_Hub.umap`, PIE avviato (`reorg-smoke.log.ue.log`). Lo script di smoke aveva un bug (editor world None in PIE, ora corretto ma non rilanciato): controllo del pawn non fatto.
- Nessun "failed to load" su percorsi spostati nei log di test e smoke (`grep -E "Failed to load|Error" ... | grep /Game/(Blueprints|Levels|GSTN)`).

## Da sapere
- `CHT_GSTN_StockMover_PoseMatch` (file locale owner) cita 4 clip `AS_GSTN_RunStart_Generic_w2t{090,180}{L,R}f` che non esistono nel checkout: preesistente, non toccato.
- `CR_WallIkControlRig` contiene ancora stringhe interne RigVM col vecchio percorso di classe: si rinfrescano a una ricompilazione del rig in editor. Da controllare al prossimo test del wall slide.
- Le cartelle vuote vecchie (`Blueprints/Animation`, `DepthMarker`, `Equipment`, `Levels/Test`) restano su disco del fisso: cancellarle dall'editor.
- I ~38 branch con binari negli stessi percorsi vanno ribasati su questo commit; chi ha asset nuovi nei vecchi percorsi rilancia `reorg_apply.py`.
- Stash `stash@{0}` nel checkout del fisso: WIP del coordinatore (handoffs/INDICE.md, costi.csv, queue.json, skill executor) messo da parte per cambiare branch.
- `docs/INDICE-PROGETTO.md` non è in main: voce da aggiungere quando arriva.

## Seguito
Fase 3 (rinomine, decisione 2A) e spostamento di `GASNPPTest/` in `GSTN/` come da piano §3, thread desktop separato.

SUBAGENTI: 0
COSTO: ~9 $ | ~55 minuti desktop | 0 token review
