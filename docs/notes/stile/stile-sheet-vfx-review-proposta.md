# Proposta: aggiornamento di `stile-sheet-vfx-review.md` (2026-11-17)
**FUSA in `stile-sheet-vfx-review.md` il 2026-11-17 23:10Z** (insieme alle voci [V2] di `vfx-stile-v2-astrobot.md`). Questo file resta solo come storico.
Da `handoffs/vfx-clarity-ricerca.md` (fonti F1-F16). Da fondere nel file dopo che il round VFX sul portatile ha finito, per non cambiare le regole a metà giro. **[NUOVO]** = voce nuova; le altre restano come sono. Numeri = punti di partenza, verdetto a occhio (regola 10).

## Aggiunte alla sezione A (costruzione)
| Voce | Regola |
|---|---|
| **[NUOVO]** Gerarchia | Ogni evento VFX ha un voto di importanza 1-5 nella scheda mossa (es. polvere 1, salto 2, atterraggio 2, colpo S 3, M 4, L 5). Taglia, luminosità e durata seguono il voto; mai due eventi di voto diverso con lo stesso aspetto. Due voti per evento: per chi lo fa e per chi lo subisce (approvato dall'owner 21:04Z); l'effetto che minaccia il giocatore locale usa il voto della vittima [F9, F10] |
| **[NUOVO]** Additivo | Solo il nucleo del frame 1-2 del colpo è additivo/emissivo (> 1). Il resto opaco Masked. Niente raggi di luce o lampi a tutto schermo alla Smash [F5, A] |
| **[NUOVO]** Stessa funzione, stessa forma | Lo stesso effetto di gioco (colpo, lancio, scudo, stordimento, fuoco, elettrico) ha la stessa forma in tutti i kit. Il colore del kit cambia solo la secondaria, mai la forma. Una variante cosmetica non è mai meno leggibile della base [F9, F10] |
| **[NUOVO]** Portata onesta | L'effetto sui frame attivi non è mai più piccolo dell'hitbox né molto più grande (tolleranza indicativa ±20%) [F9] |
| **[NUOVO]** Segno a terra | Colpo su bersaglio in aria o lancio in profondità: anello/ombra a terra sotto l'impatto, così da dietro e di lato si legge dove e verso dove [F13, A] |
| **[NUOVO]** Taglia minima a schermo | L'effetto del colpo ha una taglia minima in pixel a schermo: con la camera lontana si scala, come il tremito [F1, A] |
| **[NUOVO]** Hitstop | Proiettili: hitstop ridotto. Con più di 2 giocatori: moltiplicatore di hitstop come impostazione PER MODALITÀ di gioco (dato del game mode, non project setting; owner 21:04Z: "dipende dalla modalità"). Tremito orizzontale a terra, verticale in aria, sempre rispetto allo SCHERMO. 4 frame di passaggio sussulto → dolore durante il fermo [F1, F2] |

## Sezione D (review): voci 1-12 invariate, poi
13. **[NUOVO]** Gerarchia: guardando un fermo immagine con più effetti, l'occhio va prima al colpo più importante, poi al resto? La polvere non compete mai col colpo. ("Se tutto è importante, niente lo è.")
14. **[NUOVO]** Contrasto su due sfondi: l'effetto si legge sia su fondo chiaro (cielo, pavimento chiaro) sia su fondo scuro o saturo? Cattura su due punti di L_Test_Hub.
15. **[NUOVO]** Portata onesta: con il debug delle hitbox acceso, la parte visibile dell'effetto nei frame attivi corrisponde all'hitbox (né più corta né molto più lunga)?
16. **[NUOVO]** Profondità: dalla vista dietro e a 3/4, si capisce dove ha colpito e verso dove va il lancio (segno a terra per bersagli in aria)?
17. **[NUOVO]** Camera lontana: alla distanza massima di camera prevista il colpo S si vede ancora?
18. **[NUOVO]** Hitstop e tremito: il tremito è orizzontale a terra e verticale in aria rispetto allo schermo, cala nel tempo, e l'hurtbox resta ferma?
19. **[NUOVO]** Coerenza fra kit: lo stesso effetto di gioco ha la stessa forma nel kit nuovo e in quelli esistenti?
20. **[NUOVO]** Avvio: dal frame 1 dopo l'input c'è un cambio visibile (posa o effetto) [F7]?

Etichette dei NO invariate (forma / timing / colore / gioco), più **gerarchia** per le voci 13, 17, 19.

## Processo **[NUOVO]** [F12]
La scheda mossa (`docs/notes/mosse/`) riceve due righe: voto di importanza dei suoi eventi VFX e forma prevista. Il VFX si pensa con la mossa, non dopo.
