# Revisione visiva delle mosse: design definitivo (2026-11-03)

Richiesta owner 03/11 01:59Z: sistemare in modo definitivo le quattro cause per cui l'agente sbagliava la dair, e far
**guardare** il risultato a Claude (silhouette, leggibilità per parte, intermedi mancanti o venuti male, jitter), come
skill riusabile. Un solo percorso, già scelto. Nessun codice in questo documento.

## Le quattro cause e la correzione di ciascuna
| Causa | Correzione | Dove |
|---|---|---|
| 1. Pose dell'owner riscritte dagli agenti | lock `approved` + check **bloccante** `source_fidelity` (scheda vs posa owner in `cards/<kit>/refs/`, scarto max 0,5 cm / 1°, dita e `wrist_out` esclusi) + fantasma della posa owner nella griglia | PR #24 + CP1 |
| 2. Calcio giudicato come pugno | blocco `strike` di PR #24, tarato su dair v4 e jab v12 | PR #24 + CP1 |
| 3. I check numerici non vedono forma e leggibilità | revisione visiva con rubrica dalle regole del README | CP2-CP3 |
| 4. L'agente desktop non vede mai il risultato | cattura su file, revisore cloud Opus che legge le immagini e scrive un verdetto che la pipeline legge | CP2-CP3 |

## Il giro (massimo 2 per mossa)
1. **Desktop** `animpipe review <card> --visual`: build, check numerici (se un blocco fallisce si ferma, niente render), poi
   cattura in `Saved/AnimPipeline/visual/<move>/r<N>/`: 4 immagini composite (lato lungo 1568 px) e `manifest.json`
   (esiti numerici, frame chiave, chi possiede ogni chiave: owner o claude, segmenti di intermedi).
   - A `griglia_lato.png`: ogni frame, silhouette colorate per parte (braccio sx/dx, gambe, busto, testa), chiavi bordate,
     frame hitbox evidenziati, fantasma grigio della posa owner sulle sue chiavi.
   - B `camere.png`: le pose chiave da camera di gioco, fronte e dietro (caviglie incrociate, gomito dentro il busto).
   - C `traiettorie.png`: mano, piede, testa con un punto per frame (spacing, pop, buchi, tremolio).
   - D `hitstop_whiff.png`: striscia con e senza hitstop attorno al primo frame attivo.
2. **Cloud** (thread Opus medium, skill `gstn-visual-review`): copia la cartella dal PC con l'accesso cartella già
   approvato, guarda le 4 immagini **una volta**, applica la rubrica, scrive `verdict.json` sul PC e in project files.
3. **Desktop** legge il verdetto: `animpipe visual-gate` blocca `approve` e la consegna se c'è un problema `blocca`.
   Corregge solo pose di Claude e parti non bloccate; un problema su una chiave dell'owner diventa `chiedi_owner`, mai una
   modifica. Secondo giro uguale; dopo il secondo si ferma e consegna all'owner immagini + verdetto.

## La skill `gstn-visual-review` (`.claude/skills/gstn-visual-review/`)
- `SKILL.md`: quando usarla, input (cartella del giro, scheda, `cards/README.md`), passi, limiti (una lettura per
  immagine, niente modifiche alla scheda, niente loop).
- `rubric.md`: una voce per regola del README, ognuna con "cosa guardare, in quale immagine, quando è blocca":
  frame 0 vs idle e carica, "troppo è giusto" (posa spinta), sinergia, ginocchia avanti in carica, rilascio lento con il
  passo grande sul primo frame attivo, niente scatto dopo l'hitbox, mano villain, compenetrazioni, piedi affiancati;
  in più: silhouette leggibile dalla camera di gioco, parte che colpisce riconoscibile, linea d'azione, **intermedio
  mancante** (salto tra due frame), **intermedio brutto** (passa dentro il corpo, ginocchio indietro, pastone), jitter.
- `verdict.schema.json`: `{move, round, esito: ok|correggi|chiedi_owner, problemi: [{regola, frame, parti, gravita:
  blocca|nota, immagine, cella, prova, correzione: {frame, parte, direzione, quanto}}], segmenti: [{da, a, stato:
  ok|manca|brutto, perche}]}`.
- `calibrazione.md`: esempi noti. Deve dare **ok** su jab v12 e ftilt v3.1 (accettate) e **correggi** su dair v2 con i
  difetti che l'owner aveva visto (braccia ferme, testa che scatta, avambraccio nella testa).

## Numerico o visivo
- **Numerico, bloccante, prima del render** (costa quasi zero): lunghezze ossa, clearance, `hitstop_snap`, `co_motion`,
  frame contatto/FAF, lock e `source_fidelity`, `strike`, `rules`, jitter misurato (accelerazione sui frame non chiave).
- **Visivo, dopo**: forma e leggibilità della silhouette, parte che colpisce, spinta delle pose, intermedi mancanti o
  brutti, conferma del jitter, mano villain e piedi a colpo d'occhio, confronto con il fantasma della posa owner.

## Checkpoint (in ordine, uno per thread)
- **CP1** desktop Opus (brief "Rifinitura pipeline" già scritto): merge PR #24 su feature, taratura `rules`/`strike` su dair
  v4 e jab v12, guard 2, `source_fidelity` bloccante con le pose owner in `cards/overlord/refs/`.
- **CP2** desktop Sonnet: `review --visual` con le 4 immagini e il manifest (riusa `capture_ue.py` e `ingame_judge`),
  jitter numerico, `visual-gate`. Prova su jab v12, dair v2, dair v4.
- **CP3** cloud Opus: skill + rubrica + schema; calibrazione sui tre esempi; passa se coglie i difetti noti di dair v2 senza
  `blocca` falsi su jab v12 (max 2 tentativi di taratura della rubrica).
- **CP4** desktop Opus: l'interpolazione di Claude usa come gate numerico + visivo; prima mossa vera: la dair dalle nuove
  chiavi Cascadeur dell'owner (il thread di import in corso finisce come previsto, poi passa da qui).

## Costi stimati
Un giro di revisione: ~6k token di immagini + ~8k di rubrica/scheda/manifest + ~5k di risposta, Opus medium: **circa
$0,5-1**; massimo 2 giri, quindi **≤ $2 per mossa**. Cattura sul desktop: ~$0,2. Costruzione: CP1 ~$5, CP2 ~$3, CP3 ~$4,
CP4 nel costo già previsto per l'interpolazione ($1-3 a giro).

## Unica domanda per l'owner
Il thread di CP1 può fare da solo il merge di PR #24 su feature (il conflitto è solo nei documenti)? Se sì, si parte
subito con CP1 e gli altri a seguire senza altre domande.
