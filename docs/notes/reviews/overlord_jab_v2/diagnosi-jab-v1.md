# Perché il jab Overlord di Antigravity (overlord_jab1 / overlord_jab2, 2026-11-08) non funziona

1. **Nessuna posa disegnata.** `overlord_jab1.json` non ha blocco `author`: il moto è una lista di delta di rotazione FK sommati a un take
   placeholder (`make_clean_source_take`), scritti alla cieca. L'owner l'ha visto subito: "sembra la vecchia jab pilot modificata in modo strano".
2. **Tempi illeggibili.** Carica a f2 e contatto a f4 per entrambi i colpi, jab 1 finito in 14 frame; l'affondo del jab 2 sposta il bacino di
   18 cm in 4 frame. Per un personaggio pesante non resta un frame in cui si capisce il tipo di colpo.
3. **Braccio sinistro incollato.** Lo stesso delta su tutti i key (f0-f13): non arriva dalla idle, non si muove col busto, e senza solve la
   mano non finisce sul torso ma sul fianco, dentro il corpo (screenshot dell'owner).
4. **Direzione del colpo sbagliata.** Lo slap si apre di lato rispetto al petto, non passa nello spazio davanti dove sta l'avversario.
5. **Un bug scambiato per riferimento.** Lo screenshot con cui l'owner segnalava "le animazioni sono entrambe rotte" è stato letto come
   "posa recepita al 100%" ed è diventato il bersaglio.
6. **Script per mossa invece della card.** `author_uefn_jabs.py`, `build_authentic_jabs.py`, `jab_v1..v6_build.py` riscrivono card e canali:
   card e asset non coincidono, niente è riproducibile.
7. **Tremore su tutto il corpo.** Jab 2: cinque pose intere consecutive (f16-f20) per far vibrare una mano, contro la regola dei key sparsi.
8. **Mai guardato in moto.** Solo render statici a due angoli in `L_PilotCapture`; nessun video, nessun PIE.
