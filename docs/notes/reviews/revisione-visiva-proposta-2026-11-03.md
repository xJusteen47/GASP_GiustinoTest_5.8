# Pipeline mosse, flusso finale con revisione visiva (2026-11-03)

Richiesta owner 03/11 02:00Z: chiudere in modo definitivo le quattro cause degli errori sulla dair, far **guardare** il
risultato a Claude (silhouette, leggibilità per parte, intermedi mancanti o venuti male, jitter) come skill riusabile,
e tenere insieme le funzioni Cascadeur nuove e le regole di risparmio (`consumi-2026-11-03.md`). Un solo percorso, già
scelto. Nessun codice qui.

## Stato di ogni pezzo
| Pezzo | Stato |
|---|---|
| Giro Cascadeur a chiavi sparse (`casc-export/import/revert/diff`, snapshot) | **fatto** su feature |
| Spacing Cascadeur = build (`casc-spacing --fit/--apply/--score`), interruttori fisica per punto (`casc_physics.py`) | **fatto** |
| Menu GSTN (Esporta, Segna chiave → `<move>_owner_keys.json`, Annulla import, Stato) | **fatto**, installato |
| Strato "Manuale", `co_motion`, `hitstop_snap`, `clearance` con esenzioni, `animpipe review` | **fatto** (review mai provato end to end) |
| Causa 1: lock `approved`, `approve/diff`, `capture` | **PR #24** |
| Causa 2: blocco `strike` (calcio non più giudicato come pugno), `rules` del README, proporzioni, pollice villain | **PR #24** |
| Causa 1: `source_fidelity` bloccante (posa owner in `cards/<kit>/refs/`, scarto max 0,5 cm / 1°), guard 2 | approvato, **da fare** (CP1) |
| Chiavi owner bloccate da sole all'import (`casc-import --owner-keys`) | approvato, **da fare** (CP4) |
| Interpolazione di Claude (`animpipe interp`, pose `origin: claude`) | approvato 02/11 23:28Z, **da fare** (CP4) |
| Cause 3 e 4: cattura su file + revisione visiva + verdetto letto dalla pipeline | **nuovo** (CP2-CP3) |

## Il flusso, dall'owner alla mossa finita
1. **Owner in Cascadeur**: mette le sue chiavi, preme "Segna chiave" su ognuna, poi "Esporta"; scrive al coordinatore
   "interpola la <mossa>".
2. **Desktop, import**: `casc-import --owner-keys`: le chiavi owner entrano intere (piedi compresi) e si bloccano;
   `source_fidelity` verifica che scheda = posa owner. Dita e `wrist_out` restano quelli della scheda.
3. **Desktop, interpolazione**: `animpipe interp` scrive le pose intermedie di Claude (con `why`) solo su frame liberi.
4. **Desktop, gate numerico** (costa quasi zero, prima del render): lunghezze ossa, clearance, `hitstop_snap`,
   `co_motion`, contatto/FAF, lock, `source_fidelity`, `strike`, `rules`, jitter misurato (accelerazione sui frame non
   chiave). Se fallisce: diagnosi scritta in 3 righe, poi **una** build. Niente render finché non passa.
5. **Desktop, cattura**: `animpipe review --visual` produce **una sola immagine** composita (lato lungo 1568 px, niente
   mp4) in `Saved/AnimPipeline/visual/<move>/r<N>/` + `manifest.json` (esiti numerici, chiavi owner/claude, segmenti).
   Pannelli: griglia laterale di ogni frame con silhouette colorate per parte e fantasma grigio della posa owner sulle
   sue chiavi; pose chiave da camera di gioco e da dietro; traiettorie di mano/piede/testa con un punto per frame;
   striscia con e senza hitstop attorno al primo frame attivo.
6. **Cloud, revisione** (Opus medium, skill `gstn-visual-review`): copia la cartella dal PC con l'accesso già approvato,
   guarda l'immagine **una volta**, scrive `verdict.json` sul PC e in project files. Non tocca la scheda.
7. **Desktop, correzione**: `animpipe visual-gate` blocca `approve` e la consegna se c'è un problema `blocca`. Si correggono
   solo pose di Claude e parti non bloccate; un problema su una chiave owner diventa `chiedi_owner`, mai una modifica.
   Dopo una diagnosi scritta, una build, poi di nuovo 4-6. **Massimo 2 giri**, poi consegna all'owner immagine + verdetto.
8. **Ritorno in Cascadeur**: `casc-export` con chiavi owner e di Claude, spacing applicato (`casc-spacing --apply`),
   interruttori fisica come scelti; "Annulla import" / `casc-revert` per tornare indietro.

## La skill `gstn-visual-review` (`.claude/skills/gstn-visual-review/`)
- `SKILL.md`: input (cartella del giro, scheda, `cards/README.md`), passi, limiti (una lettura dell'immagine, niente
  modifiche, niente loop). Usabile da qualunque thread che sappia leggere immagini.
- `rubric.md`: una voce per regola del README (frame 0, "troppo è giusto", sinergia, ginocchia avanti, rilascio lento con
  il passo grande sul primo frame attivo, niente scatto dopo l'hitbox, mano villain, compenetrazioni, piedi), più
  silhouette leggibile dalla camera di gioco, parte che colpisce riconoscibile, **intermedio mancante** (salto),
  **intermedio brutto** (dentro il corpo, ginocchio indietro, pastone), jitter. Per ogni voce: dove guardare, quando blocca.
- `verdict.schema.json`: `{move, round, esito: ok|correggi|chiedi_owner, problemi: [{regola, frame, parti, gravita:
  blocca|nota, pannello, prova, correzione: {frame, parte, direzione, quanto}}], segmenti: [{da, a, stato: ok|manca|brutto}]}`.
- `calibrazione.md`: deve dare **ok** su jab v12 e ftilt v3.1 (accettate) e **correggi** su dair v2 con i difetti visti
  dall'owner (braccia ferme, testa che scatta, avambraccio nella testa).

## Checkpoint (uno per thread, tetto 80 chiamate o 100k di contesto, handover ≤ 30 righe)
- **CP1** desktop Opus (brief "Rifinitura pipeline"): merge PR #24, taratura `rules`/`strike` su dair v4 e jab v12, guard 2,
  `source_fidelity` bloccante con le pose owner in `cards/overlord/refs/`.
- **CP2** desktop Sonnet: `review --visual` (una immagine + manifest, riusa `capture_ue.py`/`ingame_judge`), jitter
  numerico, `visual-gate`. Prova su jab v12, dair v2, dair v4.
- **CP3** cloud Opus: skill + rubrica + schema, calibrazione sui tre esempi (max 2 ritocchi della rubrica).
- **CP4** desktop Opus: `--owner-keys` + `interp` (brief "interpolazione fatta da Claude") con gate numerico + visivo;
  prima mossa vera: la dair dalle nuove chiavi dell'owner (il thread di import in corso finisce, poi passa da qui).

## Costi stimati
Revisione: ~2k token di immagine + ~8k di rubrica/scheda/manifest + ~5k di risposta, Opus medium: **circa $0,3-0,6 a
giro**, max 2 giri. Una mossa intera dalle chiavi owner: **$2-4** (oggi $10-19 per versione della dair). Costruzione:
CP1 ~$5, CP2 ~$3, CP3 ~$3, CP4 ~$4.

## Unica domanda per l'owner
Il thread di CP1 può fare da solo il merge di PR #24 su feature (conflitto solo nei documenti)? Se sì, CP1-CP4 partono
in fila senza altre domande.
