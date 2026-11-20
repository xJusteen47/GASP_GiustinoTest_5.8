# Review run start v4r6 (variante di v4r5, bloccata)

Richiesta owner (2026-11-20): run start "un po' più alzato e meno in posa di sprint"; la v4r5 resta come versione sicura.

Cosa cambia in v4r6 rispetto a v4r5 (stesse gambe, piedi, bacino, root, timing; solo busto/testa/braccia):
- tutto il corpo sopra spine_02 (petto, collo, testa, braccia) ruota all'indietro attorno a spine_02 di 15° (10° al frame 0, 15° da f3, sfuma a 0 tra f29 e f45 della clip); la testa ri-annuisce in avanti del 75 % di quell'angolo, quindi lo sguardo resta avanti con il mento un po' più su;
- inclinazione spine_02->collo: v4r5 50-53° di picco, v4r6 35-38°; coda (loop stock) invariata 30°.
- curve: movedata_speed e enable_warping 0->1 nel primo 10 % come gli start stock, phase nella nostra convenzione (A/B fatto a parte).

Tratti d'identità (regola 13a, da controllare PER PRIMI):
1. rev-up frenetico dal frame 1 (stile Mario / Astro Bot): braccia che pompano forte, passi rapidi;
2. stile bruto Overlord (Ganondorf in Astro Bot): pesante, minaccioso, gomiti fuori;
3. niente primo passo piantato, rampa di accelerazione continua.

Materiale (cattura in gioco, 1 giocatore, gstn.Loco.Base 1, ASSERT: OK f95-170, peso >= 0,8 sul 93 %):
- griglie per vista, riga sopra v4r5, riga sotto v4r6, stessi frame di cattura f92-188 ogni 6: grid_az90.png (lato), grid_az0.png, grid_game.png (camera di gioco);
- video v4r6: runstart_v4r6.mp4.
- numeri: scivolamento piedi appoggiati f80-240 (cm, euristica) v4r5 L36,4 R33,4, v4r6 L30,4 R43,4; clearance braccia-corpo min +1,8 cm (come v4r4/v4r5).

Domande: giudica prima la vita (anticipazione, archi, peso, follow-through, root coerente con la capsula) guardando le immagini senza numeri, poi i tratti d'identità, poi i check tecnici. Rispondi esplicitamente alle due domande dell'owner: "è della stessa qualità di un gioco action stilizzato alla Smash Bros?" e "è alla pari di Smash Bros o altri tripla A simili?". Risultato: PASS o FAIL, ogni voce di FAIL etichettata posa / dati montage / gioco, con frame.
