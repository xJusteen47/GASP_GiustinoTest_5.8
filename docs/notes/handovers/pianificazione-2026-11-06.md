# Handover: pianificazione 2026-11-06 (cloud, Opus medium, solo doc)

- Richiesta owner (15:22Z): pianificare le prossime milestone e la loro implementazione dopo M7-15 e SD-16; poi un livello di test giocabile.
- Consegnato su `claude/project-thread-4kdr4u` (PR doc-only verso feature):
  - `docs/architecture/plans/sequenza-2026-11-06.md`: passo 0 di consolidamento, 18 righe di checkpoint (TH → M8 G0-G12 → FP → M10),
    macchina/modello/dipendenze/costo, regola dei 2 thread, residui M7, domande D1-D4 in un blocco.
  - Brief: `test-level-brief.md` (TH-1), `m8-g0a-brief.md` (portatile), `m8-g0b-brief.md` (fisso).
- Scelte applicate (regola 6): mappa di test unica `L_Test_Hub` (le mappe esistenti hanno pavimento/PlayerStart/GameMode nello stesso punto,
  quindi niente sotto-livelli; lo script importa i builder di `L_Test_Combat` e `L_WallSlideAB`); cambio kit via helper Python (D1);
  M8 G0 diviso in G0a (audit + spike 2, portatile) e G0b (spike 1, fisso) per usare entrambe le macchine; FP dopo M8 come da owner (D3).
- Fonti: piani su branch non ancora uniti (M8 PR #26 `claude/project-thread-b6loil`, M10 PR #27 `claude/project-thread-s4qjx0`); stato M7 da
  `docs/notes/handovers/m7-piggyback.md` su `claude/project-thread-7autp9`. I link relativi a m8-minion.md e m10-held.md funzionano dopo il merge di #26/#27.
- Aperto: risposte D1-D4; il passo 0 (merge M7 + SD su feature) richiede il push dell'owner dal fisso.
- Note per TH-1: non esiste un manichino bersaglio (si usano i client PIE e il test fighter delle piattaforme); i kit di mount/spin sono creati
  in memoria dai `prepare_editor()` degli scenari, non salvati; non verificato se le mosse animate (AS_ in /Game/AnimPipeline) sono legate a un kit.
- Costo stimato del thread: ~120k token di contesto, ~$2-3.
