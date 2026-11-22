# Review: overlord jab v3 round 1 contro v3 r0 — Verdetto: jab 1 FAIL, jab 2 FAIL (niente va all'owner) — 2026-11-14
Regola 10, cloud. Frame estratti dai nove mp4 (1440x720, 7 viste da 360 px), guardati uno per uno attorno a primo frame, colpo, hitstop,
FAF e fine mossa, da DIETRO (az180 e camera di gioco) e di LATO (az270). Griglie mie in questa cartella: `rv_jab1_start_contact.jpg`,
`rv_jab2_side.jpg`, `rv_jab2_behind.jpg`, `rv_hit_contact.jpg`, `rv_end_squat_side.jpg`. Misura: pixel del personaggio cambiati tra due
frame video consecutivi (ritaglio 280x288 della vista, soglia 20, maschera mia: confrontare solo tra loro). `v` = frame video,
`m` = frame della timeline, `a` = frame del montage.
**Catena di cattura: valida.** Le clip nuove girano (pose diverse dalla base dal primo frame), tre sequenze, base e variante sugli stessi
frame: jab 1 parte a v21, jab 2 a v33, hitstop v28-29 e v42-44. Limite: nessuna sequenza agisce al FAF (vedi jab 2 punto 2).

## Bloccante comune: accosciata a fine mossa (solo variante, tutte e tre le sequenze)
- jab 1 da solo: v44 in piedi (a23.1), **v45 accosciata profonda in un frame** (2743 px di lato, più del frame del colpo che vale 2511),
  poi risale piano e torna in piedi solo verso v80: circa 35 frame, mezzo secondo. Base: 423 px a v45, resta in idle.
- catena a vuoto: scende da v80 a v84 (4 frame), idle solo verso v120. Catena a segno: uguale da v84. Base: niente.
- Parte nel frame in cui il montage inizia il blend-out (sparisce `a`: m0:25, m1:49), cioè 4 frame PRIMA della fine della timeline. Quindi
  la posa del gioco sotto il montage è già l'accosciata: il blend-out la scopre. È il difetto noto dopo le reazioni
  (`handovers/anim-finish-3.md`, "hunched crouch"), ora anche su un jab normale: lato Anim BP, non nelle chiavi.
- Tra base e variante sono cambiati: le clip, il blend-in (2 frame → 0) e il root motion di jab 2 (spento). Jab 1 non ha mai avuto root
  motion, quindi il colpevole è il blend-in a 0 oppure la posa della clip nuova. Non accertato: non ho aperto l'editor.

## Jab 1
PASSA:
- **Regola del primo frame.** v20 idle, v21 ginocchia piegate e spalle già ruotate: 1435 px di lato, 1516 da dietro (il frame del colpo
  vale 2511 e 1704). Poi 1991, 1835, 2091, 1835, 2164: nessun frame fermo (la base aveva 983, 2363 e poi 112 a v23).
- **Diagonale da dietro.** A v27-v33 braccio e mano escono a destra del corpo all'altezza della testa, in az180 e in camera di gioco.
  Nella base il braccio è nascosto dal busto per tutta la tenuta.
- Arrivo secco: 2511, 1726, poi 281 e 52 px. Frame di hitstop (v28-29, a6.0) pulito, braccio teso. Piedi a terra, personaggio fermo.
FALLISCE:
1. Accosciata a fine mossa (sopra). Bloccante.
2. **Il colpo non tocca il bersaglio a vista.** Nei frame di hitstop v28-29 da dietro la mano è a destra del manichino, che reagisce
   spostandosi a sinistra; di lato resta spazio tra mano e testa del manichino (nella base la mano arriva sul petto). 44 cm a destra e
   35 cm corti sono troppi per un bersaglio dritto davanti: da dietro sembra uno schiaffo di lato che manca.
3. Minore: di lato a v27-v30 il braccio sinistro resta teso in avanti all'altezza della pancia, sotto quello che colpisce: due braccia
   avanti nella stessa sagoma.

## Jab 2
PASSA:
- **Passo nella posa, capsula ferma.** Di lato il personaggio resta nello stesso punto della vista da v32 a v78, piedi sempre a terra; la
  base avanza e scende. Coerente con `numbers.md` (z 100.0, spostamento 0.0). Nessun effetto calamita: a v76-v78 il corpo è tornato.
- Carica leggibile da dietro: mano aperta alta sopra la testa da v37 a v41.
- Colpo in 2 frame (3199 e 3189 px a v40-41), arrivo a v42 (1916), poi 289 e 316: fermo netto. Hitstop v42-44 (a9.0): di lato la mano è
  sul petto alto del manichino, il colpo tocca.
FALLISCE:
1. **Regola del primo frame.** v33 (m1:0, a0.0) è uguale alla tenuta di jab 1: 36 px di lato, 49 da dietro (la base ne aveva 2306 e
   1652). Il cambio arriva a v34 (1436 / 1106) e **v35 è un frame morto** (0 e 4 px): `a` vale 2.1 sia a v34 sia a v35, in tutte e due le
   catture della variante (a segno: 2.0 e 2.0), mai nella base. L'avvio di jab 2 è: fermo, salto, fermo, via.
2. **Il corpo torna troppo tardi.** L'affondo resta tenuto fino a a40 (v72); il FAF è 26 (v59) e lì il bacino è ancora 28 cm davanti alla
   capsula. Chi agisce al FAF vede il corpo tornare indietro di 28 cm in un frame. Non catturato (nessuna sequenza agisce al FAF): lo
   deduco dai frame v58-v72 e dalla card (passo indietro alle chiavi f41-f46). L'owner ha chiesto che nel recupero il corpo torni abbastanza.
3. **Da dietro il colpo non si vede.** A v42-v44 (contatto e tenuta) il braccio che colpisce è tutto dietro il busto: in camera di gioco
   si vede solo la mano alta che sparisce. L'owner giudica soprattutto da dietro.
4. Accosciata a fine mossa (sopra). Bloccante.
5. Minori: a v41-v45 la gamba avanti entra nella gamba del manichino a 95 cm; ginocchio dietro a 29 gradi; dopo i frame attivi il braccio
   cade al fianco in 2 frame (v45-46, 1137 e 1158 px), poi 26 frame fermi.

## Correzioni
Lato gioco (prima di ogni altra posa, altrimenti ogni cattura finisce accosciata):
1. Due catture del solo `jab1`: (a) clip r1 con blend-in 2 frame, (b) clip r0 con blend-in 0. Se l'accosciata segue il blend-in, resta da
   scegliere tra regola del primo frame e blend-in: allora la cura è nell'Anim BP (scelta della posa dopo il montage), stessa voce della
   coda "ABP crouch/drift fix". Se segue la clip, confrontare la posa di fine clip r1 con l'idle del gioco.
2. Avvio di jab 2 in catena: `a` deve crescere a ogni frame video v33-v36. Capire perché il montage salta a 2.1 e poi sta fermo un frame
   (candidati: root motion spento, quindi il montage non è più agganciato alla simulazione come prima; oppure blend-in 0).
Pose jab 1:
3. Ridurre la diagonale a circa 25 gradi: mano non oltre 25 cm a destra del centro e almeno 75 cm avanti. Verifica nella cattura a segno
   da dietro: nel frame di hitstop la mano deve sovrapporsi alla sagoma del manichino. L'hitbox si sposta solo dopo, dove sta la mano.
4. Braccio sinistro al contatto: pugno al fianco o in guardia, non teso in avanti.
Pose jab 2:
5. f0 non può essere identico a jab 1 f12: far partire la clip dalla posa che oggi è f1 (o oltre). Obiettivo: v33 sopra 1000 px di lato.
6. Passo indietro finito entro f26: bacino entro 8 cm dall'idle al FAF. Aggiungere una sequenza di cattura che agisce al FAF.
7. Contatto f9-f11: mano 15-20 cm più a destra, fuori dalla spalla destra vista da dietro, senza perdere il petto del manichino di lato.
Le chiavi dell'owner non sono toccate da nessuna di queste. Decisione che resta all'owner: l'angolo di jab 1 (25 gradi oppure altro).
