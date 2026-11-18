# Riferimenti camera Astro Bot (owner 2026-11-18 13:55Z) e misura dalla prospettiva
Screenshot da video YouTube (1456x824 circa, fotogramma 16:9 intero, centro ottico assunto al centro): astro_ref_1 (giungla), astro_ref_2 (spiaggia rosa), astro_ref_3_voxel (livello 8 bit, griglia regolare), astro_ref_bridge (ponte di notte).

## Misura sul livello voxel (astro_ref_3_voxel.png)
Metodo: punti di fuga. Bordi del pavimento nella direzione di profondità (due bordi della piattaforma davanti, a mano; bordo del pavimento grande a destra; linee della griglia via Hough, `vp_floor.py`) danno l'orizzonte. Spigoli verticali dei blocchi (`vp_vertical.py`, solo 2-3 linee nitide) danno il punto di fuga verticale. Con roll zero: f² = (cy − y_orizzonte)(y_fugaVerticale − cy), pitch = atan((cy − y_orizzonte)/f).
- Punto di fuga di profondità: x ≈ 750 (quasi al centro: camera allineata al livello), y ≈ 90 ± 40 (coppie di bordi: 121, 68; Hough 76; una coppia quasi parallela dà −90, scartata).
- Punto di fuga verticale: y ≈ 1700 (2 linee; da 1300 a 2500 plausibile).
- **Risultato: pitch verso il basso ≈ 26° (20-32°), FOV orizzontale ≈ 95° (80-110°).** Margine largo: immagine sfocata, pochi spigoli verticali.
- Il personaggio sta nel terzo inferiore, la camera è vicina e la prospettiva è marcata: grandangolo, NON teleobiettivo. Le altre tre immagini a occhio sono coerenti (prospettiva forte, pitch 25-40°).

## Conseguenze per noi
- TelephotoFOV resta 0 (spento): Astro Bot non usa un FOV stretto; il "giocattolo" viene da materiali, luce, foschia e da una camera vicina e alta.
- Tilt-shift: sottilissimo o assente nei riferimenti; tenere i default leggeri (Fstop 4, sensore 120 mm) o Intensity 0,5.
- Distanza: personaggio piccolo nel terzo inferiore con camera vicina e FOV ampio; il nostro +400 cm va giudicato in gioco su questa inquadratura.

## Misura su astro_12 (piattaforma nel cielo, ritagliata, 1170x654) — la più affidabile
Comando: `python3 vp_ransac.py astro_12.png out.png` (fuga di profondità) e `python3 vp_vertical_edges.py astro_12.png 420,0,760,300 out.png` / `840,0,1040,260` (spigoli verticali).
- Fuga di profondità (12 linee del pavimento e dei bordi): (592, 152), deviazione bootstrap 1 px. Orizzonte y = 152, centro y = 327. x = 592 ≈ centro 585: camera allineata alla piattaforma.
- Spigoli verticali (porta: x 430 pendenza +0,064, x 745 −0,064, simmetrici attorno al centro; colonna: x 860 −0,111, x 998 −0,158): convergono in basso, fuga y ≈ 2600 ± 150.
- **Senza shift (centro ottico al centro): pitch ≈ 15,5° in giù, FOV orizzontale ≈ 86°.** Se ci fosse uno shift verticale di ±50 px (non misurabile qui, serve un riferimento metrico): pitch 13-18°, FOV 79-95°.
- Le verticali convergono quanto ci si aspetta da una camera inclinata di ~15°: nessun segno di raddrizzamento con lo shift.

## Sintesi (voxel + astro_12)
FOV orizzontale 80-95° (grandangolo), pitch 15-26° in giù secondo il livello, verticali che convergono (nessuno shift visibile). Tilt quasi assente.
