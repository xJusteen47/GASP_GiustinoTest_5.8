# Brief M8-G0a: audit S0 + spike 2 (portatile, Opus medium, ~$4-7)

- Parti da: [m8-minion.md](../../architecture/plans/m8-minion.md) §1 (righe X1..X14), §3 G0, §4; [sequenza-2026-11-06.md](../../architecture/plans/sequenza-2026-11-06.md). Nient'altro.
- Prerequisito: passo 0 della sequenza fatto (feature con M7 + SD unito). Checkout del portatile su feature aggiornato, nessun thread SD aperto.
- **S0**: ricontrolla ogni riga X1..X14 sull'HEAD di partenza (scrivi l'hash). Una riga peggiore di come è scritta = stop, scrivi cosa serve.
- **Spike 2**: trova in `UNetworkPredictionWorldManager` un punto dopo tutti i modelli del frame F e la mappa tempo→frame di `RewindActors`.
  Se non c'è, scrivi il testo della patch (inerte se non collegata) per G1. Errore ≤ 1 frame, misurato con un test scratch headless.
- Solo codice scratch (non committato su feature): risultati in `docs/notes/m8-spike-report.md` e combat.md §17.
- Gate: report con numeri e hash; build headless verde se tocchi codice scratch; output delle macchine su file (max 5 righe a schermo).
- Max 2 tentativi per ogni problema, poi fermati. Contesto ≤ 100k, ≤ 80 chiamate.
- Consegna: commit doc su un branch del thread, handover ≤ 30 righe in `docs/notes/handovers/m8-minion.md` (nuovo) con costo,
  e il brief di M8-G1 (10-15 righe) se G0b è già chiuso, altrimenti la nota "G1 attende G0b".
