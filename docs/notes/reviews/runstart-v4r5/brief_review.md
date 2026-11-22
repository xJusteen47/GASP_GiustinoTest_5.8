# Brief review: Overlord run start v4r5 (senza passo piantato) + rampa di accelerazione, vs v4r4

Mossa: partenza della corsa di Overlord (bruto, stile Ganondorf in Astro Bot). Clip AS_GSTN_RunStart_v4r5, base GASP pulita
(`gstn.Loco.Base 1`), riga pose-matched del chooser. Cattura in gioco a 1 giocatore: `ASSERT: OK mv_run_start_v5 AS_GSTN_RunStart_v4r5
f95-f170: weight >= 0.8 on 93% (min 0.56)`.

## Richiesta dell'owner
Lo "stomp" che voleva era per il RUN STOP, non per il run start: togliere il passo piantato iniziale (~0,3 s) così la corsa parte subito
con il rev-up frenetico; tenere tutto il resto di v4r4 (gomiti, mani, braccia). Accelerazione graduale e continua (riferimento Toad:
"ci mette un po' a partire" = velocità che cresce, non fermo sul posto).

## Cosa è cambiato (solo questo)
- Clip: v4r4 f0-9 (alza il ginocchio, pestone, resta accucciato) tagliati; v4r5 parte dal carico accucciato dopo il pestone (v4r4 f10).
  Le pose da lì in poi sono quelle di v4r4 spostate di 10 frame (contatti spostati di 10 esatti). 59 frame a 30 fps.
- Curva Enable_Warping = 1 (come le start stock di GASP): stride/orientation warping attivo sulla clip.
- Dati: niente stamp di partenza (era 21 frame quasi fermo), rampa lineare di accelerazione dal primo frame (stage 1: 62 frame, stage 2:
  30). Velocità: 10% a 0,13 s, 50% a 0,45 s, 90% a 1,32 s dallo stick (v4r4: 0,37 / 0,52 / 1,30). Grafico `speed_v4r5.png`.
- Piedi piantati, spostamento f80-240 (euristico, `metrics_summary.json`): v4r4 L 36,6 R 73,8 cm; v4r5 L 49,5 R 43,8 cm.

## Input (guarda solo questi)
- `grid_az0.png`, `grid_az90.png`, `grid_game.png`: due righe, v4r4 (sopra) / v4r5 (sotto), STESSI frame di cattura v80-v176 passo 4
  (stick a v80). Le due clip NON sono allineate per posa: v4r5 è 10 frame di clip avanti.
- `runstart_v4r5.mp4` (60 fps): se serve, estrai al massimo 8 frame con ffmpeg.

## Tratti d'identità (controllali PER PRIMI)
1. Rev-up frenetico: primi passi corti e rapidi che poi si allungano (ora dal primo frame, senza pestone).
2. Stile bruto: busto inclinato, pugni al mento, pompaggio ampio.
3. Gomiti all'altezza approvata in v4r4, omero che oscilla; nessuna compenetrazione braccia/corpo.
(Il tratto "stomp tenuto" è RITIRATO dall'owner per il run start: non è un difetto se manca.)

## Come giudicare
Prima la vita a occhio (anticipazione: il carico iniziale basta senza pestone? peso, archi, follow-through), poi i numeri. Controlla in
particolare: l'ingresso da idle al primo frame (scatto o blend leggibile?), piedi che scivolano nei primi passi con la nuova rampa
(il warping dovrebbe coprire), passaggio al run loop. Un frame congelato isolato vicino a v99 è un bug di GIOCO noto (fuori ambito).
Ogni voce di FAIL etichettata posa / dati montage / gioco. Esito PASS o FAIL. Scrivi `review_fable_r6.md` in questa cartella, italiano,
max 60 righe (criteri owner, tratti, vita, tecnico, fix, limiti delle prove).
