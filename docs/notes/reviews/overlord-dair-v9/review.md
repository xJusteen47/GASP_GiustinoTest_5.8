# Review: overlord_dair v9 contro v8 — Verdetto: FAIL di misura (la clip passa; resta uno scatto delle gambe nell'uscita in aria, fuori dalle chiavi) — 2026-11-12
Guardate una volta le 11 griglie `cmp_*.jpg` (v8 sopra, v9 sotto). Misure sui frame grezzi di `capture_v9_r1` e `capture_v8_r5`: pixel del personaggio cambiati tra due frame video consecutivi (ritaglio attorno al personaggio, scala 360 px, soglia 20) e bordo basso della sagoma (px su 480).
Riferimento: un frame del colpo vale 3900-4500 px di lato e da dietro, 2600-2800 in camera di gioco. `a` = frame di animazione, `f` = frame della mossa nelle griglie, `v` = frame video.
**Catena di cattura: valida.** Le gambe seguono l'animazione (palla, gambe tese, ginocchia che cedono, gambe raccolte), ci sono lato, dietro, camera di gioco e front34 per i tre casi. Un frame video ogni 24 contiene due tick di simulazione (62,5 contro 60): in v9 a v48 e v72 (a vuoto), v98 (a segno), v102 (atterraggio); in v8 cade su altri frame, quindi i confronti su quei singoli frame vanno presi con cautela.
**Limite:** lo scatto del punto 3 è visto in una sola cattura (a vuoto); non ho aperto l'editor, quindi la causa non è accertata.

## 1. Dopo l'impatto (a16-a27)
DIETRO / gioco: **non è più un fermo immagine.** Da a16.6 a a22.9 il cambio medio è 830 px per frame (v8 328), cioè 2,5 volte; in camera di gioco 1019 contro 699. Il bordo basso sale di 11-12 px a a19.7-a20.8 e torna giù a a22.9 (v8: fermo entro 2 px); in camera di gioco sono circa 10 px su 480, circa il 5% dell'altezza del personaggio: piccolo ma si vede.
Da dietro le ginocchia si aprono a rombo (griglia `cmp_strike_behind`, f16-f21): la colonna sotto il bacino si rompe da sola dopo i frame attivi.
LATO: **non è più un fermo immagine.** Cambio medio 927 px per frame (v8 465). Bordo basso su di 13 px a a19.7, giù a a22.9, poi altri 4 px sotto il livello di partenza fino a a26 (i piedi premono). Le ginocchia avanzano di circa 11 px e restano piegate.
Peso: una sola oscillazione, 4 frame tra salita e discesa, poi il corpo affonda: legge come urto, rinculo, peso che scende. Non è nervoso e non è troppo grande. Le gambe restano tese fino a a17, quindi i frame attivi mostrano ancora il pestone dell'owner.
Riserva di gusto, non bloccante: da dietro le ginocchia aperte restano fino a a26; se all'owner sembrano gambe a O, basta ridurre di un terzo l'apertura in fuori.

## 2. Fine tenuta e inversione (a24-a28)
Frame morti: **nessuno**. Restano 3 transizioni lente, a23.9→a27.0: 133-215 px di lato, 126-165 da dietro (3-5% del colpo). In v8 erano 5 transizioni sotto 220 px, due delle quali a 44-56 di lato e 18-34 da dietro, dopo altre 6 quasi ferme.
In camera di gioco c'è una sola transizione a 74 px (a27→a28, all'apice del salto), con 260-320 px prima e dopo. Tre frame lenti (50 ms) leggono come assestamento, non come blocco: va bene in tutte e due le viste.

## 3. Recovery (a26-a42)
La clip ora è giusta. Di lato il cambio per frame da a27 è 599, 1403, 2274, 3020, 3214, 3098, 2522, 1234: accelera fino a a32 e rallenta prima del blend-out (v8 ci arrivava a 3569 e in salita). Sequenza leggibile da dietro: ginocchia (f27-f29), busto (f29-f31), braccia per ultime (da f34). Niente che galleggia.
Lo scatto a f38-f39 non c'è più in quel punto: da f35 a f42 il cambio scende regolare da 2377 a 1129 di lato.
**Ma a vuoto c'è uno scatto nuovo a f33→f34 (v70→v71), due frame video dopo l'inizio del blend-out (montage fuori da v69, a34.3).** In un solo frame le gambe passano da raccolte dietro a seduta in avanti e le braccia si aprono per metà: 4769 px di lato (più di un frame del colpo), 3651 da dietro, 2319 in camera di gioco (dove è il cambio più grande della mossa dopo il colpo). Il frame prima vale 1234 / 768 / 130, quelli dopo 2795, 2377, 1908.
Rispetto a v8 (due frame da 4839 e 4547 di lato, 3212 e 2701 da dietro) è più corto e quattro frame prima, ma non più piccolo per frame: un giocatore lo nota, da dietro e di lato.
Non viene dalle chiavi: la clip arriva lenta, e a terra lo stesso blend-out di 8 frame è distribuito (atterraggio: picco 2589 di lato e 1306 da dietro su 7 frame, v8 4147 e 2399 in 2; a segno dopo l'atterraggio: picco 4246 su 4-5 frame, v8 5620 su 2-3). In aria il blend verso la posa di caduta non si comporta come un blend di 8 frame.

## 4. Carica (a8-a10) da dietro
DIETRO: nella griglia ravvicinata la palla è più raccolta (gomiti non più a ventaglio ai lati, testa più bassa): si legge. In camera di gioco la differenza con v8 è minima (cambio per frame 680-860 contro 610-1020). Non è un difetto: la carica passava già.
LATO: palla più chiusa e testa più bassa, si legge. Nessun passo indietro.

## 5. Caso a segno
Frame congelato: v91-v95 identici (0-1 px cambiati), posa pulita a15.0 con gambe tese e braccia conserte, in tutte e due le viste. **Sì.**
Ingresso (v90→v91, a15.7→a15.0) e uscita (v95→v96, a15.0→a15.9): sulla sola parte alta del corpo 1466 e 1112 px di lato, 1094 e 812 da dietro (v8 all'ingresso: 529 e 415). Guardata la differenza: è un contorno di 1-2 px su tutta la figura, non una posa che salta. **Nessun pop visibile**, né all'inizio né alla fine.
Alla ripartenza l'animazione salta un frame come in v8 (a15.9→a18.0) ed entra direttamente nel cedimento delle ginocchia: ora dopo l'hitstop si vede un rilascio. In questa cattura il doppio tick cade su v98, due frame dopo la ripartenza: è il passo 62,5/60, non la clip.
Osservazioni lato gioco: la posizione resta ferma un frame in più che in v8 (v90-v96); di lato resta lo spazio visibile tra piedi e testa del manichino; il manichino non reagisce.

## 6. Passi indietro rispetto a v8
Nessuno. Colpo: 3911 / 4483 / 4308 px di lato poi 528 (v8 4037 / 4636 / 4543 poi 779); da dietro 3328 / 4314 / 4268 poi 556 (v8 3155 / 4240 / 4203 poi 482). Il bordo basso scende di 52 e 27 px negli stessi due frame in v8 e v9: spacing identico.
Frame di hitstop uguale. Carica uguale o meglio. Sagoma di lato: barra inarcata intatta a a15-a17, poi ginocchio appena spezzato dal cedimento, che è voluto. In front34 ora un ginocchio è sfasato rispetto all'altro.

## 7. Atterraggio (solo osservazione)
Tocca terra verso a19 (f17). In v9 dopo il contatto il corpo continua a muoversi (951, 879, 609, 345 px per frame di lato; v8: 7, 355, 244, 71): il cedimento delle ginocchia legge come assorbimento, molto meglio del blocco rigido di v8. Restano 2 frame fermi a a24-a26, poi il recovery legge ancora come un accosciarsi lento.
Da dietro il bordo basso resta fermo entro 1-2 px dopo il contatto: non vedo piedi staccati dal pavimento. Di lato c'è un'oscillazione di 6-7 px che c'è anche in v8 (5 px, rumore della misura), quindi non la conto. L'uscita verso idle è morbida (vedi punto 3). Serve sempre un landing cancel, lato gioco.

## Cosa resta imperfetto da dietro e con quale effetto coprirlo
1. Pestone a15-a17: per i 3 frame attivi è ancora una colonna stretta, avambracci nascosti. Coprire con una scia verticale sotto i piedi da a13 a a15 e, a segno, un anello d'urto ai piedi con 2-3 frame di shake; l'hitstop di 5 frame fa già il resto. Nessuna posa da cambiare.
2. Contatto grigio su grigio (i piedi si fondono con la testa del bersaglio): scintilla e flash chiaro sul bersaglio nel frame di hitstop, più la reazione del bersaglio (lato gioco).
3. Rimbalzo a vuoto: circa 10 px in camera di gioco, senza bersaglio non ha una causa visibile. Un piccolo anello d'aria sotto i piedi a a15-a17 lo giustifica e lo ingrandisce.
4. Carica: lo stringimento è debole in miniatura. Un bagliore o particelle che convergono a a8-a10, e un flash di un frame alla partenza del colpo (a12).
5. Ginocchia a rombo a a18-a26: vanno bene; l'anello d'urto del punto 1 copre i primi frame.

## Correzioni (nessuna tocca le chiavi 0, 3, 13, 14, 15, 37, 42)
1. Lato kit / AnimBP, caso in aria, frame v69-v77 (f32-f40), gambe: capire perché l'uscita verso la posa di caduta scatta in un frame invece di distribuirsi su 8 (curva del blend-out del montage, IK delle gambe, stato di caduta sotto il montage). Obiettivo misurabile: nessun frame sopra 2400 px di lato (metà del colpo), come già succede a terra.
2. Ricatturare solo `dair_whiff` e rimisurare v69-v77 nelle tre viste. Non serve un altro giro sulle pose: i punti 1, 2, 4, 5 e 6 sono chiusi.
