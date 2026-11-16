# Brief M8-G0b: spike 1, baseline fighter AI (fisso, Sonnet, ~$5-8)

- Parti da: [m8-minion.md](../../architecture/plans/m8-minion.md) §0 (prestazioni), §3 G0 "Spike 1", e la spec M8 §2 e §11 (solo quelle sezioni).
- Macchina: fisso come **host listen che renderizza** (Development, `stat unit`, Insights), + 2 client; mai un host `-nullrhi`.
  Gira dopo TH-1 nel worktree m7 aggiornato a feature; l'editor dell'owner nel checkout principale resta intoccato (si chiede "editor libero" al coordinatore).
- Scenario: `Brawl` con N = 20 / 40 / 60 fighter guidati da `UGSTN_AIInputSource` (codice scratch, non committato).
  Registra ms per entità (game thread, host) e KB/s per client, a 0 % e 2 % di perdita.
- **Niente regola di stop** (owner 06/11: bersaglio hardware molto più basso del fisso, rif. Warframe su Switch 1; M8 resta completo).
  Misura anche con budget da hardware economico (frame limitato, ms ridotti) e ripeti N = 40 sul portatile; proponi i budget di M8 §11 per quel profilo.
- Risultati in `docs/notes/m8-spike-report.md` (sezione Spike 1); log in `Saved/Logs/m8-g0/`, a schermo max 5 righe.
- Max 2 tentativi per problema; contesto ≤ 100k, ≤ 80 chiamate; diagnosi scritta di 3 righe prima di ogni build.
- Niente salvataggio mappe dell'owner; mappe scratch solo nuove, salvate dall'owner se servono.
- Consegna: handover ≤ 30 righe in `docs/notes/handovers/m8-minion.md` con costo e il brief di M8-G1 (10-15 righe), che unisce i risultati di G0a.
