# Review visiva VFX kit v1 (polish M2), round 1
2026-11-17. Reviewer: Fable (regola 10). Catture: ON `r8/vfx1`, OFF `r6/vfx0`, vista side e game, frame grezzi 480x480 guardati uno a uno attorno a ogni evento (crop e zoom fuori dal repo). Checklist: sezione D dello style sheet. Misure in cm stimate dall'altezza del manichino (~180 cm), colori letti a schermo.

## Verdetto complessivo: FAIL
Nessun effetto passa tutti i punti. La base è giusta: sprite opachi a bordo netto, niente traslucido, niente bagliore tranne il lampo, poche particelle, frame 1 sempre presente, moto che frena, colpo chiuso in 12-13 frame. Non sembra "Unreal generico" come materiale. I NO sono di forma (palline che si fondono in nuvola, anello mancante, artefatto a fine vita), di gioco (linee sul corpo, taglie del colpo uguali) e di colore.

## Difetto comune a tutte le palline (forma, punti 2, 6, 12)
Negli ultimi 3-6 frame di vita la pallina non sparisce pulita: le spunta una stanghetta orizzontale, poi diventa una croce o una T sottile. Visto in: salto f29-f35, atterraggio f98-f103, partenza f41-f42, scia tumble f46-f54 (qui è evidentissimo: 4-5 croci in fila). Ipotesi: celle vicine dell'atlas che entrano col mip quando lo sprite è piccolo.

## Per effetto
| Effetto | Verdetto | Cosa non passa (frame guardati) |
|---|---|---|
| Partenza corsa | FAIL | **forma** (3, tabella B): si legge 1 solo grumo di ~32 cm, non 3-4 palline spinte indietro (run_skid f28-f40). **colore** (8): grigio chiaro, vedi sotto. OK: nasce a f28, che è il frame dell'evento; vive 15 frame; muore per erosione |
| Skid | FAIL | **forma** (2, 3, 10): le palline nascono una sull'altra e fanno una nuvola lunga ~80 cm con base tagliata dal pavimento, non una fila (f134-f150). **gioco** (1): la nuvola copre i piedi (f140-f148) |
| Salto | FAIL | **gioco**: anello e palline nascono ~20 cm sopra il pavimento (l'anello bianco non è concentrico al marker nero, f22-f31): l'evento prende la posizione del personaggio già in aria. **forma**: artefatto a croce f29-f35. OK: f21 già visibile, 3 palline di 17-21 cm + anello sottile, 250 ms, più piccolo dell'atterraggio |
| Atterraggio | FAIL | **forma** (tabella B): l'anello che si allarga non si vede in nessun frame (jump f85-f95, side e game); 3 palline invece di 4-6 radiali. **forma**: artefatto f98-f103. OK: f85 già visibile, palline 23-29 cm, 19 frame, frenata chiara |
| Linee sprint | FAIL | **gioco** (1, identità "mai davanti"): dalla camera del giocatore le strisce stanno sopra gambe e bacino (sprint game f180, f186, f194, f250). **forma**: da dietro sono 1-3 bastoncini corti a terra, non linee di velocità. OK di lato: striscia bianca affusolata pulita, parte a f176 (frame dell'evento), si spegne rimpicciolendo a f283-f288 dopo il rilascio |
| Colpo S | FAIL | **colore** (8): ai frame 2-3 anello e schegge sono beige sporco (#C8C0A0 a f27, #C0B080 a f28), poi giallo senape #D0B010 e arancio #D08A00, più spenti di #FFD23C / #FF8A1E; dentro il lampo di f26 c'è una stella beige. **forma** (3): le schegge nascono dietro il corpo della vittima e restano un grumo arancione, non si contano (f30-f33). OK: lampo 1 frame a f26, il frame 1 è il più chiaro e il più pieno, tutto finito a f37 |
| Colpo M | FAIL | **gioco** (9): lampo ~45 cm e anello ~70-75 cm identici al colpo S. **colore**: come S. **forma**: 2-3 stelle visibili invece di 6 (f29-f37). Le stelle a 5 punte di f32-f33 sono belle e pulite |
| Colpo L | FAIL | **gioco** (9): lampo e anello ancora della stessa taglia, cambiano solo le stelle (3 visibili invece di 8); nessuna secondaria del kit. **colore**: come S |
| Scia tumble | FAIL | **forma** (2, 3, 10): al lancio le palline si fondono in una nuvola bianca dietro il corpo (hit_medium f34-f38, hit_large f35-f38); poi ~10 palline vive insieme, circa 1 per frame invece di 1 ogni 4-5 (f40-f46); ribbon non visibile; croci a fine vita. **gioco** (1): la scia parte in hitstop e mette una macchia bianca sul bacino della vittima (hit_large f30-f33, hit_medium f32-f33) |
| Anello knockdown | NON GIUDICABILE | In hit_large f100-f199 la vittima è fuori quadro di lato e un puntino nella vista game; dal log il knockdown cade ~190 frame dopo il colpo (circa v216), oltre la fine della cattura. Usa lo stesso NS_GSTN_LandRing dell'atterraggio, dove l'anello non si vede |

Non catturati in questo round, non giudicati: rimbalzo su muro o soffitto (NS_GSTN_Impact), splat. Punto 11 (confronto col .mov di Toad) non verificato: il riferimento non era tra i file della review.

## Colore della polvere (colore, punto 8, da verificare)
A schermo la polvere è #C9C5BF con ombra #B0AAA3: grigio chiaro quasi neutro, con il secondo tono ridotto a un filo sul bordo. Il target è #FFF6E8 con ombra #D9C7B0. Accanto al lampo (#F7F7F7) si legge grigia. Può dipendere dall'esposizione della mappa di test: va ricontrollato, ma oggi non è "bianco caldo".

## Fix in ordine
1. Artefatto a fine vita: margine di almeno 16 px tra le celle dell'atlas (o niente mip), e uccidere la pallina sotto il 30% della taglia (minimo ~6 cm) invece di portarla a zero.
2. Atterraggio: rendere visibile l'anello (60 → 140 cm in 6 frame, spessore 6 → 2 cm, 300 ms, 3 cm sopra il pavimento); 4-6 palline radiali di 22-28 cm. Knockdown: stessa ricetta a ~170 cm.
3. Taglie del colpo dalla magnitudine: S lampo 35 / anello 55 / 4 schegge da 12 cm; M 50 / 80 / 6 da 18 cm; L 70 / 115 / 8 da 26 cm più la secondaria del kit.
4. Colore del colpo: anello giallo già al frame 2 (niente passaggio beige), a schermo almeno #F0C830 e #F08020; lampo bianco pieno senza stella scura dentro.
5. Schegge: ventaglio di ~50° attorno alla spinta, 500-700 cm/s con drag 6, origine ~20 cm verso la camera, così si separano e si contano entro il frame 3.
6. Skid: 1 pallina ogni 35-40 cm percorsi (o 1 ogni 4 frame con spinta indietro di ~200 cm/s), 14-18 cm, al massimo 4-5 vive, mai sovrapposte oltre il 30%.
7. Partenza corsa: 3-4 palline separate (20 / 16 / 12 cm), 150-350 cm/s all'indietro in un ventaglio di 60°, drag 6, 300 ms.
8. Salto: agganciare anello e palline al pavimento (ultima posizione a terra), non alla posizione in aria.
9. Linee sprint: solo ai lati, ad almeno 50-80 cm dall'asse del personaggio (mai sotto 45 cm), altezza 60-150 cm, 3-5 vive, lunghe 120-160 cm, spesse 4-5 cm.
10. Scia tumble: parte al frame del lancio, non in hitstop; 1 pallina ogni 4-5 frame, 18 cm, 300 ms; aggiungere il ribbon corto o toglierlo dalla tabella.
11. Polvere: a schermo almeno #F0E8DA con ombra ~#D0BEA8 sul 25-30% della pallina.

## Per il round 2
Stessa cattura più: camera che segue la vittima fino al knockdown (o un muro che la ferma), una sequenza con muro per l'impatto, e la vista game più vicina (oggi il personaggio è alto ~45 px nelle sequenze salto e colpo).
