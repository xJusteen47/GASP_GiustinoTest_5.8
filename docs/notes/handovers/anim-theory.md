# Handover: guida di teoria dell'animazione (2026-11-07)

Thread cloud, solo documenti. Branch `claude/project-thread-0ppwn5` da `feature/gas-npp-5.8`.

## Cosa c'è
- `docs/notes/anim-theory/principi-animazione-corporea.md`: body mechanics in dettaglio
  (timing/spacing con frame a 60 fps e 24 fps, peso e COM, anticipazione, overshoot/settle,
  overlap e secondary con la regola "ritardato, decade, finisce", moving hold, archi,
  squash/stretch con rig stretch off, silhouette, tabella errori, checklist misurabile).
- `docs/notes/anim-theory/animazione-picchiaduro-action.md`: startup/active/recovery/FAF
  mappati sui principi, posa d'impatto e hitstop, smear, leggibilità, pose tenute,
  reazioni, aerei e landing lag, budget di moto secondario, esempio dair commentato,
  checklist di revisione del video in-game attorno a hit e FAF.

## Come usarli
- Ogni thread (o Antigravity) che genera o revisiona una mossa legge i due doc prima.
- La checklist del doc 2 §10 è la rubrica della revisione video (regola 10).
- Le chiavi dell'owner vincono sempre; i numeri sono valori di partenza.

## Non fatto
- Nessuna modifica al worktree Antigravity né a `docs/notes/anim-process.md`.
- Non collegati alla skill gstn-move-animation né alla skill di revisione visiva: se
  l'owner vuole, un passo successivo è aggiungere "leggi docs/notes/anim-theory/" lì.
- Stile a pose tenute (doc 2 §5) è una scelta dell'owner, non applicata.

## Costo stimato
~$3-5 (Opus, un solo thread, nessuna ricerca web, ~60k token).
