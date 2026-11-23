# Handover: inventario animazioni (cloud, Sonnet medium, 2026-11-23)
Branch `claude/anim-inventario-2026-11-23` da main. Solo lettura repo, un file: `docs/notes/anim-inventario.xlsx` (copia in project files `anim-inventario/anim-inventario.xlsx`).
Fogli: Inventario (92 righe: personaggio x mossa), Base gameplay P0, Gaps, Legenda.
Fonti: anim-approvate.md e milestones-stato-2026-11-21.md (via API dai branch lock/yepa62), memoria-progetto.md, handovers/, Tools/AnimPipeline/cards, tag remoti approved/*.
Cosa risulta: tag remoti solo runstart-v4r5/v4r7/w1/w2; 29w3 approvata ma tag solo locale; nessuna mossa di combattimento bloccata.
NON INIZIATI (richiesta owner 11-23): wall slide/jump (tutte le porzioni), cavalluccio, presa/pummel/lanci, oggetti in mano e attacchi, set animazioni nemici, test nemici multi-parte.
Limiti: cloud non legge .uasset (LFS): ogni 'clip esiste' e' INFERITO. Linea claude/runstart-generic-desktop non clonata (git fetch troppo lento).
Prossimo passo: script su PC che inventaria gli AnimSequence/Montage reali e confronta con il foglio; owner decide elenco personaggi e mosse kit base.
COSTO: ~2.5 $ | 0 minuti desktop | 0 token review
