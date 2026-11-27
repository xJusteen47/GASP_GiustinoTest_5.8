# Handover mmo-scalabilita-piano: piano dei limiti di scala della sezione MMO (2026-11-25)

Branch `claude/project-thread-aglpry`, base `main` 152d25e6. Cloud, solo documento, nessun codice, niente da compilare.

## Cosa
- `docs/architecture/plans/mmo-scalabilita.md`: risposta alla domanda dell'owner (il server non risimula mai, il rollback è solo sui
  client; "ibrido" = modalità Scalato del piano rollback, non il misto vietato), CPU del server (3 modelli NP per giocatore a 62,5 Hz,
  leve in ordine: tetto per istanza, percorso a riposo, tick ridotto solo con patch al fork se la misura lo chiede, istanze con
  passaggio), CPU del rollback sui client (LOD per entità di combat.md §12.7), rete (rilevanza a budget e fasce per i giocatori,
  giocatori lontani come voci a lotti come i minion), test di carico 20-50 client headless con bot a input virtuali, quando partire,
  fasi S0-S7 con costi, 3 decisioni a lettera per quando partirà.
- Riga aggiunta in `docs/architecture/plans/README.md`. Copia in project files `rete/mmo-scalabilita-piano.md`.

## Da notare
- Tutti i numeri di CPU e byte dei giocatori sono stime [E]: il costo per giocatore parte dal dato S-FP (0,03-0,04 ms per entità per
  frame, combat.md §12.7). Li sostituisce la fase S3.
- Il link a `tag-verbi-simulazione-rollback.md` funziona solo dopo il merge della PR #55.

ESITO: PASS piano scritto, in attesa di lettura dell'owner; nessuna decisione richiesta ora
SUBAGENTI: 0
COSTO: ~2 $ | 0 | 0
