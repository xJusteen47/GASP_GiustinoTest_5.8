# Braccia e mani nelle pose di colpo (nota di ricerca, thread hit-arms-1, 2026-11-18)

Ambito: pose di danno a terra (HitStand) e additive direzionali di colpo. Non riguarda le mosse d'attacco
(lì le mani seguono l'identità della mossa: jab a mano aperta, dropkick a pugni).

## Il problema visto dall'owner
Nella posa HitStand il braccio verso l'attaccante resta teso e rigido per tutto l'hitstun (anche con -25° della
decisione 3A): gomito dritto, polso dritto, dita in posa di riferimento. Si legge come un manichino spinto,
non come un personaggio che sente il colpo.

## Cosa fa il riferimento (griglia di Sakurai, 9 tipi Alto/Medio/Basso x S/M/L)
1. **Gomiti sempre piegati.** Nessun braccio è una linea dritta: anche nei colpi forti, dove le braccia volano
   via, il gomito tiene 20-60°. Il braccio dritto si legge come rigido o come un'azione volontaria (puntare).
2. **Mani aperte e divaricate.** Dita larghe e un po' piegate all'indietro, pollice fuori: è il segno grafico
   dello shock, leggibile anche in miniatura. Il pugno chiuso in un danno si legge come "pronto a reagire".
3. **Testa e spalle guidano.** La parte colpita si muove per prima (alto = testa e spalle, medio = busto che si
   piega, basso = bacino e gambe che cedono); le braccia arrivano dopo, in ritardo, come appendici trascinate.
4. **Asimmetria.** Le due braccia non fanno mai la stessa cosa: una lanciata in fuori/in alto, l'altra più
   raccolta. Le pose speculari (twinning) sembrano rigide.
5. **Polso rotto all'indietro.** Nel braccio lanciato la mano resta indietro rispetto all'avambraccio
   (polso esteso): è il drag che fa leggere la forza.
6. **La forza cambia l'ampiezza, non la forma.** S, M, L tengono la stessa idea con angoli più grandi e
   mani più tese su L; l'altezza (alto, medio, basso) cambia quale parte guida.

## Dal video di Sakurai sulle animazioni di danno (trascrizione passata dall'owner, 2026-11-18)
Testo completo nel messaggio dell'owner nella chat di progetto (18/11, 19:16 locali); qui solo i punti, riassunti.
I 6 fotogrammi allegati a quel messaggio non sono arrivati a questo thread.
- Il primo frame del danno cade nell'hitstop, quindi è il più visto: deve già leggersi come dolore, subito.
- Posa estrema e molto diversa dall'idle, leggibile anche da lontano.
- Breve blend dalla postura al momento dell'impatto al primo frame del danno (non un taglio secco al 100%).
- Rischio noto: spostare molto il corpo sposta il nodo radice e il bersaglio esce dalla combo del jab. Per noi: le
  braccia e le mani non toccano la radice; se una variante futura sposta il bacino, va controllata la combo.
- 9 tipi a terra: altezza (alto, medio, basso: reagisce la zona colpita) x intensità (lieve, media, forte). In aria
  molte varianti a parte.
- Evitare la simmetria, priorità assoluta alla silhouette; nei lanci il corpo si torce perché braccia e gambe si
  stacchino dallo sfondo.
- Se dura un istante, una deformazione estrema va bene.
Conseguenza per la variante: la piega di gomiti, polsi e mani è già piena dal frame 0 (nessuna rampa nella clip; il
raccordo lo fa il blend d'ingresso del sistema), e la mano più tesa sta proprio nei frame dell'hitstop.

## Cosa si vede nella griglia (fotogramma 2:21 del video, `ref_sakurai_damage_grid.png`)
- Gomiti molto piegati: 60-110° in quasi tutte le 9 pose, mai sotto i 45°. Le mani salgono spesso all'altezza di
  spalle e testa (alto S, medio M, basso L).
- Un braccio avanti e uno indietro, quasi sempre; le mani non sono mai alla stessa altezza.
- Mani: dita larghissime, nocche quasi dritte, punte un po' uncinate, pollice ben fuori; polso piegato.
- Spalle alte e incassate attorno alla testa; la testa si chiude verso la spalla dal lato del colpo.
- Da S a L cresce l'ampiezza (busto più piegato, braccia più lontane dal corpo), la forma resta.

## I 6 fotogrammi (`sakurai_1..6.png`, dal branch claude/ref-astrobot-vfx, a3bd7311)
- Falcon idle contro danno: in idle pugni chiusi davanti al petto; nel danno le due braccia si aprono larghe ai lati,
  gomiti a ~90°, mani all'altezza delle spalle con dita divaricate, testa girata. Il cambio di silhouette è totale.
- Lancio forte: corpo torto e capovolto, braccia e gambe lanciate lontano dal busto, mani aperte.
- Little Mac e R.O.B.: simmetria rotta di proposito, un arto in alto e uno in basso.
- Mario su Marth (prima e dopo): il bersaglio passa da una posa composta a un busto piegato con le braccia staccate.

## Regole pratiche per noi
- Valori finali (ArmsV3, dopo 3 giri Fable): vedi ARMS in `Tools/GAS/build_hit_arms_v2.py`. Punto di partenza era:
- Braccio guida (lato dell'attaccante): gomito +60° sulla posa, polso esteso ~28°, mano Splayed
  (SplayedTense nei primi 0,12 s). Braccio dietro: gomito +45°, polso ~18°, mano quasi tutta Splayed.
- Spalle: alzata di 12° sul lato guida e 7° sull'altro: le spalle "salgono" con lo spavento.
- Ritardo: braccio dietro 3 frame dopo il guida; molla smorzata (overshoot visibile, assestamento entro ~0,4 s);
  nei frame tenuti una deriva lenta di 2-3° così la posa non è congelata (era una voce della review armlow-1).
- Fine dell'hitstun: le mani tornano Relaxed nell'ultimo quinto, per non scattare quando riparte la locomozione.
- Archi delle braccia in FK (rotazioni sulle catene, mai IK sulla mano), come da principio del progetto.
- Interpolazione: slerp sui quaternioni per le dita e per le pose di mano (sono rotazioni pure). Le curve di
  tempo seguono lo sheet delle curve già usato dalle additive (scatto in 3 frame, molla con un overshoot).
- Vincolo gioco: se una mano attraversa il corpo o un hitbox, si sposta la posa del braccio, non si accorcia l'arco.

## Fuori da questo giro (proposte)
- Mappa dei 9 tipi: oggi abbiamo una sola posa base più 4 additive direzionali e un peso d'intensità. Alto/medio/basso
  richiede di sapere dove colpisce l'hitbox: è una feature nuova, quindi decisione dell'owner.
- Scatto delle dita dentro le additive direzionali (le mani si aprono di colpo all'impatto): stessa libreria, giro dopo.

Immagini: `docs/notes/reviews/ref-sakurai-damage/` (griglia qui; i 6 fotogrammi sul branch claude/ref-astrobot-vfx, a3bd7311).
Fonti: griglia di Sakurai passata dall'owner (video "Masahiro Sakurai on Creating Games", animazioni di danno);
principi standard di animazione (overlap, drag, twinning). Nessun numero qui è misurato: sono i valori di partenza
della variante, verificati poi in gioco.
