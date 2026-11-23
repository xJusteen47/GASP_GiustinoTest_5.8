# Brief review: Overlord run start v4r4 (giro di posa 4, extra approvato dall'owner) vs v4r3 e v4r2

Mossa: partenza della corsa di Overlord (bruto, stile Ganondorf in Astro Bot). Clip AS_GSTN_RunStart_v4r4, base GASP pulita
(`gstn.Loco.Base 1`), riga pose-matched del chooser. Cattura in gioco: `ASSERT: OK mv_run_start_v4s4 AS_GSTN_RunStart_v4r4 f100-f180: weight >= 0.8 on 100% (min 0.94)`.

## Feedback dell'owner su v4r3
"Niente più compenetrazione, bene, ma i gomiti sono troppo alzati": riportare l'ALTEZZA dei gomiti al livello di v4r2 SENZA cambiare
la posizione delle mani né altro; le braccia non devono rientrare nel corpo. Perché: con i gomiti alti sembra uno zombie che non riesce
ad abbassare le braccia e muove solo gli avambracci. Anche l'omero deve oscillare.

## Cosa è cambiato in v4r4 (solo questo)
- Avambraccio e mano ruotano rigidamente attorno all'asse spalla->mano (il pugno sta sull'asse: posizione invariata, 0,001 cm) fino
  all'altezza del gomito che v4r2 aveva alla stessa fase della mano; rotazione max ~40 gradi, curva smussata, zero su stomp f0-11 e coda.
- Altezza gomito sopra v4r2 (punti del rig, f12-58): v4r3 media +2,7 cm, picco +10; v4r4 media -0,1, picco +1,9.
- Busto, gambe, spalle, tempi, root travel (998,7 cm) identici a v4r3. Distanza minima braccio/corpo +1,8 cm (come v4r3), braccio
  (omero) da pancia/cosce min +8,1 cm.
- Il pugno ruota con l'avambraccio attorno a quell'asse (polso invariato rispetto all'avambraccio): l'orientamento del pugno nel mondo
  cambia un po', la posizione no.

## Input (guarda solo questi)
- `grid_az0_a/b.png`, `grid_az90_a/b.png`, `grid_az45_a/b.png`, `grid_game_a/b.png`: tre righe, v4r2 / v4r3 / v4r4, frame v100-v136 (a) e
  v140-v176 (b), passo 4. v4r2 ha tempi diversi (cadenza cambiata in v4r3): confrontala per l'altezza dei gomiti, non frame per frame.
  v4r3 e v4r4 sono allineate frame per frame (stessa clip tranne i gomiti).
- `owner/runstart_v4r4.mp4` (60 fps, 6 viste + stato): se serve, estrai al massimo 8 frame con ffmpeg.

## Tratti d'identità (controllali PER PRIMI)
1. Stomp tenuto (carica e pestone iniziali).
2. Rev-up frenetico: primi passi corti e rapidi che poi si allungano.
3. Stile bruto: busto inclinato, pugni al mento, pompaggio ampio.
4. Nessuna compenetrazione braccia/corpo da nessuna vista.
5. NUOVO: gomiti all'altezza di v4r2, omero che oscilla con il braccio (niente "zombie" con soli avambracci).

## Come giudicare
Prima la vita a occhio (anticipazione, archi, peso, follow-through), poi i numeri. Controlla in particolare: gomiti abbassati come v4r2,
pugni nelle stesse posizioni di v4r3, nessuna compenetrazione nuova, nessun polso/avambraccio rotto o scatto di rotazione del pugno,
resto identico a v4r3. Frame congelati f109/f112 = bug di GIOCO noto, fuori ambito. Ogni voce di FAIL etichettata posa / dati montage /
gioco. Esito PASS o FAIL. Scrivi `review_fable_r5.md` in questa cartella, italiano, max 60 righe (criteri owner, tratti, vita, tecnico,
fix, limiti delle prove).
