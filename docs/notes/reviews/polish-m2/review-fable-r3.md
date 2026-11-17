# Review visiva VFX kit v1 (polish M2), round 3 (ultimo)
2026-11-17. Reviewer: Fable (regola 10). Catture: `r3a2` e `r3b2`, ON `vfx1`, OFF `vfx0`. Guardate una volta le 16 griglie off/on di `r3/` e la griglia di Toad; in più 8 montaggi miei dai frame grezzi ON (crop e zoom 2x-3x, fuori dal repo) e una lettura di colore per script, usata solo come indizio. Misure in cm dal manichino (~180 cm = ~132 px nella vista side). Checklist: sezione D dello style sheet. I frame sono i frame di cattura (v).

## Verdetto complessivo: FAIL (di misura)
Il nucleo del kit adesso passa: colpo S/M/L, partenza corsa, skid, salto, atterraggio, linee sprint, scia tumble. Le schegge escono davvero a ventaglio e frenano, le tre taglie si contano (4/6/8), il rosso è solo del colpo forte, la polvere è crema calda. Non passa la parte "palline" degli effetti d'impatto (`NS_GSTN_Impact`: splat, rimbalzo muro, terra, soffitto, bump) e dell'anello di knockdown: le palline nascono a grumo e stanno sopra il corpo per 6-10 frame (punti 1 e 3). Il bump muro non è nella cattura, quindi non è giudicabile. Con questi NO la regola "PASS solo se tutte sì" non è soddisfatta.

## Per effetto
| Effetto | Verdetto | Note (frame guardati) |
|---|---|---|
| Partenza corsa | PASS (con nota) | run_skid side v28-v43, zoom 3x. 4 palline in 2 coppie (grande ~22 cm che sale, piccola ~12 cm bassa), visibili da v28, frenano e sono ferme da ~v36, muoiono rimpicciolendo entro v43 (16 frame). Nota **forma** (3): fino a v35 le due palline di ogni coppia sono sovrapposte ("pupazzo di neve"), si leggono 2 forme e non 4. Punto 11: stessa famiglia di Toad (sbuffo chiaro tondo dietro i piedi, vita breve); Toad ne ha uno solo e più grande |
| Skid | PASS (con nota) | run_skid side v136-v166, zoom 3x. 1 pallina a v138-v148, poi fila di 3-4 da ~15 cm (v150-v160), niente più lunetta a terra né grumo a fine corsa, morte pulita (v162-v164). Nota **forma** (1): a v150-v152 due palline sono davanti allo stinco. Nota di cattura: qui la camera non è di lato, la fila corre in profondità |
| Salto | PASS | jump side v21-v35. 3 palline + anello sottile a terra, tutto chiuso in 14 frame. Più piccolo dell'atterraggio |
| Atterraggio | PASS (con nota) | jump side v84-v100, zoom 3x. Anello pieno da v84, si allarga fino a ~190 cm (v88), poi si assottiglia e si chiude a v98: 15 frame (250 ms). 6 palline radiali, morte pulita a v100. Note **forma**: l'anello è spesso (~25 cm) e più grande dell'anello del colpo forte (~100 cm), contro la scala "atterraggio < colpo forte"; a fine vita si restringe verso il centro invece di assottigliarsi sul posto; le palline sono 3 coppie sovrapposte; 1 pallina resta davanti al piede (v86-v92) |
| Linee sprint | PASS | sprint close v180-v280. 3-4 strisce bianche affusolate ai lati, mai sul corpo |
| Colpo S | PASS | hit_small side v25-v36 (zoom 2x) e close. Lampo bianco 1 frame, anello giallo, 4 stelle separate già a v26 che frenano (v26-v32) e finiscono in arancio; chiuso in 12 frame; nessun rosso. Note: a v25 le stelle si vedono, chiare, sul bordo del lampo (non sono nascoste come dichiarato, ma non disturbano); a v26-v28 2-3 stelle passano sul busto della vittima |
| Colpo M | PASS | hit_medium side v26-v37. 6 stelle, ventaglio più ampio, anello ~90 cm; più grande di S a colpo d'occhio; nessun rosso |
| Colpo L | PASS (con nota) | hit_large side v26-v37 e close v26-v38. 8 stelle gialle + stelle rosse, anello ~100 cm; le tre taglie si distinguono (punto 9). Dalla camera del giocatore ora le stelle escono dalla sagoma dell'attaccante e il colpo si legge. Nota **forma**: a v27-v31 3-4 stelle (rosse e gialle) restano sovrapposte vicino alla testa della vittima |
| Scia tumble | PASS | hit_medium v38-v51, hit_large v36-v51 e v186-v213. 4-5 palline in fila da ~18 cm, una ogni ~60 cm, disco pulito, si spengono rimpicciolendo |
| Anello knockdown | FAIL (lieve) | hit_large side v198-v230, zoom 2x. Tempo ora giusto: l'anello parte a v214, quando il corpo è a terra (prima era ~8-10 frame in anticipo); dura 16 frame. **forma** (1): 4 delle 6 palline da ~30 cm stanno sopra busto, anche e testa del corpo sdraiato per v215-v224; l'anello spesso passa sotto e intorno. Il raggio di nascita di 38 cm cade dentro un corpo lungo 180 cm |
| Splat | PASS (con nota) | splat side v48-v62. Anello sottile sul piano del muro che si allarga e si erode, 4-5 palline da ~40 cm. Nota **forma** (1, 3): come al round 2, le palline nascono sovrapposte (2-3 fuse) e coprono cosce e bacino a v48-v54 |
| Rimbalzo muro | PASS (con nota) | wall side v40-v50. Anello sottile sul muro, 6-7 palline che frenano; bello da v44 in poi. Nota **forma** (1, 3): a v40-v43 le palline sono a grumo su gambe e bacino, e insieme alle palline della scia (stesso colore) si arriva a ~10 palline a schermo |
| Rimbalzo terra | FAIL (lieve) | ground_bounce side v46-v56, zoom. Anello sottile a terra che si allarga: bene. **forma** (3, 7, 1): le 3-4 palline restano un grumo fuso, quasi ferme da v47 a v56, e stanno davanti al busto dell'attaccante; non si aprono radiali sul piano |
| Soffitto | FAIL (lieve) | ceiling side v38-v48. Anello sottile sul piano del soffitto: bene. **forma** (3, 1): 6-7 palline in fila fusa ("bruco" di 4 a v41-v46), a v38-v39 davanti al busto. **gioco**: l'urto col soffitto arriva come evento WallBounce |
| Bump muro | NON GIUDICABILE | L'evento `Bump.Wall` è al frame di simulazione 2969 (~v176) ma la sequenza wall_bump ha 110 frame. In quella sequenza si vede altro: 2 rimbalzi muro a 6 frame di distanza (v55, v61) più un secondo colpo a v47: 2 anelli e 12+ palline sopra il corpo (v58-v64), **gioco**. Visto solo il bump soffitto (ceiling v57-v68, taglia 0,4): anello piccolo + grumo di palline sul petto per ~10 frame, **forma** (1); lì nello stesso frame parte anche un WallBounce (**gioco**), quindi la taglia 0,4 non si può confermare |

Colore (indizio da script): polvere #F0E8D8 con ombra ~#D8C0B0; schegge #F8C818 → #F8A808; rosso #F84018 solo nel colpo L. A occhio: polvere neutra calda e colpo caldo saturo si distinguono bene (punto 8). Punto 12: non sembra "Unreal generico".

## Stato dei 9 fix del round 2
| # | Fix | Stato |
|---|---|---|
| 1 | Schegge fuori dal grumo entro il frame 3 | CHIUSO: separate già al frame 2, frenano; resta un piccolo grumo di 3-4 stelle nel colpo L |
| 2 | Rosso solo dal colpo forte, niente stella crema | CHIUSO per il rosso. Stelle chiare ancora visibili sul frame del lampo (accettabile) |
| 3 | Partenza corsa: 4 palline separate, drag reale | QUASI: frenano e si fermano; 2 coppie sovrapposte per 8 frame |
| 4 | Anello atterraggio 15-18 frame, palline non sui piedi | QUASI: durata giusta; anello spesso che si restringe; 1 pallina sul piede. Per lo splat: APERTO (palline ancora sulle cosce) |
| 5 | Knockdown a tempo col corpo | CHIUSO per il tempo; ora le palline coprono il corpo |
| 6 | Polvere chiara e calda, secondo tono 25-30% | CHIUSO per il colore; l'ombra a schermo è ancora poca (~10-15%), le palline sembrano quasi piatte |
| 7 | Skid per distanza, palline alzate | CHIUSO |
| 8 | Colpo dalla camera del giocatore | CHIUSO |
| 9 | Scia per distanza | CHIUSO |

## Cosa deve decidere l'owner (non è previsto un altro round)
1. Accettare il nucleo così com'è (partenza, skid, salto, atterraggio, sprint, colpo S/M/L, scia): per me è pronto da vedere.
2. Impatti e knockdown: tenerli come sono per la v1 (anelli buoni, palline a grumo sul corpo) oppure concedere un solo giro mirato sulle palline: nascita più larga e spinta radiale sul piano, fuori dalla sagoma (knockdown: raggio ~90 cm o solo ai lati del corpo), senza toccare gli anelli.
3. Anello di atterraggio/knockdown: va bene così spesso e più grande dell'anello del colpo forte, o va riportato sotto (più sottile, ~130 cm)?
4. Bump muro/soffitto a taglia 0,4: non confermabile da questa cattura; serve una cattura che arrivi a ~v180 o una prova in gioco.
5. Coda codice (gioco, non conta per i VFX): rimbalzi muro doppi a 6 frame di distanza, bump e WallBounce nello stesso frame, urto col soffitto segnalato come WallBounce.
