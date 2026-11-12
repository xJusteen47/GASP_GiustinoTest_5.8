# Handover: silhouette leggibili da dietro e di lato (2026-11-12)

Thread cloud, solo documenti. Branch `claude/project-thread-ujyw42` da `feature/gas-npp-5.8`.
Risposta dell'owner (10:14Z): giudica le mosse **soprattutto da dietro e di lato**.

## Cosa c'è
- `docs/notes/anim-theory/silhouette-multidirezionali.md`: terzo doc di teoria (solo generale).
  Cosa vede ogni vista, regole di posa e di movimento, aiuti indipendenti dalla vista, soluzioni
  di camera e di regole, checklist animatore e revisore (passa solo se reggono TUTTE E DUE le viste).
- `docs/notes/reviews/overlord-silhouette-viste-2026-11-12.md`: applicazione a jab 1, jab 2 e
  down-air dell'Overlord, con 3 domande non bloccanti per l'owner.
- Copie nei project files: `anim-theory/`.

## Cosa c'era già
Doc 1 §11 e doc 2 §2.2, §3, §4 dicono che la parte che colpisce deve vedersi da ogni camera, ma
non come. La griglia di `review --visual` e `legibility.py` hanno già la vista da dietro. Il
pilota del jab (10-31) aveva già trovato il rimedio pratico (linea del pugno in fuori).

## Scelte mie
- Le note sulle mosse stanno FUORI dalla cartella di teoria (regola owner 11-07: nelle guide
  solo teoria generale). Il brief le chiedeva nello stesso file.
- Le regole su diagonali, verticale e archi inclinati sono dedotte dalla geometria (marcate
  [geom]); le fonti coprono una camera alla volta. Vanno confermate in gioco.
- Non ho guardato catture: i suggerimenti sull'Overlord vengono dagli handover.

## Per i prossimi thread di animazione
- Leggere il doc 3 insieme agli altri due; mettere nel brief la nota sull'Overlord.
- La revisione chiede due riprese PIE della stessa mossa (camera di gioco dietro e laterale).
  Verificare se la cattura in gioco ha già la laterale; se no, aggiungerla prima del giro.
- Jab 1: il piano v3 (petto a 80-90°, braccio dritto avanti) è la causa del braccio che sparisce.

## Non fatto
Nessuna animazione, nessun editor, nessun asset. Skill `animpipe-move` e rubrica non toccate
(passo possibile: aggiungere alla rubrica la voce "due viste").

Costo stimato: ~75k token di contesto (Fable), 9 ricerche e 25 letture web, nessun subagente.
Artifact "Cose da fare a mano": nessun passo manuale nuovo.
