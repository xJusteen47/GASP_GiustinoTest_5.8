# Handover: pianificazione delle milestone restanti (cloud, solo doc, 2026-11-06)

- Branch `claude/project-thread-uun5p8`, base `claude/project-thread-4kdr4u` (PR #29, sequenza): la PR nuova contiene anche i commit di #29.
- Audit su feature `60da6fa`: M3 C0-C19 fatti (aperti: suite non stabile 2 volte, harness lane, T3, O-CameraYaw, canary); M4b P0-P9,
  P7b, P13, P14 fatti; M5 E1-E14 fatti; T1, T3, T5 fatti, T2 in pausa. D-1 e `ServerRespawn` esistono; niente Game Features nel `.uproject`;
  `GSTNInteract` e `GSTNCamera` sperimentali, mai su un pawn vero.
- Piani nuovi in `docs/architecture/plans/`: `resto-m3-m4b-m5-t.md` ($63-109 ora + $17-31 dopo M8), `fp-forward-prediction.md`
  ($30-51, o $4-7 se G0 chiude), `m11-kit.md` ($53-92), `m12-modi-livelli.md` ($70-120), `m14-pvp.md` ($28-46), `m13-spedizione.md`
  ($59-102). README dei piani aggiornato.
- `sequenza-2026-11-06.md`: tabella A-I con cumulativo da oggi **$515-861** (incl. M8 e M10), conflitti di file, decisioni D5-D10.
- Decisioni per l'owner: D5 Game Features/Experiences (architettura, serve il sì prima di M12 G1); D6-D10 hanno default applicati.
- Default scelti (regola 6): drop del kit alla morte ON in Livelli e Versus (respawn con il kit della lobby); vite 4, ricarica 20 s;
  P10 solo feedback minimo; E18 mai; FP con regola di stop sullo spike 4.
- Prossimo: brief per il blocco B (R3-1 per primo, Sonnet portatile, $5-8) quando c'è budget e una macchina libera da M8;
  brief M11 G0 solo dopo il merge di TH-1b (`kizkcl`) su feature.
- Non letti per risparmio: spec complete di interaction/camera/gravity/activation (solo titoli e stato); i costi sono stime grezze
  sulla scala M6/M7 (S $3-6, M $6-10, L $10-18).
- Costo di questo thread: ~$4-6 (Opus, ~90k token di contesto, nessuna macchina usata).
