# Handover: ricerca silhouette multi-angolo e VFX (2026-11-15)

Thread cloud, solo ricerca e documento. Nessun codice toccato. Punto 5 dell'owner (cmsg ...G9iwTaGrnDyd5cxt7ZtRVF).

## Consegnato
- Artifact (italiano): https://claude.ai/artifact/MySdFXExJmvrDviYWkatdb
- Copia: `Artifacts/Animazione/silhouette-multi-angolo-e-vfx.html`, riga aggiunta in `Artifacts/README.md`.
- Branch `claude/project-thread-tvze3n` (da `claude/project-thread-ybl6a8`). Non su feature.

## Cosa dice
1. Nessuno studio risolve la silhouette da ogni angolo: si posa per una camera e le deformazioni spinte
   si usano solo a camera controllata (Xrd, Smash zoom finale, Overwatch, tesi Lendenfeld 2018).
2. A camera libera la lettura passa a scia, scintilla, hitstop, reazione del bersaglio, suono.
3. Posa per le fasi lunghe (carica, follow-through), VFX per quelle corte (frame attivi, contatto).
4. Regole di review proposte, per fase: carica veloce (<13 f = 200 ms a 62,5 Hz) solo primo frame da
   dietro; carica lenta anche fronte a 3/4; frame attivi non si giudica la silhouette; contatto
   congelato e follow-through decide dietro; lato = vista di controllo (FAIL solo per arto invisibile
   in posa tenuta, compenetrazioni, sagome uguali fra fasi). Conflitto: vince dietro, mai media.
   Review in due passaggi: VFX spenti (pose) poi accesi (verdetto su attivi e contatto).
5. Checklist VFX di 9 voci + 3 controlli trasversali (effetto non copre, taglia segue importanza,
   ogni effetto è informazione).

## Aperto (owner)
- Conferma: nei conflitti vince dietro, lato vista di controllo? (chiude la domanda 3 di
  overlord-silhouette-viste-2026-11-12.md)
- Aggiungere la cattura fronte a 3/4 per la carica delle mosse lente?

## Seguito possibile (non fatto)
- Aggiornare §8 di `docs/notes/anim-theory/silhouette-multidirezionali.md` con la tabella per fasi,
  dopo l'OK dell'owner. Doc semplice, Haiku medium, ~$1-2.
- Kit VFX minimo (scia, scintilla 3 taglie, tremito bersaglio rispetto allo schermo): non verificato
  cosa esiste nel codice oltre all'hitstop. Va con CP4 visual hooks.

## Limiti
- Sakurai (hit mark, scie) e talk GDC di Motomura e Sheth letti solo in riassunto.
- Nessuna fonte dello studio per Tekken, SF6, Strive, Elden Ring, DMC5, Hi-Fi Rush su questo tema.
- Divisione per fasi e fronte a 3/4 sono deduzione nostra: da provare in gioco.

Costo stimato: circa $8-12 (Fable, ~45 ricerche e letture web, un artifact).
