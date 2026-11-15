# Handover: ricerca silhouette multi-angolo e VFX (2026-11-15)

Thread cloud, solo ricerca e documento, nessun codice. Punto 5 dell'owner.
- Artifact (italiano): https://claude.ai/artifact/MySdFXExJmvrDviYWkatdb
- Copia: `Artifacts/Animazione/silhouette-multi-angolo-e-vfx.html` + riga in `Artifacts/README.md`.
- Branch `claude/project-thread-tvze3n` (da `claude/project-thread-ybl6a8`). Non su feature.

## Cosa dice
1. Nessuno studio risolve la silhouette da ogni angolo: si posa per una camera; le deformazioni spinte
   solo a camera controllata (Xrd, zoom finale di Smash, Overwatch, tesi Lendenfeld 2018).
2. A camera libera la lettura passa a scia, scintilla, hitstop, reazione del bersaglio, suono.
3. Posa per le fasi lunghe (carica, follow-through), VFX per le corte (frame attivi, contatto).
4. Regole di review per fase: carica veloce (<13 f = 200 ms a 62,5 Hz) solo primo frame da dietro;
   carica lenta anche fronte a 3/4; frame attivi niente silhouette; contatto congelato e
   follow-through decide dietro; lato = vista di controllo (FAIL solo per arto invisibile in posa
   tenuta, compenetrazioni, sagome uguali fra fasi). Conflitto: vince dietro, mai media.
   Due passaggi sul video: VFX spenti (pose), poi accesi (verdetto su attivi e contatto).
5. Checklist VFX di 9 voci + 3 controlli trasversali.

## Aperto (owner)
- Nei conflitti vince dietro e il lato è vista di controllo? (chiude la domanda 3 delle note Overlord)
- Cattura fronte a 3/4 per la carica delle mosse lente?

## Seguito possibile (non fatto)
- Aggiornare §8 di `anim-theory/silhouette-multidirezionali.md` dopo l'OK. Haiku medium, ~$1-2.
- Kit VFX minimo (scia, scintilla 3 taglie, tremito rispetto allo schermo): non verificato cosa
  esiste nel codice oltre all'hitstop. Va con CP4 visual hooks.

Limiti: Sakurai (hit mark, scie) e GDC di Motomura e Sheth letti solo in riassunto; nessuna fonte
dello studio per Tekken, SF6, Strive, Elden Ring, DMC5, Hi-Fi Rush; divisione per fasi = deduzione nostra.
Costo stimato: circa $8-12 (Fable, ~45 ricerche e letture web, un artifact).
