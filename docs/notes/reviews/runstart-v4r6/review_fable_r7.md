# Review run start v4r6 (fable r7) — PASS con riserve

Materiale guardato: brief, grid_az90 / grid_az0 / grid_game (v4r5 sopra, v4r6 sotto, f92-188 ogni 6),
più 8 frame di runstart_v4r6.mp4 (cattura v072, v080, v088, v096, v106, v118, v132, v150; video n = cattura - 60).
Limite: campioni ogni 6-18 frame, quindi un pop di 1-2 frame non si può escludere da qui (vedi "Non verificato").

## Vita
- Anticipazione: c'è e resta leggibile. v088 peso che scende, v096-v106 corpo raccolto e basso con la nuvola di polvere, poi rilascio. Uguale a v4r5.
- Peso: tra v104 e v146 il petto di v4r6 è chiaramente più verticale di v4r5, ma resta davanti al bacino: non vedo il busto che va all'indietro mentre accelera. Il punto più dritto è v128-v140 (quasi verticale di lato): è il limite, oltre diventa senza peso.
- Archi delle braccia: la pompata resta ampia e alternata (v118, v132, v150), gomiti fuori in az0. Nessun braccio fuori arco visibile nei campioni.
- Follow-through: da v152 in poi le due righe sono di fatto uguali (coda stock), nessuna differenza di posa a v164-v188.
- Root e capsula: in grid_game il personaggio avanza in modo continuo, stessa traiettoria nelle due righe.

## Tratti d'identità
1. Rev-up frenetico dal frame 1: tenuto. Braccia già in pompata a v096-v106, passi rapidi.
2. Bruto Overlord: tenuto, anzi il petto più alto e i gomiti fuori leggono più "spalle larghe" in az0 (v110-v134). Perde un po' di carica da toro di lato a v128-v140.
3. Niente primo passo piantato: tenuto, gambe identiche a v4r5.

## Domanda chiave dell'owner
Sì: v4r6 è visibilmente più alzato e meno in posa di sprint di v4r5, soprattutto in vista laterale v104-v146
(v4r5 ha la schiena quasi a 45-50°, v4r6 sta sui 30-35° a occhio). In camera di gioco la differenza è piccola:
il personaggio è minuscolo in grid_game, si nota appena.

## Riserve (non bloccanti)
- posa, v096 (clip f~5): in az45/az90 la testa sembra con il mento alto mentre il corpo è raccolto. A questa risoluzione non è certo; da riguardare a piena dimensione. Se confermato, alzare il ri-annuire dal 75 % verso il 90 % solo nei primi 10 frame.
- posa, v128-v152: l'inclinazione del busto non è monotona. v4r6 è quasi verticale a v134-v140 e poi si piega di nuovo in avanti a v146-v158, mentre la variante sfuma. Non è un pop nei campioni, ma letto di fila può sembrare un "ripensamento". Possibile cura: sfumare più tardi o più a lungo (f35-f55).
- gioco, f80-240: scivolamento piede destro 43,4 cm contro 33,4 di v4r5 con gambe dichiarate identiche. O l'euristica è rumorosa o le due catture non sono allineate: da chiarire prima di usare quel numero.

## Non verificato
- Pop a inizio clip (v088-v094) e durante la sfumata f29-45 (circa v120-v137): nei campioni v122, v128, v132, v134, v140 la progressione è liscia, ma serve guardare il video frame per frame per chiuderlo.
- Clearance braccia-corpo: preso dal brief (+1,8 cm), non giudicabile dalle griglie.

## Le due domande dell'owner
- "Stessa qualità di un action stilizzato alla Smash?" Come lettura della mossa sì: anticipazione chiara, silhouette forte, carattere riconoscibile. v4r6 non peggiora v4r5.
- "Alla pari di Smash o tripla A simili?" Non ancora. Mancano: piedi che scivolano 30-43 cm, testa e mani poco caratterizzate nei primi frame, transizione verso il loop stock che appiattisce il personaggio da v152 in poi. Questo vale uguale per v4r5.

Risultato: PASS come variante accanto a v4r5 (che resta la versione sicura), con le tre riserve sopra da controllare.

## Addendum (thread, dopo la review)
- Riserva "piede destro 43,4 vs 33,4": causa = set di curve stock (movedata_speed + rampa enable_warping), non la posa: v4r5d (v4r5 + stesse curve) dava lo stesso R 43,4. v4r6 finale ha il set di curve della v4r5 (kind start). Ricattura Saved/RunStartV6/v4r6s: ASSERT: OK f95-170 93 %, piedi L36,5 R33,2 (v4r5 L36,4 R33,4). Griglie e mp4 di questa cartella rifatti su questa cattura.
- Pop f29-45: la correzione sfuma con smoothstep su 16 frame a 30 fps (15° in 0,53 s); a inizio clip l'ingresso è in blend dall'idle.
