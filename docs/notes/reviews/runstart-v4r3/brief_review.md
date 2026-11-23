# Brief review: Overlord run start v4r3 (giro di posa 3 di 3) vs v4r2

Mossa: partenza della corsa di Overlord (bruto, stile Ganondorf in Astro Bot). Clip AS_GSTN_RunStart_v4r3, base GASP pulita
(`gstn.Loco.Base 1`), riga del chooser pose-matched. Cattura in gioco: `ASSERT: OK mv_run_start_v4s3 AS_GSTN_RunStart_v4r3 f100-f180: weight >= 0.8 on 100% (min 0.94)`.

## Feedback dell'owner su v4r2 (gli piace l'effetto)
1. Le braccia compenetrano il corpo mentre pompano avanti e indietro. Va corretta la POSA (arco più largo, gomiti), non ridotto il movimento
   (regola 13e; un fix che riduce ampiezza, velocità o root motion oltre il 15 % è FAIL).
2. Primi 2-3 passi più rapidi (il motore che sale di giri).
3. Il busto "scatta" quando si rialza.

## Cosa è cambiato in v4r3
- Pugni: passano l'anca FUORI dalla coscia (+14 cm verso l'esterno in basso, rientrano solo nella metà alta dell'arco, al mento -9 cm
  invece di -12), gomiti più aperti. Arco, raggio e tempi del pompaggio invariati. Distanza minima avambraccio/pugno da petto, pancia,
  testa, cosce (capsule dai punti del rig, approssimate): v4r2 -10,1 cm (pugno nella coscia, 18 frame su 69 sotto zero); v4r3 +1,8 cm.
- Cadenza: primi 3 passi 5 frame a 30 fps (v4r2: 7, 6, 7), poi 7, 8, 9, 10 fino al loop; falcata accorciata in proporzione, quindi
  stessa velocità: root travel della clip 998,7 cm (v4r2 994,4), travel in gioco f80-200 794 cm (v4r2 785).
- Stomp f0-11 della clip invariato (scarto 0,001 cm), busto e uscita come v4r2.
- Scatto del busto (punto 3): la catena di cattura mostra frame CONGELATI (simulazione e animazione ferme, immagine identica alla
  precedente) a f109 e f112 in tutte le catture base 1, anche con la clip stock; in v4r3 solo f109. Cade proprio sulla risalita dopo lo
  stomp: è il "pop" visto dall'owner, voce GIOCO (coda codice), non posa. L'inclinazione del busto in gioco varia al massimo di
  1,7 gradi/frame² (v4r2 1,3).

## Input (guarda solo questi)
- `grid_az90.png`, `grid_az0.png`, `grid_game.png`: riga alta v4r2, riga bassa v4r3, stessi frame f84-f196 passo 4.
- `runstart_v4r3.mp4` (60 fps, 6 viste): se serve, estrai al massimo 8 frame con ffmpeg (attorno a f108-f114 e f130-f150).
- `metrics_summary.json`: peso della clip, piedi.

## Tratti d'identità (controllali PER PRIMI)
1. Stomp tenuto (carica e pestone iniziali come v4r2).
2. Rev-up frenetico: passi iniziali più corti e rapidi che poi si allungano.
3. Stile bruto fino all'uscita (busto inclinato, gomiti fuori, pugni al mento).
4. Nessuna compenetrazione braccia/corpo da nessuna vista.
5. Nessuno scatto del busto dovuto alla clip (i frame congelati f109/f112 sono gioco, etichettali così).

## Come giudicare
Prima la vita a occhio (anticipazione, archi, peso, follow-through, root coerente con la capsula), poi i numeri.
Ogni voce di FAIL etichettata posa / dati montage / gioco. Esito PASS o FAIL. Scrivi `review_fable_r4.md` in questa cartella, in
italiano, max 60 righe, con le stesse sezioni di una review normale (criteri owner, tratti, vita, tecnico, fix, limiti delle prove).
