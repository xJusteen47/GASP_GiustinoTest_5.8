# Il dossier del riferimento (animazioni e VFX)

Owner 2026-11-19 ("sì, se migliora i risultati"). Metodo preso in prestito dall'idea di `video-decompiler`
(JimGHTB12, solo il README: nulla installato né eseguito). Ambito: mosse e VFX che hanno un **riferimento video**
(clip dell'owner, Smash, DK Bananza, Astro Bot). Senza un riferimento video non si fa: si giudica con i principi.
Le skill che lo usano: `gstn-anim-reviewer` (§1, §3.6, §3b) e `gstn-vfx-reviewer` (§1, §3.5, §3b). Il modello
compilabile è `dossier-template.md`. Esempio già esistente: `handoffs/dash-ganon-ref/NOTE.md` (dash di Ganondorf).

## Perché
"Lontano da Bananza", "sembra un PNG piatto": un riferimento guardato a memoria dà giudizi vaghi e chi costruisce
non sa cosa misurare. Smontare il riferimento **una volta, per iscritto** prima di costruire dà a chi anima o fa
i VFX dei bersagli concreti (quanti frame, quanto grande rispetto al personaggio, quando sparisce) e al critico un
metro che non dipende da cosa voleva fare chi ha costruito.

## Principi (non ricette)
1. **Prima del lavoro, una volta per riferimento.** Il dossier si scrive prima della build e si riusa per tutti i
   giri e per le mosse o gli effetti della stessa classe. Lo scrive un agente che non costruisce (di solito il
   revisore al primo giro, o un subagente). Se esiste già, si legge e basta.
2. **Si scrive ciò che si vede, misurato quanto il video permette.** Frame contati da 1 al primo frame che si muove,
   in frame di gioco (scrivi fps del video e del gioco e la conversione; il nostro sim gira a 62.5 Hz). Taglie in
   rapporto al personaggio (altezza o larghezza del corpo), non in pixel. Ciò che il video non permette di misurare
   si scrive come "non misurabile" con il motivo, mai stimato a occhio come se fosse un numero.
3. **Il dossier ha le parti che servono a quel riferimento**, non tutte per forza: tempi, forme e silhouette,
   taglie, movimento e curve, luce e materia, valore e colore, opacità e vita (VFX), tratti d'identità. Il template
   è una lista di cose da guardare, non un modulo da riempire.
4. **Dice anche cosa NON copiare.** Una sezione "dove il riferimento è debole o non vale per noi" (camera diversa,
   personaggio di altro peso, limiti della ripresa col telefono, stile che contraddice una regola dell'owner).
5. **Fotogrammi chiave con ffmpeg, fuori dal repo.** Le griglie del riferimento si fanno con lo stesso
   `rv_frames.py` delle review. Immagini di altri giochi restano sul desktop o in project files (`handoffs/<ref>/`),
   mai nel repo; nel repo va solo il testo del dossier.
6. **Il critico giudica contro il dossier, alla cieca.** Riceve dossier, griglie, tratti d'identità e regole
   dell'owner; non il brief completo, non le note di chi ha costruito, non il verdetto del giudice. Ogni scostamento
   dal dossier che cita è una voce con il campo del dossier e il frame.
7. **Il dossier non è legge.** Uno scostamento voluto (regola dell'owner, peso diverso del personaggio, "Ganondorf
   dentro Astro Bot") non è un difetto: il giudice lo respinge citando la regola. Cambiare un tratto d'identità
   resta una decisione dell'owner (regola 13a).
8. **Costo piccolo.** Un dossier = una lettura del riferimento (max 8 immagini, regola 4), max ~80 righe.

## Dove sta
`docs/notes/reference-dossiers/<riferimento>.md` nel repo (solo testo); griglie in project files
`handoffs/<riferimento>/`. Il brief della mossa o dell'effetto cita il dossier; il revisore lo legge per primo.
