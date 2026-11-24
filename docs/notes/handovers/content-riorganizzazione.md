# Handover: riorganizzazione Content/, fase 1 (solo lettura)

Thread "Map and propose Content folder reorganization", cloud, Opus medium, budget 3 $, 2026-11-23. Branch `claude/content-riorganizzazione` da `origin/main` a5b2095. Nessun asset spostato, `_ue59` non toccata.

## Fatto
- `docs/notes/content-riorganizzazione.md`: mappa cartella per cartella, dati sparsi, struttura target, regole designer, rischi, piano desktop, cosa non toccare, 3 decisioni.
- Copia per l'owner: project files `handoffs/content-riorganizzazione.md`.

## Esiti chiave (numeri con comando nel doc, sezione "Comandi")
- Content versionato 641 file: 261 GASP (import f1a280d5), 380 nostri; 91 nostri nei plugin.
- 55 file nostri dentro cartelle GASP (Blueprints/Animation, Data, DepthMarker, Equipment, 3 mappe).
- "GSTN Test" non esiste come cartella in nessun branch: è `GASNPPTest/` (239 file, misto gioco/fixture/esempi).
- 343 file di testo fuori da docs/ citano percorsi `/Game/...` nostri; `PrimaryAssetTypesToScan` dei kit = `/Game/GASNPPTest`.
- ~38 branch claude/* dal 19-11 hanno binari diversi da main negli stessi percorsi (da classificare prima della fase 2).
- Il piano del 16-11 (`handoffs/riorganizzazione-cartelle-piano.md`) non è mai stato eseguito; questo lo sostituisce.

## Decisioni applicate (consigliate, in attesa owner)
1A test/esempi/archivio come `GSTN/_Test`, `_Examples`, `_Archive`; 2A rinomine in fase 3; 3A file locali dell'owner (DA_Locomotion_Default, CHT_*_PoseMatch) gestiti con lui presente prima dello spostamento.

## Non verificato
- Contenuto non versionato sul fisso (PC offline): il thread desktop fa l'inventario dall'editor al passo 3.
- Quali dei ~38 branch sono vivi: confronto solo con `git diff`, non con INDICE.md.

## Seguito
Thread desktop (fisso, editor, Opus medium, ~12 $, ~3 h) col piano §4, dopo le risposte dell'owner e lo stop dei branch vivi.

SUBAGENTI: 0
COSTO: ~1,5 $ | 0 minuti desktop | 0 token review
